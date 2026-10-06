// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import FishUI as FishUI

FishUI.DesktopMenu {
    id: control

    required property var root

    FishUI.MenuItem {
        text: qsTr("No Audio Tracks")
        enabled: false
        visible: control.root.player.audioTracks.length === 0
    }

    Repeater {
        model: control.root.player.audioTracks

        FishUI.MenuItem {
            required property var modelData
            text: modelData.label
            checked: control.root.player.audioTrack === modelData.id
            onTriggered: control.root.selectAudio(modelData.id)
        }
    }
}
