// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thumbnailer.h"

#include <QElapsedTimer>
#include <QThread>

#include <mpv/client.h>
#include <mpv/render.h>

#include <cmath>

static constexpr int frameWidth = 240;
static constexpr int bucketCount = 150;

// Waits up to timeout milliseconds for event; false on an error, the end of
// the file, or when cancelled says so.
template<typename Cancelled>
static bool waitFor(mpv_handle *mpv, mpv_event_id wanted, int timeout, Cancelled cancelled)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout && !cancelled()) {
        mpv_event *event = mpv_wait_event(mpv, (timeout - timer.elapsed()) / 1000.0);
        if (event->event_id == wanted)
            return true;
        if (event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN)
            return false;
    }
    return false;
}

static void setOption(mpv_handle *mpv, const char *name, const char *value)
{
    mpv_set_option_string(mpv, name, value);
}

Thumbnailer::Thumbnailer(QObject *parent)
    : QObject(parent)
{
    m_thread = QThread::create([this] { run(); });
    m_thread->setObjectName(QStringLiteral("Thumbnailer"));
}

Thumbnailer::~Thumbnailer()
{
    {
        QMutexLocker locker(&m_mutex);
        m_stopping = true;
        if (m_activeMpv)
            mpv_wakeup(m_activeMpv);
        m_wake.wakeAll();
    }
    m_thread->wait();
    delete m_thread;
}

void Thumbnailer::setSource(const QUrl &source)
{
    if (m_source == source)
        return;
    m_source = source;
    {
        QMutexLocker locker(&m_mutex);
        // Streams would download a second copy; only local files get frames.
        m_file = source.isLocalFile() ? source.toLocalFile() : QString();
        ++m_generation;
        m_pending = -1;
        m_frames.clear();
        if (m_activeMpv)
            mpv_wakeup(m_activeMpv);
        m_wake.wakeAll();
    }
    // The thread starts with the first file that can have frames.
    if (!m_thread->isRunning() && source.isLocalFile())
        m_thread->start(QThread::LowPriority);
    Q_EMIT sourceChanged();
    ++m_revision;
    Q_EMIT revisionChanged();
}

void Thumbnailer::setDuration(double duration)
{
    if (qFuzzyCompare(m_duration, duration))
        return;
    m_duration = duration;
    {
        QMutexLocker locker(&m_mutex);
        m_step = step();
        m_frames.clear();
    }
    Q_EMIT durationChanged();
}

double Thumbnailer::step() const
{
    return std::max(1.0, m_duration / bucketCount);
}

int Thumbnailer::bucket(double seconds) const
{
    return qBound(0, int(std::floor(seconds / step())), int(m_duration / step()));
}

double Thumbnailer::bucketTime(int bucket) const
{
    return bucket * step();
}

bool Thumbnailer::has(int bucket) const
{
    QMutexLocker locker(&m_mutex);
    return m_frames.contains(bucket);
}

void Thumbnailer::request(int bucket)
{
    if (m_duration <= 0 || bucket < 0)
        return;
    QMutexLocker locker(&m_mutex);
    if (m_file.isEmpty() || m_frames.contains(bucket) || m_pending == bucket)
        return;
    m_pending = bucket;
    m_wake.wakeAll();
}

QImage Thumbnailer::image(int bucket) const
{
    QMutexLocker locker(&m_mutex);
    return m_frames.value(bucket);
}

bool Thumbnailer::cancelled(quint64 generation) const
{
    QMutexLocker locker(&m_mutex);
    return m_stopping || generation != m_generation;
}

void Thumbnailer::addFrame(int bucket, const QImage &image, quint64 generation, double step)
{
    {
        QMutexLocker locker(&m_mutex);
        // A frame taken for another file or another length is not this bucket's.
        if (generation != m_generation || step != m_step)
            return;
        m_frames.insert(bucket, image);
    }
    ++m_revision;
    Q_EMIT revisionChanged();
}

void Thumbnailer::run()
{
    mpv_handle *mpv = nullptr;
    mpv_render_context *context = nullptr;
    quint64 loadedGeneration = 0;
    QSize frameSize;

    quint64 generation = 0;
    auto isCancelled = [&] { return cancelled(generation); };

    auto close = [&] {
        {
            QMutexLocker locker(&m_mutex);
            m_activeMpv = nullptr;
        }
        if (context)
            mpv_render_context_free(context);
        context = nullptr;
        if (mpv)
            mpv_terminate_destroy(mpv);
        mpv = nullptr;
        frameSize = QSize();
    };

    auto open = [&](const QString &file) {
        mpv = mpv_create();
        if (!mpv)
            return;
        {
            QMutexLocker locker(&m_mutex);
            m_activeMpv = mpv;
        }
        setOption(mpv, "vo", "libmpv");
        setOption(mpv, "ao", "null");
        setOption(mpv, "aid", "no");
        setOption(mpv, "sid", "no");
        setOption(mpv, "hwdec", "no");
        setOption(mpv, "pause", "yes");
        setOption(mpv, "keep-open", "always");
        setOption(mpv, "hr-seek", "no");
        setOption(mpv, "cache", "no");
        setOption(mpv, "osd-level", "0");
        setOption(mpv, "load-scripts", "no");
        setOption(mpv, "ytdl", "no");
        setOption(mpv, "vd-lavc-skiploopfilter", "all");
        setOption(mpv, "vd-lavc-fast", "yes");
        if (mpv_initialize(mpv) < 0)
            return close();

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_SW)},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        if (mpv_render_context_create(&context, mpv, params) < 0) {
            context = nullptr;
            return close();
        }

        const QByteArray path = file.toUtf8();
        const char *command[] = {"loadfile", path.constData(), nullptr};
        // The size is known once the first frame has configured the output.
        if (mpv_command(mpv, command) < 0 || !waitFor(mpv, MPV_EVENT_FILE_LOADED, 5000, isCancelled)
                || !waitFor(mpv, MPV_EVENT_VIDEO_RECONFIG, 5000, isCancelled))
            return close();
        int64_t width = 0;
        int64_t height = 0;
        mpv_get_property(mpv, "dwidth", MPV_FORMAT_INT64, &width);
        mpv_get_property(mpv, "dheight", MPV_FORMAT_INT64, &height);
        if (width <= 0 || height <= 0)
            return close();
        frameSize = QSize(frameWidth, qMax<int>(1, qRound(double(frameWidth) * height / width)));
    };

    Q_FOREVER {
        QString file;
        int bucket = -1;
        double step = 1;
        {
            QMutexLocker locker(&m_mutex);
            while (!m_stopping && m_pending < 0 && loadedGeneration == m_generation)
                m_wake.wait(&m_mutex);
            if (m_stopping)
                break;
            file = m_file;
            generation = m_generation;
            bucket = m_pending;
            step = m_step;
            m_pending = -1;
        }

        if (generation != loadedGeneration) {
            close();
            loadedGeneration = generation;
            if (!file.isEmpty())
                open(file);
        }
        if (bucket < 0 || !context || frameSize.isEmpty())
            continue;

        const QByteArray target = QByteArray::number(bucket * step, 'f', 3);
        const char *seek[] = {"seek", target.constData(), "absolute+keyframes", nullptr};
        if (mpv_command(mpv, seek) < 0 || !waitFor(mpv, MPV_EVENT_PLAYBACK_RESTART, 3000, isCancelled))
            continue;

        // The decoded frame reaches the render context shortly after the seek.
        QElapsedTimer timer;
        timer.start();
        while (!(mpv_render_context_update(context) & MPV_RENDER_UPDATE_FRAME) && timer.elapsed() < 500
               && !isCancelled())
            QThread::msleep(4);

        QImage image(frameSize, QImage::Format_RGBX8888);
        int size[2] = {frameSize.width(), frameSize.height()};
        size_t stride = image.bytesPerLine();
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_SW_SIZE, size},
            {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("rgb0")},
            {MPV_RENDER_PARAM_SW_STRIDE, &stride},
            {MPV_RENDER_PARAM_SW_POINTER, image.bits()},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        if (mpv_render_context_render(context, params) < 0)
            continue;
        QMetaObject::invokeMethod(this, [this, bucket, image, generation, step] {
            addFrame(bucket, image, generation, step);
        }, Qt::QueuedConnection);
    }
    close();
}

ThumbnailProvider::ThumbnailProvider(Thumbnailer *thumbnailer)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_thumbnailer(thumbnailer)
{
}

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &)
{
    const QImage image = m_thumbnailer->image(id.section(QLatin1Char('/'), 0, 0).toInt());
    if (size)
        *size = image.size();
    return image;
}
