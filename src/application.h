// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "durationprobe.h"
#include "playersettings.h"
#include "playlist.h"
#include "resumestore.h"
#include "sleepinhibitor.h"
#include "thumbnailer.h"

#include <QObject>
#include <QSize>
#include <QUrl>

class QWindow;

// What the window works with, and the D-Bus face of the running player:
// a second launch hands its files over through open().
class Application : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.VideoPlayer")
    Q_PROPERTY(Playlist *playlist READ playlist CONSTANT)
    Q_PROPERTY(PlayerSettings *settings READ settings CONSTANT)
    Q_PROPERTY(ResumeStore *resume READ resume CONSTANT)
    Q_PROPERTY(SleepInhibitor *inhibitor READ inhibitor CONSTANT)
    Q_PROPERTY(Thumbnailer *thumbnailer READ thumbnailer CONSTANT)
    // The picture size of the file on the command line, known before the
    // window first shows so that it opens already fitted to it.
    Q_PROPERTY(QSize launchVideoSize READ launchVideoSize CONSTANT)

public:
    explicit Application(const QString &settingsFile, QObject *parent = nullptr);

    Playlist *playlist() const { return m_playlist; }
    PlayerSettings *settings() const { return m_settings; }
    ResumeStore *resume() const { return m_resume; }
    SleepInhibitor *inhibitor() const { return m_inhibitor; }
    Thumbnailer *thumbnailer() const { return m_thumbnailer; }
    QSize launchVideoSize() const { return m_launchVideoSize; }
    void setLaunchVideoSize(const QSize &size) { m_launchVideoSize = size; }
    // Reports window's device pixel ratio as soon as it changes: the
    // fractional scale arrives just ahead of the first frame, and QML hears
    // of it only after that frame.
    void watchRatio(QWindow *window);

    // 1:05 or 1:02:05; hours are shown whenever reference reaches an hour,
    // so elapsed and total times keep the same shape.
    Q_INVOKABLE static QString formatTime(double seconds, double reference = 0);
    Q_INVOKABLE static QString formatBytes(qint64 bytes);
    // The name to show for a file: its title without the extension.
    Q_INVOKABLE static QString displayName(const QUrl &url);
    // A URL typed or pasted by the user, or an empty one if it is not one.
    Q_INVOKABLE static QUrl urlFromInput(const QString &text);
    // The clipboard's text when it is a URL to play.
    Q_INVOKABLE QString clipboardUrl() const;
    // Where a screenshot of title taken now is saved; the folder is created.
    Q_INVOKABLE QString screenshotPath(const QString &title) const;
    Q_INVOKABLE void showInFolder(const QUrl &url) const;
    Q_INVOKABLE static bool isSubtitle(const QUrl &url);

    // Splits urls into media to play and subtitles to add, and asks the
    // window to open them.
    Q_INVOKABLE void openUrls(const QList<QUrl> &urls);

public Q_SLOTS:
    // D-Bus: files and URLs from another launch, as given on its command line.
    void open(const QStringList &arguments);
    void activateWindow();

Q_SIGNALS:
    void openRequested(const QList<QUrl> &media, const QList<QUrl> &subtitles);
    void activationRequested();
    void windowRatioChanged(qreal ratio);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Playlist *m_playlist;
    PlayerSettings *m_settings;
    ResumeStore *m_resume;
    SleepInhibitor *m_inhibitor;
    Thumbnailer *m_thumbnailer;
    DurationProbe *m_durationProbe;
    QSize m_launchVideoSize;
};
