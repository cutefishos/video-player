// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "application.h"
#include "durationprobe.h"
#include "playersettings.h"
#include "playlist.h"
#include "resumestore.h"
#include "sleepinhibitor.h"
#include "thumbnailer.h"

#include <QClipboard>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QWindow>

#include <cmath>

Application::Application(const QString &settingsFile, QObject *parent)
    : QObject(parent)
    , m_playlist(new Playlist(this))
    , m_settings(new PlayerSettings(settingsFile, this))
    , m_resume(new ResumeStore(settingsFile, this))
    , m_inhibitor(new SleepInhibitor(this))
    , m_thumbnailer(new Thumbnailer(this))
    , m_durationProbe(new DurationProbe(this))
{
    m_playlist->setRepeatMode(Playlist::RepeatMode(m_settings->repeatMode()));
    connect(m_playlist, &Playlist::repeatModeChanged, m_settings, [this] {
        m_settings->setRepeatMode(m_playlist->repeatMode());
    });
    connect(m_playlist, &Playlist::countChanged, m_durationProbe, [this] {
        m_durationProbe->probe(m_playlist->unprobed());
    });
    connect(m_durationProbe, &DurationProbe::found, m_playlist, &Playlist::setDuration);
}

QString Application::formatTime(double seconds, double reference)
{
    if (!std::isfinite(seconds) || seconds < 0)
        seconds = 0;
    const qint64 total = qint64(std::floor(seconds));
    const qint64 hours = total / 3600;
    const qint64 minutes = total / 60 % 60;
    const qint64 rest = total % 60;
    if (hours > 0 || reference >= 3600)
        return QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(rest, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(minutes).arg(rest, 2, 10, QLatin1Char('0'));
}

QString Application::formatBytes(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

QString Application::displayName(const QUrl &url)
{
    if (url.isLocalFile())
        return QFileInfo(url.toLocalFile()).completeBaseName();
    const QString name = QFileInfo(url.path()).completeBaseName();
    return name.isEmpty() ? url.toDisplayString() : name;
}

QUrl Application::urlFromInput(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(QLatin1Char('\n')))
        return {};
    const QUrl url = QUrl::fromUserInput(trimmed, QDir::currentPath());
    if (!url.isValid())
        return {};
    if (url.isLocalFile())
        return QFileInfo::exists(url.toLocalFile()) ? url : QUrl();
    // Schemes mpv streams from; a bare word would otherwise become http://word.
    static const QStringList schemes = {
        QStringLiteral("http"), QStringLiteral("https"), QStringLiteral("rtsp"), QStringLiteral("rtmp"),
        QStringLiteral("mms"), QStringLiteral("ftp"), QStringLiteral("smb"), QStringLiteral("sftp"),
    };
    if (!schemes.contains(url.scheme()) || url.host().isEmpty())
        return {};
    if (!trimmed.contains(QLatin1String("://")) && !url.host().contains(QLatin1Char('.')))
        return {};
    return url;
}

QString Application::clipboardUrl() const
{
    const QString text = QGuiApplication::clipboard()->text().trimmed();
    const QUrl url = urlFromInput(text);
    return url.isValid() && !url.isLocalFile() && text.contains(QLatin1String("://")) ? text : QString();
}

QString Application::screenshotPath(const QString &title) const
{
    QDir folder(m_settings->screenshotFolder());
    folder.mkpath(QStringLiteral("."));
    QString name = title;
    name.replace(QLatin1Char('/'), QLatin1Char('-'));
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh.mm.ss"));
    QString path = folder.filePath(QStringLiteral("%1 %2.png").arg(name, stamp));
    for (int i = 2; QFileInfo::exists(path); ++i)
        path = folder.filePath(QStringLiteral("%1 %2 (%3).png").arg(name, stamp).arg(i));
    return path;
}

void Application::showInFolder(const QUrl &url) const
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
                                                       QStringLiteral("/org/freedesktop/FileManager1"),
                                                       QStringLiteral("org.freedesktop.FileManager1"),
                                                       QStringLiteral("ShowItems"));
    call << QStringList{url.toString()} << QString();
    QDBusConnection::sessionBus().asyncCall(call);
}

bool Application::isSubtitle(const QUrl &url)
{
    return Playlist::isSubtitleFile(url.path());
}

void Application::openUrls(const QList<QUrl> &urls)
{
    QList<QUrl> media;
    QList<QUrl> subtitles;
    for (const QUrl &url : urls) {
        if (!url.isValid())
            continue;
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir()) {
            const QFileInfoList entries = QDir(url.toLocalFile()).entryInfoList(QDir::Files, QDir::Name);
            QList<QUrl> files;
            for (const QFileInfo &entry : entries) {
                if (Playlist::isMediaFile(entry.absoluteFilePath()))
                    files.append(QUrl::fromLocalFile(entry.absoluteFilePath()));
            }
            if (!files.isEmpty())
                media += Playlist::folderMedia(files.first().toLocalFile());
            continue;
        }
        (isSubtitle(url) ? subtitles : media).append(url);
    }
    if (!media.isEmpty() || !subtitles.isEmpty())
        Q_EMIT openRequested(media, subtitles);
}

void Application::open(const QStringList &arguments)
{
    QList<QUrl> urls;
    for (const QString &argument : arguments)
        urls.append(QUrl::fromUserInput(argument, QDir::currentPath(), QUrl::AssumeLocalFile));
    openUrls(urls);
    Q_EMIT activationRequested();
}

void Application::activateWindow()
{
    Q_EMIT activationRequested();
}

void Application::watchRatio(QWindow *window)
{
    window->installEventFilter(this);
}

bool Application::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::DevicePixelRatioChange)
        Q_EMIT windowRatioChanged(static_cast<QWindow *>(watched)->devicePixelRatio());
    return QObject::eventFilter(watched, event);
}
