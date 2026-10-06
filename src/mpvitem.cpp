// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpvitem.h"
#include "mpvnode.h"
#include "tracks.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMutex>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>
#include <QSGImageNode>

#include <mpv/render_gl.h>

#include <clocale>

// The mpv handle outlives the item while a renderer still holds its render
// context: that has to be freed on the render thread before the handle goes.
struct MpvHandle {
    mpv_handle *mpv = nullptr;
    QMutex mutex;
    MpvItem *item = nullptr;

    ~MpvHandle()
    {
        if (mpv)
            mpv_terminate_destroy(mpv);
    }
};

Q_LOGGING_CATEGORY(lcMpv, "cutefish.videoplayer.mpv", QtWarningMsg)

static void *glProcAddress(void *, const char *name)
{
    QOpenGLContext *context = QOpenGLContext::currentContext();
    return context ? reinterpret_cast<void *>(context->getProcAddress(name)) : nullptr;
}

// Runs on an mpv thread when events are waiting. The item may be going away
// meanwhile, so it is reached through the handle's guarded pointer.
static void requestEvents(void *context)
{
    auto *handle = static_cast<MpvHandle *>(context);
    QMutexLocker locker(&handle->mutex);
    if (handle->item)
        QMetaObject::invokeMethod(handle->item, "processEvents", Qt::QueuedConnection);
}

// Runs on an mpv thread whenever a new frame is due.
static void requestFrame(void *context)
{
    auto *handle = static_cast<MpvHandle *>(context);
    QMutexLocker locker(&handle->mutex);
    if (handle->item)
        QMetaObject::invokeMethod(handle->item, &QQuickItem::update, Qt::QueuedConnection);
}

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvRenderer(std::shared_ptr<MpvHandle> handle)
        : m_handle(std::move(handle))
    {
    }

    ~MpvRenderer() override
    {
        if (!m_context)
            return;
        mpv_render_context_free(m_context);
        QMutexLocker locker(&m_handle->mutex);
        if (m_handle->item)
            QMetaObject::invokeMethod(m_handle->item, &MpvItem::setRenderLost, Qt::QueuedConnection);
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        if (!m_context)
            createContext();
        return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
    }

    void render() override
    {
        if (!m_context)
            createContext();
        if (!m_context)
            return;
        QOpenGLFramebufferObject *fbo = framebufferObject();
        mpv_opengl_fbo target{static_cast<int>(fbo->handle()), fbo->width(), fbo->height(), 0};
        int flipY = 0;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &target},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context_render(m_context, params);
    }

private:
    void createContext()
    {
        if (!m_handle->mpv)
            return;
        mpv_opengl_init_params glParams{glProcAddress, nullptr};
        mpv_render_param display{MPV_RENDER_PARAM_INVALID, nullptr};
#if QT_CONFIG(wayland)
        // Lets hardware decoders hand frames to GL without a copy.
        if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
            display.type = MPV_RENDER_PARAM_WL_DISPLAY;
            display.data = wayland->display();
        }
#endif
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glParams},
            display,
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        if (mpv_render_context_create(&m_context, m_handle->mpv, params) < 0) {
            qWarning("mpv: could not create the OpenGL render context");
            m_context = nullptr;
            return;
        }
        mpv_render_context_set_update_callback(m_context, requestFrame, m_handle.get());
        QMutexLocker locker(&m_handle->mutex);
        if (m_handle->item)
            QMetaObject::invokeMethod(m_handle->item, &MpvItem::setRenderReady, Qt::QueuedConnection);
    }

    std::shared_ptr<MpvHandle> m_handle;
    mpv_render_context *m_context = nullptr;
};

static int fromTrackId(const QVariant &value)
{
    return value.typeId() == QMetaType::LongLong ? value.toInt() : 0;
}

MpvItem::MpvItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
    , m_handle(std::make_shared<MpvHandle>())
{
    // mpv parses numbers with the C locale; Qt set the user's at startup.
    std::setlocale(LC_NUMERIC, "C");

    m_handle->item = this;
}

// mpv starts with the first file, so an empty window costs no player.
bool MpvItem::ensurePlayer()
{
    if (m_handle->mpv)
        return true;
    if (m_playerFailed)
        return false;
    m_playerFailed = true;
    m_handle->mpv = mpv_create();
    if (!m_handle->mpv) {
        qWarning("mpv: could not create a player");
        return false;
    }

    setOption("vo", QStringLiteral("libmpv"));
    setOption("hwdec", QStringLiteral("auto-safe"));
    // Playback stops on the last frame; the playlist decides what comes next.
    setOption("keep-open", QStringLiteral("yes"));
    setOption("idle", QStringLiteral("yes"));
    setOption("input-default-bindings", false);
    setOption("input-vo-keyboard", false);
    setOption("osd-level", 0);
    setOption("sub-auto", QStringLiteral("fuzzy"));
    setOption("sub-codepage", QStringLiteral("auto"));
    setOption("sub-file-paths", QStringLiteral("subs:Subs:Subtitles:subtitles"));
    setOption("screenshot-format", QStringLiteral("png"));
    setOption("audio-client-name", QStringLiteral("cutefish-video-player"));
    setOption("volume-max", 150);

    // What was set before mpv started.
    for (const auto &property : std::as_const(m_pendingProperties))
        setOption(property.first.constData(), property.second);
    m_pendingProperties.clear();

    if (mpv_initialize(m_handle->mpv) < 0) {
        qWarning("mpv: could not initialise the player");
        mpv_destroy(m_handle->mpv);
        m_handle->mpv = nullptr;
        return false;
    }

    static const struct {
        const char *name;
        mpv_format format;
    } observed[] = {
        {"time-pos", MPV_FORMAT_DOUBLE},
        {"duration", MPV_FORMAT_DOUBLE},
        {"pause", MPV_FORMAT_FLAG},
        {"seekable", MPV_FORMAT_FLAG},
        {"paused-for-cache", MPV_FORMAT_FLAG},
        {"seeking", MPV_FORMAT_FLAG},
        {"eof-reached", MPV_FORMAT_FLAG},
        {"volume", MPV_FORMAT_DOUBLE},
        {"mute", MPV_FORMAT_FLAG},
        {"speed", MPV_FORMAT_DOUBLE},
        {"media-title", MPV_FORMAT_STRING},
        {"dwidth", MPV_FORMAT_INT64},
        {"dheight", MPV_FORMAT_INT64},
        {"track-list", MPV_FORMAT_NODE},
        {"aid", MPV_FORMAT_NODE},
        {"sid", MPV_FORMAT_NODE},
        {"sub-delay", MPV_FORMAT_DOUBLE},
        {"chapter-list", MPV_FORMAT_NODE},
        {"chapter", MPV_FORMAT_INT64},
        {"video-rotate", MPV_FORMAT_INT64},
        {"loop-file", MPV_FORMAT_STRING},
        {"hwdec-current", MPV_FORMAT_STRING},
        {"vo-configured", MPV_FORMAT_FLAG},
    };
    for (const auto &property : observed)
        mpv_observe_property(m_handle->mpv, 0, property.name, property.format);

    mpv_request_log_messages(m_handle->mpv, "error");
    mpv_set_wakeup_callback(m_handle->mpv, requestEvents, m_handle.get());
    m_playerFailed = false;
    // A frame now creates the render context, which the first load waits for.
    update();
    return true;
}

MpvItem::~MpvItem()
{
    {
        // First, so no callback reaches this item from here on.
        QMutexLocker locker(&m_handle->mutex);
        m_handle->item = nullptr;
    }
    if (m_softwareContext)
        mpv_render_context_free(m_softwareContext);
    if (m_handle->mpv) {
        mpv_set_wakeup_callback(m_handle->mpv, nullptr, nullptr);
        // Silences the player now; the handle itself may live on in the renderer.
        command({QStringLiteral("stop")});
    }
}

QSize MpvItem::probeVideoSize(const QUrl &url, int timeout)
{
    if (!url.isLocalFile())
        return {};
    std::setlocale(LC_NUMERIC, "C");
    mpv_handle *mpv = mpv_create();
    if (!mpv)
        return {};
    mpv_set_option_string(mpv, "vo", "null");
    mpv_set_option_string(mpv, "ao", "null");
    mpv_set_option_string(mpv, "aid", "no");
    mpv_set_option_string(mpv, "sid", "no");
    mpv_set_option_string(mpv, "pause", "yes");
    mpv_set_option_string(mpv, "load-scripts", "no");
    mpv_set_option_string(mpv, "ytdl", "no");

    QSize size;
    const QByteArray path = url.toLocalFile().toUtf8();
    const char *command[] = {"loadfile", path.constData(), nullptr};
    if (mpv_initialize(mpv) >= 0 && mpv_command(mpv, command) >= 0) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeout) {
            const mpv_event_id event = mpv_wait_event(mpv, (timeout - timer.elapsed()) / 1000.0)->event_id;
            if (event == MPV_EVENT_END_FILE || event == MPV_EVENT_SHUTDOWN)
                break;
            if (event != MPV_EVENT_FILE_LOADED)
                continue;
            mpv_node node;
            if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) < 0)
                break;
            const QVariantList tracks = MpvNode::toVariant(&node).toList();
            mpv_free_node_contents(&node);
            for (const QVariant &entry : tracks) {
                const QVariantMap track = entry.toMap();
                if (track.value(QStringLiteral("type")).toString() != QLatin1String("video")
                        || !track.value(QStringLiteral("selected")).toBool() || track.value(QStringLiteral("albumart")).toBool())
                    continue;
                // The display size as mpv derives it: the pixel aspect widens
                // the picture or makes it taller, never narrower.
                int width = track.value(QStringLiteral("demux-w")).toInt();
                int height = track.value(QStringLiteral("demux-h")).toInt();
                const double aspect = track.value(QStringLiteral("demux-par"), 1.0).toDouble();
                if (aspect > 1)
                    width = int(width * aspect + 1e-6);
                else if (aspect > 0 && aspect < 1)
                    height = int(height / aspect + 1e-6);
                if (width > 0 && height > 0)
                    size = QSize(width, height);
                break;
            }
            break;
        }
    }
    mpv_terminate_destroy(mpv);
    return size;
}

QQuickFramebufferObject::Renderer *MpvItem::createRenderer() const
{
    return new MpvRenderer(m_handle);
}

void MpvItem::setRenderReady()
{
    if (m_renderReady)
        return;
    m_renderReady = true;
    if (m_videoLost) {
        // mpv dropped the video output with the old context: a file still open
        // gets its picture back, one that ended for want of it is reopened.
        m_videoLost = false;
        if (m_loaded) {
            setMpvProperty("vid", QStringLiteral("no"));
            setMpvProperty("vid", QStringLiteral("auto"));
        } else if (!m_source.isEmpty() && m_pendingLoad.isEmpty() && !m_lostState.isEmpty()) {
            const bool paused = m_lostState.take(QStringLiteral("pause")).toBool();
            const QVariantMap options = m_lostState;
            m_lostState.clear();
            m_reopening = true;
            load(m_source, options, paused);
            return;
        }
    }
    startLoad();
}

void MpvItem::setRenderLost()
{
    if (!m_renderReady)
        return;
    m_renderReady = false;
    m_videoLost = true;
    m_lostState.clear();
    if (m_loaded) {
        m_lostState = {
            {QStringLiteral("start"), m_position},
            {QStringLiteral("speed"), m_speed},
            {QStringLiteral("sub-delay"), m_subtitleDelay},
            {QStringLiteral("aid"), m_audioTrack > 0 ? QVariant(m_audioTrack) : QVariant(QStringLiteral("no"))},
            {QStringLiteral("sid"), m_subtitleTrack > 0 ? QVariant(m_subtitleTrack) : QVariant(QStringLiteral("no"))},
            {QStringLiteral("pause"), m_paused},
        };
    }
}

void MpvItem::itemChange(ItemChange change, const ItemChangeData &value)
{
    // Leaving a software-rendered window takes the context with it.
    if (change == ItemSceneChange && m_softwareContext)
        releaseSoftwareContext();
    QQuickFramebufferObject::itemChange(change, value);
}

void MpvItem::releaseSoftwareContext()
{
    mpv_render_context_free(m_softwareContext);
    m_softwareContext = nullptr;
    setRenderLost();
}

QSGNode *MpvItem::updatePaintNode(QSGNode *node, UpdatePaintNodeData *data)
{
    if (window() && window()->rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
        return updateSoftwareNode(node);
    return QQuickFramebufferObject::updatePaintNode(node, data);
}

// The software renderer draws on the GUI thread, so the context lives here.
QSGNode *MpvItem::updateSoftwareNode(QSGNode *oldNode)
{
    if (!m_softwareContext && m_handle->mpv) {
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_SW)},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        if (mpv_render_context_create(&m_softwareContext, m_handle->mpv, params) < 0) {
            m_softwareContext = nullptr;
            return oldNode;
        }
        mpv_render_context_set_update_callback(m_softwareContext, requestFrame, m_handle.get());
        QMetaObject::invokeMethod(this, &MpvItem::setRenderReady, Qt::QueuedConnection);
    }

    const qreal ratio = window()->effectiveDevicePixelRatio();
    const QSize size = (QSizeF(width(), height()) * ratio).toSize();
    if (!m_softwareContext || size.isEmpty()) {
        delete oldNode;
        return nullptr;
    }

    QImage image(size, QImage::Format_RGBX8888);
    int imageSize[2] = {size.width(), size.height()};
    size_t stride = image.bytesPerLine();
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_SW_SIZE, imageSize},
        {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("rgb0")},
        {MPV_RENDER_PARAM_SW_STRIDE, &stride},
        {MPV_RENDER_PARAM_SW_POINTER, image.bits()},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    if (mpv_render_context_render(m_softwareContext, params) < 0)
        image.fill(Qt::black);

    auto *node = static_cast<QSGImageNode *>(oldNode);
    if (!node)
        node = window()->createImageNode();
    node->setTexture(window()->createTextureFromImage(image));
    node->setOwnsTexture(true);
    node->setRect(boundingRect());
    return node;
}

void MpvItem::processEvents()
{
    while (m_handle->mpv) {
        mpv_event *event = mpv_wait_event(m_handle->mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;

        switch (event->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE:
            handlePropertyChange(static_cast<mpv_event_property *>(event->data));
            break;
        case MPV_EVENT_FILE_LOADED:
            m_loaded = true;
            Q_EMIT loadedChanged();
            // A file reopened after a lost picture is not news to the window.
            if (!m_reopening)
                Q_EMIT fileLoaded();
            m_reopening = false;
            break;
        case MPV_EVENT_END_FILE: {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (m_loaded) {
                m_loaded = false;
                Q_EMIT loadedChanged();
            }
            // Ending because the picture went away is recovered from, not reported.
            if (end->reason == MPV_END_FILE_REASON_ERROR && !m_videoLost)
                Q_EMIT playbackFailed(QString::fromUtf8(mpv_error_string(end->error)));
            break;
        }
        case MPV_EVENT_LOG_MESSAGE: {
            const auto *message = static_cast<mpv_event_log_message *>(event->data);
            qCWarning(lcMpv, "%s: %s", message->prefix, QByteArray(message->text).trimmed().constData());
            break;
        }
        case MPV_EVENT_COMMAND_REPLY:
            if (event->reply_userdata >= FirstScreenshot) {
                const QString path = m_pendingScreenshots.take(event->reply_userdata);
                Q_EMIT screenshotSaved(path, event->error >= 0);
            } else if (event->reply_userdata == SubtitleRequest) {
                Q_EMIT subtitleAdded(event->error >= 0);
            }
            break;
        default:
            break;
        }
    }
}

void MpvItem::handlePropertyChange(const mpv_event_property *property)
{
    const QByteArray name(property->name);
    const bool unset = property->format == MPV_FORMAT_NONE;
    auto asDouble = [&] { return unset ? 0.0 : *static_cast<double *>(property->data); };
    auto asFlag = [&] { return !unset && *static_cast<int *>(property->data) != 0; };
    auto asInt = [&] { return unset ? 0 : int(*static_cast<int64_t *>(property->data)); };
    auto asString = [&] { return unset ? QString() : QString::fromUtf8(*static_cast<char **>(property->data)); };
    auto asNode = [&] { return unset ? QVariant() : MpvNode::toVariant(static_cast<mpv_node *>(property->data)); };

    auto update = [this](auto &field, const auto &value, void (MpvItem::*signal)()) {
        if (field == value)
            return;
        field = value;
        Q_EMIT(this->*signal)();
    };

    if (name == "time-pos") {
        update(m_position, asDouble(), &MpvItem::positionChanged);
    } else if (name == "duration") {
        update(m_duration, asDouble(), &MpvItem::durationChanged);
    } else if (name == "pause") {
        update(m_paused, asFlag(), &MpvItem::pausedChanged);
    } else if (name == "seekable") {
        update(m_seekable, asFlag(), &MpvItem::seekableChanged);
    } else if (name == "paused-for-cache" || name == "seeking") {
        const bool before = buffering();
        (name == "seeking" ? m_seeking : m_pausedForCache) = asFlag();
        if (before != buffering())
            Q_EMIT bufferingChanged();
    } else if (name == "eof-reached") {
        update(m_endReached, asFlag(), &MpvItem::endReachedChanged);
    } else if (name == "volume") {
        if (!unset)
            update(m_volume, qRound(asDouble()), &MpvItem::volumeChanged);
    } else if (name == "mute") {
        update(m_muted, asFlag(), &MpvItem::mutedChanged);
    } else if (name == "speed") {
        if (!unset)
            update(m_speed, asDouble(), &MpvItem::speedChanged);
    } else if (name == "media-title") {
        update(m_mediaTitle, asString(), &MpvItem::mediaTitleChanged);
    } else if (name == "dwidth") {
        update(m_videoSize, QSize(asInt(), m_videoSize.height()), &MpvItem::videoSizeChanged);
    } else if (name == "dheight") {
        update(m_videoSize, QSize(m_videoSize.width(), asInt()), &MpvItem::videoSizeChanged);
    } else if (name == "track-list") {
        const Tracks::Split tracks = Tracks::split(asNode().toList());
        m_audioTracks = tracks.audio;
        m_subtitleTracks = tracks.subtitle;
        m_hasVideo = tracks.hasVideo;
        Q_EMIT tracksChanged();
    } else if (name == "aid") {
        update(m_audioTrack, fromTrackId(asNode()), &MpvItem::audioTrackChanged);
    } else if (name == "sid") {
        update(m_subtitleTrack, fromTrackId(asNode()), &MpvItem::subtitleTrackChanged);
    } else if (name == "sub-delay") {
        update(m_subtitleDelay, asDouble(), &MpvItem::subtitleDelayChanged);
    } else if (name == "chapter-list") {
        update(m_chapters, asNode().toList(), &MpvItem::chaptersChanged);
    } else if (name == "chapter") {
        update(m_chapter, unset ? -1 : asInt(), &MpvItem::chapterChanged);
    } else if (name == "video-rotate") {
        update(m_rotation, asInt(), &MpvItem::rotationChanged);
    } else if (name == "loop-file") {
        const QString loop = asString();
        update(m_loopFile, !loop.isEmpty() && loop != QLatin1String("no"), &MpvItem::loopFileChanged);
    } else if (name == "vo-configured") {
        update(m_videoActive, asFlag(), &MpvItem::videoActiveChanged);
    } else if (name == "hwdec-current") {
        const QString decoder = asString();
        update(m_hardwareDecoder, decoder == QLatin1String("no") ? QString() : decoder,
               &MpvItem::hardwareDecoderChanged);
    }
}

void MpvItem::command(const QVariantList &arguments, uint64_t request)
{
    if (!m_handle->mpv)
        return;
    mpv_node node;
    MpvNode::fromVariant(&node, arguments);
    mpv_command_node_async(m_handle->mpv, request, &node);
    MpvNode::release(&node);
}

void MpvItem::setOption(const char *name, const QVariant &value)
{
    mpv_node node;
    MpvNode::fromVariant(&node, value);
    mpv_set_option(m_handle->mpv, name, MPV_FORMAT_NODE, &node);
    MpvNode::release(&node);
}

void MpvItem::setMpvProperty(const char *name, const QVariant &value)
{
    if (!m_handle->mpv) {
        m_pendingProperties.append({QByteArray(name), value});
        return;
    }
    mpv_node node;
    MpvNode::fromVariant(&node, value);
    mpv_set_property_async(m_handle->mpv, GenericRequest, name, MPV_FORMAT_NODE, &node);
    MpvNode::release(&node);
}

void MpvItem::setPaused(bool paused)
{
    // Playing again from the last frame starts the file over.
    if (!paused && m_endReached && !m_loopFile)
        seek(0, true);
    setMpvProperty("pause", paused);
}

void MpvItem::setVolume(int volume)
{
    setMpvProperty("volume", double(qBound(0, volume, 150)));
}

void MpvItem::setMuted(bool muted)
{
    setMpvProperty("mute", muted);
}

void MpvItem::setSpeed(double speed)
{
    setMpvProperty("speed", qBound(0.25, speed, 4.0));
}

void MpvItem::setAudioTrack(int id)
{
    setMpvProperty("aid", id > 0 ? QVariant(id) : QVariant(QStringLiteral("no")));
}

void MpvItem::setSubtitleTrack(int id)
{
    setMpvProperty("sid", id > 0 ? QVariant(id) : QVariant(QStringLiteral("no")));
}

void MpvItem::setSubtitleDelay(double delay)
{
    setMpvProperty("sub-delay", delay);
}

void MpvItem::setChapter(int chapter)
{
    if (m_chapters.isEmpty())
        return;
    setMpvProperty("chapter", qBound(0, chapter, int(m_chapters.size()) - 1));
}

void MpvItem::setRotation(int degrees)
{
    setMpvProperty("video-rotate", ((degrees % 360) + 360) % 360);
}

void MpvItem::setLoopFile(bool loop)
{
    setMpvProperty("loop-file", loop ? QStringLiteral("inf") : QStringLiteral("no"));
}

void MpvItem::load(const QUrl &url, const QVariantMap &options, bool paused)
{
    if (!ensurePlayer()) {
        Q_EMIT playbackFailed(QStringLiteral("mpv could not be started"));
        return;
    }
    if (m_source != url) {
        m_source = url;
        Q_EMIT sourceChanged();
    }
    resetMedia();
    setMpvProperty("pause", paused);
    QStringList fileOptions;
    for (auto it = options.cbegin(); it != options.cend(); ++it) {
        const QVariant &value = it.value();
        fileOptions.append(it.key() + QLatin1Char('=')
                           + (value.typeId() == QMetaType::Double ? QString::number(value.toDouble(), 'f', 3)
                                                                  : value.toString()));
    }
    const QString target = url.isLocalFile() ? url.toLocalFile() : url.toString(QUrl::FullyEncoded);
    m_pendingLoad = {QStringLiteral("loadfile"), target, QStringLiteral("replace"), -1, fileOptions.join(QLatin1Char(','))};
    startLoad();
}

void MpvItem::startLoad()
{
    if (!m_renderReady || m_pendingLoad.isEmpty())
        return;
    command(m_pendingLoad);
    m_pendingLoad.clear();
}

void MpvItem::stop()
{
    m_pendingLoad.clear();
    command({QStringLiteral("stop")});
    if (!m_source.isEmpty()) {
        m_source.clear();
        Q_EMIT sourceChanged();
    }
    resetMedia();
}

void MpvItem::resetMedia()
{
    // Per-file settings start over with each file.
    setMpvProperty("speed", 1.0);
    setMpvProperty("sub-delay", 0.0);
    setMpvProperty("video-rotate", 0);
}

void MpvItem::seek(double seconds, bool exact)
{
    command({QStringLiteral("seek"), qMax(0.0, seconds),
             exact ? QStringLiteral("absolute+exact") : QStringLiteral("absolute+keyframes")});
}

void MpvItem::seekRelative(double seconds)
{
    command({QStringLiteral("seek"), seconds, QStringLiteral("relative+exact")});
}

void MpvItem::frameStep(bool backwards)
{
    command({backwards ? QStringLiteral("frame-back-step") : QStringLiteral("frame-step")});
}

void MpvItem::addSubtitle(const QUrl &url)
{
    command({QStringLiteral("sub-add"), url.isLocalFile() ? url.toLocalFile() : url.toString(QUrl::FullyEncoded),
             QStringLiteral("select")}, SubtitleRequest);
}

void MpvItem::screenshot(const QString &path)
{
    const uint64_t request = m_nextScreenshot++;
    m_pendingScreenshots.insert(request, path);
    command({QStringLiteral("screenshot-to-file"), path, QStringLiteral("subtitles")}, request);
}

QVariantMap MpvItem::mediaInfo() const
{
    QVariantMap info;
    if (!m_handle->mpv || !m_loaded)
        return info;
    static const char *const names[] = {
        "path", "file-format", "file-size", "duration", "width", "height", "container-fps",
        "video-codec", "video-bitrate", "audio-codec-name", "audio-bitrate",
        "audio-params/samplerate", "audio-params/channel-count",
    };
    for (const char *name : names) {
        mpv_node node;
        if (mpv_get_property(m_handle->mpv, name, MPV_FORMAT_NODE, &node) < 0)
            continue;
        info.insert(QString::fromLatin1(name), MpvNode::toVariant(&node));
        mpv_free_node_contents(&node);
    }
    return info;
}

void MpvItem::setPreferredLanguages(const QStringList &audio, const QStringList &subtitle)
{
    setMpvProperty("alang", audio.join(QLatin1Char(',')));
    setMpvProperty("slang", subtitle.join(QLatin1Char(',')));
}

void MpvItem::setSubtitleScale(double scale)
{
    setMpvProperty("sub-scale", qBound(0.5, scale, 2.0));
}
