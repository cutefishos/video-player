// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "playersettings.h"
#include "playlist.h"
#include "resumestore.h"
#include "sleepinhibitor.h"
#include "thumbnailer.h"

#include <QObject>
#include <QUrl>

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

public:
    explicit Application(const QString &settingsFile, QObject *parent = nullptr);

    Playlist *playlist() const { return m_playlist; }
    PlayerSettings *settings() const { return m_settings; }
    ResumeStore *resume() const { return m_resume; }
    SleepInhibitor *inhibitor() const { return m_inhibitor; }
    Thumbnailer *thumbnailer() const { return m_thumbnailer; }

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

private:
    Playlist *m_playlist;
    PlayerSettings *m_settings;
    ResumeStore *m_resume;
    SleepInhibitor *m_inhibitor;
    Thumbnailer *m_thumbnailer;
};
