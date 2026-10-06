// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>

// The player's preferences and recent files in video-player.conf.
class PlayerSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool resumePlayback READ resumePlayback WRITE setResumePlayback NOTIFY resumePlaybackChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(int subtitleScale READ subtitleScale WRITE setSubtitleScale NOTIFY subtitleScaleChanged)
    Q_PROPERTY(bool folderPlaylist READ folderPlaylist WRITE setFolderPlaylist NOTIFY folderPlaylistChanged)
    Q_PROPERTY(QString audioLanguage READ audioLanguage WRITE setAudioLanguage NOTIFY audioLanguageChanged)
    Q_PROPERTY(QString subtitleLanguage READ subtitleLanguage WRITE setSubtitleLanguage NOTIFY subtitleLanguageChanged)
    Q_PROPERTY(QString screenshotFolder READ screenshotFolder WRITE setScreenshotFolder NOTIFY screenshotFolderChanged)
    Q_PROPERTY(QUrl lastFolder READ lastFolder WRITE setLastFolder NOTIFY lastFolderChanged)
    Q_PROPERTY(QVariantList recentFiles READ recentFiles NOTIFY recentFilesChanged)

public:
    static constexpr int RecentCapacity = 10;

    explicit PlayerSettings(const QString &file, QObject *parent = nullptr);

    int volume() const { return m_volume; }
    void setVolume(int volume);
    bool muted() const { return m_muted; }
    void setMuted(bool muted);
    bool resumePlayback() const { return m_resumePlayback; }
    void setResumePlayback(bool resume);
    // A Playlist::RepeatMode.
    int repeatMode() const { return m_repeatMode; }
    void setRepeatMode(int mode);
    // Subtitle size in percent of the default.
    int subtitleScale() const { return m_subtitleScale; }
    void setSubtitleScale(int scale);
    // Opening one file queues the other media files of its folder.
    bool folderPlaylist() const { return m_folderPlaylist; }
    void setFolderPlaylist(bool enabled);
    // ISO 639 codes, or empty for the display language.
    QString audioLanguage() const { return m_audioLanguage; }
    void setAudioLanguage(const QString &language);
    QString subtitleLanguage() const { return m_subtitleLanguage; }
    void setSubtitleLanguage(const QString &language);
    // The track languages to ask mpv for, the chosen one first, then the display language and English.
    Q_INVOKABLE QStringList languagePreference(const QString &language) const;
    QString screenshotFolder() const { return m_screenshotFolder; }
    void setScreenshotFolder(const QString &folder);
    QUrl lastFolder() const { return m_lastFolder; }
    void setLastFolder(const QUrl &folder);
    // Most recent first; entries that no longer exist on disk are left out.
    QVariantList recentFiles() const;

    Q_INVOKABLE void addRecentFile(const QUrl &url);
    Q_INVOKABLE void removeRecentFile(const QUrl &url);
    Q_INVOKABLE void clearRecentFiles();

Q_SIGNALS:
    void volumeChanged();
    void mutedChanged();
    void resumePlaybackChanged();
    void repeatModeChanged();
    void subtitleScaleChanged();
    void folderPlaylistChanged();
    void audioLanguageChanged();
    void subtitleLanguageChanged();
    void screenshotFolderChanged();
    void lastFolderChanged();
    void recentFilesChanged();

private:
    void write(const QString &key, const QVariant &value);

    QString m_file;
    int m_volume = 100;
    bool m_muted = false;
    bool m_resumePlayback = true;
    int m_repeatMode = 0;
    int m_subtitleScale = 100;
    bool m_folderPlaylist = true;
    QString m_audioLanguage;
    QString m_subtitleLanguage;
    QString m_screenshotFolder;
    QUrl m_lastFolder;
    QStringList m_recent;
};
