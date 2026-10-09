// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QUrl>

// The files queued for playback. Opening one local file queues the other
// media files of its folder around it, in the order a file manager lists them.
class Playlist : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentIndexChanged)
    Q_PROPERTY(RepeatMode repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY navigationChanged)
    Q_PROPERTY(bool hasPrevious READ hasPrevious NOTIFY navigationChanged)

public:
    enum RepeatMode {
        RepeatOff,
        RepeatOne,
        RepeatAll,
    };
    Q_ENUM(RepeatMode)

    enum Roles {
        UrlRole = Qt::UserRole + 1,
        TitleRole,
        CurrentRole,
        // Seconds, 0 while unknown.
        DurationRole,
    };

    explicit Playlist(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_urls.size(); }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);
    QUrl currentUrl() const;
    RepeatMode repeatMode() const { return m_repeatMode; }
    void setRepeatMode(RepeatMode mode);
    bool hasNext() const { return nextIndex(false) >= 0; }
    bool hasPrevious() const { return previousIndex() >= 0; }

    QList<QUrl> urls() const { return m_urls; }
    // The queued files whose length has not been read yet.
    QList<QUrl> unprobed() const;
    // Kept per URL across queues, so a file is read once.
    Q_INVOKABLE void setDuration(const QUrl &url, double seconds);

    // Replaces the queue and makes the first of urls current. With
    // includeFolder, a single local file brings its folder's media files along.
    Q_INVOKABLE void open(const QList<QUrl> &urls, bool includeFolder = true);
    // Adds urls that are not queued yet to the end.
    Q_INVOKABLE void append(const QList<QUrl> &urls);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void clear();

    // What follows the current file: automatic says the current one ended by
    // itself, so Repeat One plays it again and Repeat Off stops at the end.
    Q_INVOKABLE int nextIndex(bool automatic = false) const;
    Q_INVOKABLE int previousIndex() const;
    Q_INVOKABLE bool next(bool automatic = false);
    Q_INVOKABLE bool previous();

    static bool isMediaFile(const QString &fileName);
    static bool isSubtitleFile(const QString &fileName);
    // The media files in the folder of file, file included, in natural order.
    static QList<QUrl> folderMedia(const QString &file);
    static QString titleFor(const QUrl &url);

Q_SIGNALS:
    void countChanged();
    void currentIndexChanged();
    void repeatModeChanged();
    void navigationChanged();

private:
    QList<QUrl> m_urls;
    QHash<QUrl, double> m_durations;
    int m_current = -1;
    RepeatMode m_repeatMode = RepeatOff;
};
