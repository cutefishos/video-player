// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

// mpv's track-list, as the audio and subtitle choices the player offers.
namespace Tracks
{
struct Split {
    // Each entry: id, label, selected, external.
    QVariantList audio;
    QVariantList subtitle;
    // A real picture, not cover art attached to an audio file.
    bool hasVideo = false;
};

Split split(const QVariantList &trackList);

// The track's title and language, else its language, else "Track N".
QString label(const QVariantMap &track);
}
