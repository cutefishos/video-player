// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import FishUI as FishUI

// The subtitle tracks, adding a file, and the timing; with audio, the audio
// tracks as a submenu.
FishUI.DesktopMenu {
    id: control

    required property var root
    property bool withAudio: false

    FishUI.MenuItem {
        text: qsTr("Off")
        checked: control.root.player.subtitleTrack === 0
        onTriggered: control.root.selectSubtitle(0)
    }

    Repeater {
        model: control.root.player.subtitleTracks

        FishUI.MenuItem {
            required property var modelData
            text: modelData.label
            checked: control.root.player.subtitleTrack === modelData.id
            onTriggered: control.root.selectSubtitle(modelData.id)
        }
    }

    FishUI.MenuSeparator {}

    FishUI.MenuItem {
        text: qsTr("Add Subtitle File…")
        onTriggered: control.root.openSubtitleFile()
    }

    FishUI.MenuItem {
        text: qsTr("Subtitle Timing")
        enabled: control.root.player.subtitleTrack > 0

        childMenu: FishUI.DesktopMenu {
            FishUI.MenuItem {
                text: qsTr("Show Earlier")
                shortcut: "Z"
                onTriggered: control.root.shiftSubtitles(-0.1)
            }
            FishUI.MenuItem {
                text: qsTr("Show Later")
                shortcut: "Shift+Z"
                onTriggered: control.root.shiftSubtitles(0.1)
            }
            FishUI.MenuItem {
                text: qsTr("Reset (%1)").arg(control.root.delayText(control.root.player.subtitleDelay))
                enabled: control.root.player.subtitleDelay !== 0
                onTriggered: control.root.shiftSubtitles(-control.root.player.subtitleDelay)
            }
        }
    }

    FishUI.MenuSeparator {
        visible: control.withAudio
    }

    FishUI.MenuItem {
        text: qsTr("Audio")
        visible: control.withAudio
        childMenu: AudioMenu {
            root: control.root
        }
    }
}
