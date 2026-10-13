// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thumbnailer.h"
#include "mpvwait.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>

#include <mpv/client.h>
#include <mpv/render.h>

#include <clocale>
#include <cmath>

static constexpr int frameWidth = 240;
static constexpr int bucketCount = 150;

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
    setCover({});
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
        m_coverWanted = false;
        m_frames.clear();
        if (m_activeMpv)
            mpv_wakeup(m_activeMpv);
        m_wake.wakeAll();
    }
    // The thread starts with the first file that can have frames.
    if (!m_thread->isRunning() && source.isLocalFile()) {
        // mpv needs the C locale for numbers.
        std::setlocale(LC_NUMERIC, "C");
        m_thread->start(QThread::LowPriority);
    }
    m_coverAsked = false;
    setCover({});
    askCover();
    Q_EMIT sourceChanged();
    ++m_revision;
    Q_EMIT revisionChanged();
}

void Thumbnailer::setDuration(double duration)
{
    if (qFuzzyCompare(m_duration, duration))
        return;
    {
        QMutexLocker locker(&m_mutex);
        m_duration = duration;
        m_step = step();
        m_frames.clear();
    }
    askCover();
    Q_EMIT durationChanged();
}

void Thumbnailer::askCover()
{
    QMutexLocker locker(&m_mutex);
    if (m_coverAsked || m_duration <= 0 || m_file.isEmpty())
        return;
    m_coverAsked = true;
    m_coverWanted = true;
    m_wake.wakeAll();
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

void Thumbnailer::addCover(const QImage &image, quint64 generation)
{
    {
        QMutexLocker locker(&m_mutex);
        if (generation != m_generation)
            return;
    }

    QDir folder(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/covers"));
    // Covers left behind by a player that did not quit cleanly.
    if (!m_staleCoversRemoved) {
        m_staleCoversRemoved = true;
        folder.removeRecursively();
    }
    if (!folder.mkpath(QStringLiteral(".")))
        return;

    const int side = qMin(image.width(), image.height());
    const QImage square = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side);
    // A name never used before, also by an earlier player, as media controls cache images by URL.
    const QString path = folder.filePath(QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".png"));
    if (square.save(path))
        setCover(QUrl::fromLocalFile(path));
}

void Thumbnailer::setCover(const QUrl &cover)
{
    if (m_cover == cover)
        return;
    if (m_cover.isLocalFile())
        QFile::remove(m_cover.toLocalFile());
    m_cover = cover;
    Q_EMIT coverChanged();
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

    auto render = [&](QImage *image) {
        int size[2] = {frameSize.width(), frameSize.height()};
        size_t stride = image->bytesPerLine();
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_SW_SIZE, size},
            {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("rgb0")},
            {MPV_RENDER_PARAM_SW_STRIDE, &stride},
            {MPV_RENDER_PARAM_SW_POINTER, image->bits()},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        return mpv_render_context_render(context, params) >= 0;
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
        // The size is known once the first frame has configured the output. The
        // load's own restart follows; left queued, it would end the first seek.
        if (mpv_command(mpv, command) < 0 || !waitForMpvEvent(mpv, MPV_EVENT_FILE_LOADED, 5000, isCancelled)
                || !waitForMpvEvent(mpv, MPV_EVENT_VIDEO_RECONFIG, 5000, isCancelled)
                || !waitForMpvEvent(mpv, MPV_EVENT_PLAYBACK_RESTART, 5000, isCancelled))
            return close();
        int64_t width = 0;
        int64_t height = 0;
        mpv_get_property(mpv, "dwidth", MPV_FORMAT_INT64, &width);
        mpv_get_property(mpv, "dheight", MPV_FORMAT_INT64, &height);
        if (width <= 0 || height <= 0)
            return close();
        frameSize = QSize(frameWidth, qMax<int>(1, qRound(double(frameWidth) * height / width)));

    };

    // The keyframe at or before seconds, at frameSize; null on failure.
    auto grab = [&](double seconds) -> QImage {
        const QByteArray target = QByteArray::number(seconds, 'f', 3);
        const char *seek[] = {"seek", target.constData(), "absolute+keyframes", nullptr};
        if (mpv_command(mpv, seek) < 0)
            return {};

        // mpv redraws the old frame while it seeks and hands over the new one
        // just before the restart, so the last frame drawn by then is the one.
        QImage image(frameSize, QImage::Format_RGBX8888);
        bool drawn = false;
        bool restarted = false;
        QElapsedTimer timer;
        timer.start();
        while (!restarted) {
            if (timer.elapsed() > 3000 || isCancelled())
                return {};
            const mpv_event_id event = mpv_wait_event(mpv, 0.004)->event_id;
            if (event == MPV_EVENT_END_FILE || event == MPV_EVENT_SHUTDOWN)
                return {};
            restarted = event == MPV_EVENT_PLAYBACK_RESTART;
            if (mpv_render_context_update(context) & MPV_RENDER_UPDATE_FRAME)
                drawn = render(&image);
        }
        return drawn ? image : QImage();
    };

    Q_FOREVER {
        QString file;
        int bucket = -1;
        double step = 1;
        bool cover = false;
        {
            QMutexLocker locker(&m_mutex);
            while (!m_stopping && m_pending < 0 && !m_coverWanted && loadedGeneration == m_generation)
                m_wake.wait(&m_mutex);
            if (m_stopping)
                break;
            file = m_file;
            generation = m_generation;
            bucket = m_pending;
            step = m_step;
            m_pending = -1;
            cover = m_coverWanted;
            m_coverWanted = false;
        }

        if (generation != loadedGeneration) {
            close();
            loadedGeneration = generation;
            if (!file.isEmpty())
                open(file);
        }
        if (!context || frameSize.isEmpty())
            continue;

        if (cover) {
            double duration = 0;
            {
                // The length may have arrived after the file did.
                QMutexLocker locker(&m_mutex);
                duration = m_duration;
            }
            const QImage image = grab(duration / 10);
            if (!image.isNull()) {
                QMetaObject::invokeMethod(this, [this, image, generation] {
                    addCover(image, generation);
                }, Qt::QueuedConnection);
            }
        }
        if (bucket >= 0) {
            const QImage image = grab(bucket * step);
            if (!image.isNull()) {
                QMetaObject::invokeMethod(this, [this, bucket, image, generation, step] {
                    addFrame(bucket, image, generation, step);
                }, Qt::QueuedConnection);
            }
        }
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
