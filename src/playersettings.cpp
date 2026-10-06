// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "playersettings.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

static const QString playerGroup = QStringLiteral("Player");
static const QString recentKey = QStringLiteral("Recent/Files");

PlayerSettings::PlayerSettings(const QString &file, QObject *parent)
    : QObject(parent)
    , m_file(file)
{
    QSettings settings(m_file, QSettings::IniFormat);
    settings.beginGroup(playerGroup);
    m_volume = qBound(0, settings.value(QStringLiteral("Volume"), 100).toInt(), 150);
    m_muted = settings.value(QStringLiteral("Muted"), false).toBool();
    m_resumePlayback = settings.value(QStringLiteral("ResumePlayback"), true).toBool();
    m_repeatMode = qBound(0, settings.value(QStringLiteral("RepeatMode"), 0).toInt(), 2);
    m_subtitleScale = qBound(50, settings.value(QStringLiteral("SubtitleScale"), 100).toInt(), 200);
    m_folderPlaylist = settings.value(QStringLiteral("FolderPlaylist"), true).toBool();
    m_audioLanguage = settings.value(QStringLiteral("AudioLanguage")).toString();
    m_subtitleLanguage = settings.value(QStringLiteral("SubtitleLanguage")).toString();
    m_screenshotFolder = settings.value(QStringLiteral("ScreenshotFolder"),
                                        QDir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
                                                .filePath(QStringLiteral("Screenshots"))).toString();
    m_lastFolder = settings.value(QStringLiteral("LastFolder")).toUrl();
    settings.endGroup();
    m_recent = settings.value(recentKey).toStringList();
}

void PlayerSettings::write(const QString &key, const QVariant &value)
{
    QSettings settings(m_file, QSettings::IniFormat);
    settings.setValue(key, value);
}

void PlayerSettings::setVolume(int volume)
{
    volume = qBound(0, volume, 150);
    if (m_volume == volume)
        return;
    m_volume = volume;
    write(playerGroup + QStringLiteral("/Volume"), volume);
    Q_EMIT volumeChanged();
}

void PlayerSettings::setMuted(bool muted)
{
    if (m_muted == muted)
        return;
    m_muted = muted;
    write(playerGroup + QStringLiteral("/Muted"), muted);
    Q_EMIT mutedChanged();
}

void PlayerSettings::setResumePlayback(bool resume)
{
    if (m_resumePlayback == resume)
        return;
    m_resumePlayback = resume;
    write(playerGroup + QStringLiteral("/ResumePlayback"), resume);
    Q_EMIT resumePlaybackChanged();
}

void PlayerSettings::setRepeatMode(int mode)
{
    mode = qBound(0, mode, 2);
    if (m_repeatMode == mode)
        return;
    m_repeatMode = mode;
    write(playerGroup + QStringLiteral("/RepeatMode"), mode);
    Q_EMIT repeatModeChanged();
}

void PlayerSettings::setSubtitleScale(int scale)
{
    scale = qBound(50, scale, 200);
    if (m_subtitleScale == scale)
        return;
    m_subtitleScale = scale;
    write(playerGroup + QStringLiteral("/SubtitleScale"), scale);
    Q_EMIT subtitleScaleChanged();
}

void PlayerSettings::setFolderPlaylist(bool enabled)
{
    if (m_folderPlaylist == enabled)
        return;
    m_folderPlaylist = enabled;
    write(playerGroup + QStringLiteral("/FolderPlaylist"), enabled);
    Q_EMIT folderPlaylistChanged();
}

void PlayerSettings::setAudioLanguage(const QString &language)
{
    if (m_audioLanguage == language)
        return;
    m_audioLanguage = language;
    write(playerGroup + QStringLiteral("/AudioLanguage"), language);
    Q_EMIT audioLanguageChanged();
}

void PlayerSettings::setSubtitleLanguage(const QString &language)
{
    if (m_subtitleLanguage == language)
        return;
    m_subtitleLanguage = language;
    write(playerGroup + QStringLiteral("/SubtitleLanguage"), language);
    Q_EMIT subtitleLanguageChanged();
}

QStringList PlayerSettings::languagePreference(const QString &language) const
{
    QStringList codes;
    auto add = [&codes](const QLocale::Language value) {
        if (value == QLocale::AnyLanguage || value == QLocale::C)
            return;
        // mpv matches both the two- and three-letter forms a file may carry.
        for (const QString &code : {QLocale::languageToCode(value, QLocale::ISO639Part1),
                                    QLocale::languageToCode(value, QLocale::ISO639Part2B),
                                    QLocale::languageToCode(value, QLocale::ISO639Part2T)}) {
            if (!code.isEmpty() && !codes.contains(code))
                codes.append(code);
        }
    };
    if (!language.isEmpty())
        add(QLocale::codeToLanguage(language, QLocale::AnyLanguageCode));
    add(QLocale().language());
    add(QLocale::English);
    return codes;
}

void PlayerSettings::setScreenshotFolder(const QString &folder)
{
    if (m_screenshotFolder == folder || folder.isEmpty())
        return;
    m_screenshotFolder = folder;
    write(playerGroup + QStringLiteral("/ScreenshotFolder"), folder);
    Q_EMIT screenshotFolderChanged();
}

void PlayerSettings::setLastFolder(const QUrl &folder)
{
    if (m_lastFolder == folder)
        return;
    m_lastFolder = folder;
    write(playerGroup + QStringLiteral("/LastFolder"), folder);
    Q_EMIT lastFolderChanged();
}

QVariantList PlayerSettings::recentFiles() const
{
    QVariantList files;
    for (const QString &entry : m_recent) {
        const QUrl url(entry);
        if (url.isLocalFile() && !QFileInfo::exists(url.toLocalFile()))
            continue;
        QVariantMap file;
        file.insert(QStringLiteral("url"), url);
        file.insert(QStringLiteral("name"), url.isLocalFile() ? QFileInfo(url.toLocalFile()).fileName()
                                                              : url.toDisplayString());
        file.insert(QStringLiteral("folder"), url.isLocalFile() ? QFileInfo(url.toLocalFile()).absolutePath()
                                                                : url.host());
        files.append(file);
    }
    return files;
}

void PlayerSettings::addRecentFile(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty())
        return;
    const QString entry = url.toString(QUrl::FullyEncoded);
    if (!m_recent.isEmpty() && m_recent.first() == entry)
        return;
    m_recent.removeAll(entry);
    m_recent.prepend(entry);
    while (m_recent.size() > RecentCapacity)
        m_recent.removeLast();
    write(recentKey, m_recent);
    Q_EMIT recentFilesChanged();
}

void PlayerSettings::removeRecentFile(const QUrl &url)
{
    if (m_recent.removeAll(url.toString(QUrl::FullyEncoded)) == 0)
        return;
    write(recentKey, m_recent);
    Q_EMIT recentFilesChanged();
}

void PlayerSettings::clearRecentFiles()
{
    if (m_recent.isEmpty())
        return;
    m_recent.clear();
    write(recentKey, m_recent);
    Q_EMIT recentFilesChanged();
}
