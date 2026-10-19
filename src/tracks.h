// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "mpvnode.h"

#include <string>
#include <vector>

// mpv's track-list, as the audio and subtitle choices the player offers.
namespace tracks
{

struct Track
{
    int id = 0;
    std::string label;
    bool selected = false;
    bool external = false;

    bool operator==(const Track&) const = default;
};

struct Split
{
    std::vector<Track> audio;
    std::vector<Track> subtitle;
    // A real picture, not cover art attached to an audio file.
    bool hasVideo = false;
};

Split split(const MpvNode& trackList);

// The track's title and language, else its language, else "Track N".
std::string label(const MpvNode& track);

} // namespace tracks
