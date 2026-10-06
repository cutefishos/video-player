// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QUrl>
#include <QWaitCondition>

class QThread;

// Seek-bar previews: frames of a local file decoded by a second, silent mpv
// on a thread of its own, so the player itself never seeks. Frames are taken
// at keyframes in steps of about 1/150 of the length, and kept per file.
class Thumbnailer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(double duration READ duration WRITE setDuration NOTIFY durationChanged)
    // Bumped whenever a frame is added, for bindings to re-check has().
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit Thumbnailer(QObject *parent = nullptr);
    ~Thumbnailer() override;

    QUrl source() const { return m_source; }
    void setSource(const QUrl &source);
    double duration() const { return m_duration; }
    void setDuration(double duration);
    int revision() const { return m_revision; }

    // The step a time falls in, and the time its frame shows.
    Q_INVOKABLE int bucket(double seconds) const;
    Q_INVOKABLE double bucketTime(int bucket) const;
    Q_INVOKABLE bool has(int bucket) const;
    // Asks for the frame of bucket; only the latest request is worked on.
    Q_INVOKABLE void request(int bucket);

    // Called from the image provider's thread.
    QImage image(int bucket) const;

Q_SIGNALS:
    void sourceChanged();
    void durationChanged();
    void revisionChanged();

private:
    void run();
    void addFrame(int bucket, const QImage &image, quint64 generation, double step);
    // Whether the worker should drop what it is doing: quitting or a new file.
    bool cancelled(quint64 generation) const;
    double step() const;

    QUrl m_source;
    double m_duration = 0;
    int m_revision = 0;

    QThread *m_thread = nullptr;
    mutable QMutex m_mutex;
    QWaitCondition m_wake;
    // Guarded by m_mutex.
    bool m_stopping = false;
    QString m_file;
    quint64 m_generation = 0;
    double m_step = 1;
    int m_pending = -1;
    QHash<int, QImage> m_frames;
    // The worker's mpv, woken to cut a wait short.
    struct mpv_handle *m_activeMpv = nullptr;
};

class ThumbnailProvider : public QQuickImageProvider
{
public:
    explicit ThumbnailProvider(Thumbnailer *thumbnailer);
    // id: "<bucket>/<revision>", the revision only to defeat Qt's cache.
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    Thumbnailer *m_thumbnailer;
};
