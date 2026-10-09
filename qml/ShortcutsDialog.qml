// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

FishUI.DialogWindow {
    id: dialog
    objectName: "shortcutsDialog"

    readonly property var sections: [
        { title: qsTr("Playback"), keys: [
            [qsTr("Play or pause"), qsTr("Space, K")],
            [qsTr("Back or forward 5 seconds"), "← →"],
            [qsTr("Back or forward 30 seconds"), qsTr("Shift+← →")],
            [qsTr("Go to the start or the end"), qsTr("Home, End")],
            [qsTr("Slower, faster, normal speed"), qsTr("[ ] Backspace")],
            [qsTr("Previous or next frame"), ", ."],
            [qsTr("Previous or next chapter"), qsTr("Page Up, Page Down")],
            [qsTr("Previous or next file"), "B, N"]
        ] },
        { title: qsTr("Sound and Subtitles"), keys: [
            [qsTr("Volume up or down"), "↑ ↓"],
            [qsTr("Mute"), "M"],
            [qsTr("Next audio track"), "A"],
            [qsTr("Next or previous subtitle"), qsTr("J, Shift+J")],
            [qsTr("Show or hide subtitles"), "V"],
            [qsTr("Subtitles earlier or later"), qsTr("Z, Shift+Z")]
        ] },
        { title: qsTr("Window"), keys: [
            [qsTr("Full screen"), qsTr("F, F11, double-click")],
            [qsTr("Leave full screen"), "Esc"],
            [qsTr("Playlist"), "P"],
            [qsTr("Open a file"), FishUI.KeySequence.nativeText("Ctrl+O")],
            [qsTr("Open a URL"), FishUI.KeySequence.nativeText("Ctrl+L")],
            [qsTr("Take a screenshot"), FishUI.KeySequence.nativeText("Ctrl+Alt+S")],
            [qsTr("Media info"), FishUI.KeySequence.nativeText("Ctrl+I")],
            [qsTr("Settings"), FishUI.KeySequence.nativeText("Ctrl+,")],
            [qsTr("Keyboard shortcuts"), FishUI.KeySequence.nativeText("Ctrl+?")],
            [qsTr("Close"), FishUI.KeySequence.nativeText("Ctrl+W")]
        ] }
    ]

    title: qsTr("Keyboard Shortcuts")
    width: 480
    contentHeight: 600 - header.height
    blurEnabled: false
    background.color: FishUI.Theme.backgroundColor
    background.opacity: 1

    Flickable {
        id: flickable
        anchors.fill: parent
        contentHeight: layout.implicitHeight + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        FishUI.WheelHandler {
            target: flickable
        }
        FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

        ColumnLayout {
            id: layout
            x: 16
            y: 4
            width: flickable.width - 32
            spacing: 12

            Repeater {
                model: dialog.sections

                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 8

                    SectionLabel {
                        text: modelData.title
                    }

                    ListCard {
                        Repeater {
                            model: modelData.keys

                            InfoRow {
                                required property var modelData
                                text: modelData[0]
                                value: modelData[1]
                            }
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: dialog.close()
    }
}
