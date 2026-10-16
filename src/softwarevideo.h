// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "base/signal.h"
#include "base/weak_ptr.h"
#include "ui/image.h"

#include <memory>

class Player;
struct MpvHandle;
struct mpv_render_context;

// A video output drawing through mpv's software render API into an image on
// the loop's thread: the fallback without a GPU, and what headless renders
// and the tests use.
class SoftwareVideo
{
public:
    explicit SoftwareVideo(Player& player);
    // Tells the player its picture is gone.
    ~SoftwareVideo();

    SoftwareVideo(const SoftwareVideo&) = delete;
    SoftwareVideo& operator=(const SoftwareVideo&) = delete;

    // Makes the render context once the player has started; false until then.
    bool ensureContext();
    bool hasContext() const { return m_context != nullptr; }
    // The current frame at width x height device pixels, opaque; black when
    // mpv has none.
    const cutefish::ui::Image& render(int width, int height);
    // Drops the context, as a window going away does.
    void release();

    // mpv has a new frame to draw; render() again.
    cutefish::base::Signal<> frameDue;

private:
    cutefish::base::WeakPtr<Player> m_player;
    std::shared_ptr<MpvHandle> m_handle;
    mpv_render_context* m_context = nullptr;
    cutefish::ui::Image m_image;
    cutefish::base::WeakPtrFactory<SoftwareVideo> m_weakFactory{this};
};
