// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QQuickFramebufferObject>
#include <QSize>
#include <QUrl>
#include <QVariantList>

#include <memory>

#include <mpv/render.h>

#include <mpv/client.h>

struct MpvHandle;

// A video surface driven by libmpv's render API. mpv draws into the item's
// framebuffer object on the render thread; everything else runs on the GUI
// thread through the asynchronous client API, so no call here ever blocks.
class MpvItem : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source NOTIFY sourceChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    Q_PROPERTY(bool endReached READ endReached NOTIFY endReachedChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaTitleChanged)
    Q_PROPERTY(QSize videoSize READ videoSize NOTIFY videoSizeChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY tracksChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY tracksChanged)
    Q_PROPERTY(int audioTrack READ audioTrack WRITE setAudioTrack NOTIFY audioTrackChanged)
    Q_PROPERTY(int subtitleTrack READ subtitleTrack WRITE setSubtitleTrack NOTIFY subtitleTrackChanged)
    Q_PROPERTY(double subtitleDelay READ subtitleDelay WRITE setSubtitleDelay NOTIFY subtitleDelayChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(int chapter READ chapter WRITE setChapter NOTIFY chapterChanged)
    Q_PROPERTY(int rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(bool loopFile READ loopFile WRITE setLoopFile NOTIFY loopFileChanged)
    Q_PROPERTY(QString hardwareDecoder READ hardwareDecoder NOTIFY hardwareDecoderChanged)
    // mpv's video output is up and drawing.
    Q_PROPERTY(bool videoActive READ videoActive NOTIFY videoActiveChanged)

public:
    explicit MpvItem(QQuickItem *parent = nullptr);
    ~MpvItem() override;

    // The picture size the player would report for a local file, read from
    // its container without decoding; empty if mpv cannot tell within timeout ms.
    static QSize probeVideoSize(const QUrl &url, int timeout);

    Renderer *createRenderer() const override;
    // Called by the renderer once mpv can draw: files load only after that,
    // or mpv would find no video output and play them without a picture.
    Q_INVOKABLE void setRenderReady();
    // The render context went away with the scene graph; a new one follows.
    Q_INVOKABLE void setRenderLost();

    QUrl source() const { return m_source; }
    bool loaded() const { return m_loaded; }
    bool paused() const { return m_paused; }
    void setPaused(bool paused);
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    bool seekable() const { return m_seekable; }
    bool buffering() const { return m_pausedForCache || m_seeking; }
    bool endReached() const { return m_endReached; }
    int volume() const { return m_volume; }
    void setVolume(int volume);
    bool muted() const { return m_muted; }
    void setMuted(bool muted);
    double speed() const { return m_speed; }
    void setSpeed(double speed);
    QString mediaTitle() const { return m_mediaTitle; }
    QSize videoSize() const { return m_videoSize; }
    QVariantList audioTracks() const { return m_audioTracks; }
    QVariantList subtitleTracks() const { return m_subtitleTracks; }
    bool hasVideo() const { return m_hasVideo; }
    int audioTrack() const { return m_audioTrack; }
    void setAudioTrack(int id);
    int subtitleTrack() const { return m_subtitleTrack; }
    void setSubtitleTrack(int id);
    double subtitleDelay() const { return m_subtitleDelay; }
    void setSubtitleDelay(double delay);
    QVariantList chapters() const { return m_chapters; }
    int chapter() const { return m_chapter; }
    void setChapter(int chapter);
    int rotation() const { return m_rotation; }
    void setRotation(int degrees);
    bool loopFile() const { return m_loopFile; }
    void setLoopFile(bool loop);
    QString hardwareDecoder() const { return m_hardwareDecoder; }
    bool videoActive() const { return m_videoActive; }

    // Opens url with mpv file options such as start, aid, sid and sub-delay;
    // paused leaves it on its first frame.
    Q_INVOKABLE void load(const QUrl &url, const QVariantMap &options = {}, bool paused = false);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(double seconds, bool exact = false);
    Q_INVOKABLE void seekRelative(double seconds);
    Q_INVOKABLE void frameStep(bool backwards);
    Q_INVOKABLE void addSubtitle(const QUrl &url);
    // Saves the current frame, with subtitles, as a PNG at path.
    Q_INVOKABLE void screenshot(const QString &path);
    // Codecs, size, frame rate and bit rates of the loaded file.
    Q_INVOKABLE QVariantMap mediaInfo() const;
    // Languages to pick audio and subtitle tracks by, most wanted first.
    Q_INVOKABLE void setPreferredLanguages(const QStringList &audio, const QStringList &subtitle);
    Q_INVOKABLE void setSubtitleScale(double scale);
    // Keeps subtitles above the bottom inset logical pixels of the item, such
    // as controls drawn over the video.
    Q_INVOKABLE void setSubtitleBottomInset(qreal inset);

Q_SIGNALS:
    void sourceChanged();
    void loadedChanged();
    void pausedChanged();
    void positionChanged();
    void durationChanged();
    void seekableChanged();
    void bufferingChanged();
    void endReachedChanged();
    void volumeChanged();
    void mutedChanged();
    void speedChanged();
    void mediaTitleChanged();
    void videoSizeChanged();
    void tracksChanged();
    void audioTrackChanged();
    void subtitleTrackChanged();
    void subtitleDelayChanged();
    void chaptersChanged();
    void chapterChanged();
    void rotationChanged();
    void loopFileChanged();
    void hardwareDecoderChanged();
    void videoActiveChanged();

    void fileLoaded();
    void playbackFailed(const QString &message);
    void screenshotSaved(const QString &path, bool success);
    void subtitleAdded(bool success);

private:
    // Reply ids: screenshots count up from FirstScreenshot.
    enum Request : uint64_t {
        GenericRequest,
        SubtitleRequest,
        FirstScreenshot = 1000,
    };

    bool ensurePlayer();
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;
    QSGNode *updateSoftwareNode(QSGNode *node);
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    void releaseSoftwareContext();
    void startLoad();
    Q_INVOKABLE void processEvents();
    void handlePropertyChange(const mpv_event_property *property);
    void command(const QVariantList &arguments, uint64_t request = GenericRequest);
    void setOption(const char *name, const QVariant &value);
    void setMpvProperty(const char *name, const QVariant &value);
    void resetMedia();

    std::shared_ptr<MpvHandle> m_handle;
    QHash<uint64_t, QString> m_pendingScreenshots;
    uint64_t m_nextScreenshot = FirstScreenshot;
    bool m_renderReady = false;
    // The render context was lost while a file played; video comes back with the next one.
    bool m_videoLost = false;
    // Without a picture mpv may end the file; then it is reopened where it was.
    QVariantMap m_lostState;
    bool m_reopening = false;
    bool m_playerFailed = false;
    QList<QPair<QByteArray, QVariant>> m_pendingProperties;
    QVariantList m_pendingLoad;
    // Without OpenGL (Qt Quick's software renderer) mpv draws into memory.
    mpv_render_context *m_softwareContext = nullptr;

    QUrl m_source;
    bool m_loaded = false;
    bool m_paused = false;
    double m_position = 0;
    double m_duration = 0;
    bool m_seekable = false;
    bool m_pausedForCache = false;
    bool m_seeking = false;
    bool m_endReached = false;
    int m_volume = 100;
    bool m_muted = false;
    double m_speed = 1.0;
    QString m_mediaTitle;
    QSize m_videoSize;
    QVariantList m_audioTracks;
    QVariantList m_subtitleTracks;
    bool m_hasVideo = false;
    int m_audioTrack = 0;
    int m_subtitleTrack = 0;
    double m_subtitleDelay = 0;
    qreal m_subtitleInset = 0;
    QVariantList m_chapters;
    int m_chapter = -1;
    int m_rotation = 0;
    bool m_loopFile = false;
    QString m_hardwareDecoder;
    bool m_videoActive = false;
};
