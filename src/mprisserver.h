// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/dbus.h"
#include "base/signal.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The player's MPRIS face (org.mpris.MediaPlayer2.<name>), for the Control
// Center and other media controls, as cutefish-framework's MprisPlayer
// served it: the window sets the state, requests come back as signals.
class MprisServer
{
public:
    enum class PlaybackStatus { Playing, Paused, Stopped };
    enum class LoopStatus { None, Track, Playlist };

    struct Metadata
    {
        // An object path.
        std::string trackId;
        std::string title;
        // Microseconds.
        int64_t length = 0;
        std::string url;
        // Empty for none.
        std::string artUrl;

        bool operator==(const Metadata&) const = default;
    };

    struct Description
    {
        std::string identity;
        std::string desktopEntry;
        std::vector<std::string> supportedUriSchemes;
        std::vector<std::string> supportedMimeTypes;
        bool canQuit = false;
        bool canRaise = false;
        bool canSetFullscreen = false;
        bool hasTrackList = false;
    };

    // Takes "org.mpris.MediaPlayer2." + serviceName on bus.
    MprisServer(cutefish::base::dbus::Connection* bus, const std::string& serviceName, Description description);
    ~MprisServer();

    bool isRegistered() const { return m_registered; }

    void setFullscreen(bool fullscreen);
    void setCanControl(bool can);
    void setCanPlay(bool can);
    void setCanPause(bool can);
    void setCanSeek(bool can);
    void setCanGoNext(bool can);
    void setCanGoPrevious(bool can);
    void setPlaybackStatus(PlaybackStatus status);
    void setLoopStatus(LoopStatus status);
    void setRate(double rate);
    void setMinimumRate(double rate);
    void setMaximumRate(double rate);
    // 0 to 1, more when boosted.
    void setVolume(double volume);
    // Microseconds; read on demand, never announced.
    void setPosition(int64_t position);
    void setMetadata(const Metadata& metadata);
    // After a seek or a jump: microseconds.
    void seeked(int64_t position);

    cutefish::base::Signal<> quitRequested;
    cutefish::base::Signal<> raiseRequested;
    cutefish::base::Signal<bool> fullscreenRequested;
    cutefish::base::Signal<> playRequested;
    cutefish::base::Signal<> pauseRequested;
    cutefish::base::Signal<> stopRequested;
    cutefish::base::Signal<> nextRequested;
    cutefish::base::Signal<> previousRequested;
    // Microseconds, relative.
    cutefish::base::Signal<int64_t> seekRequested;
    // The track id and microseconds.
    cutefish::base::Signal<const std::string&, int64_t> setPositionRequested;
    cutefish::base::Signal<double> volumeRequested;
    cutefish::base::Signal<double> rateRequested;
    cutefish::base::Signal<LoopStatus> loopStatusRequested;
    cutefish::base::Signal<bool> shuffleRequested;
    cutefish::base::Signal<const std::string&> openUriRequested;

    static std::string toString(PlaybackStatus status);
    static std::string toString(LoopStatus status);

private:
    void changedPlayer(const std::vector<std::string>& names);
    cutefish::base::dbus::VariantMap typedMetadata() const;

    cutefish::base::dbus::Connection* m_bus;
    std::string m_name;
    bool m_registered = false;
    Description m_description;
    std::unique_ptr<cutefish::base::dbus::Object> m_root;
    std::unique_ptr<cutefish::base::dbus::Object> m_player;

    bool m_fullscreen = false;
    bool m_canControl = false;
    bool m_canPlay = false;
    bool m_canPause = false;
    bool m_canSeek = false;
    bool m_canGoNext = false;
    bool m_canGoPrevious = false;
    PlaybackStatus m_playbackStatus = PlaybackStatus::Stopped;
    LoopStatus m_loopStatus = LoopStatus::None;
    double m_rate = 1;
    double m_minimumRate = 1;
    double m_maximumRate = 1;
    double m_volume = 0;
    int64_t m_position = 0;
    bool m_shuffle = false;
    Metadata m_metadata;
};
