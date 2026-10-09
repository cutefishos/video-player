// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// What the playing file is: where it is, how long and large, and how its
// picture and sound are encoded.
FishUI.DialogWindow {
    id: dialog
    objectName: "mediaInfoDialog"

    property var info: ({})
    property string name

    title: qsTr("Media Info")
    width: 440
    // The layout's 4px top inset, and the same 16px below as at the sides.
    contentHeight: Math.ceil(layout.implicitHeight) + 4 + 16
    blurEnabled: false
    background.color: FishUI.Theme.backgroundColor
    background.opacity: 1

    function present(details, title) {
        info = details
        name = title
        visible = true
        raise()
        requestActivate()
    }

    function value(key) {
        const v = info[key]
        return v === undefined || v === null || v === "" ? "" : v
    }

    function bitrate(key) {
        const v = value(key)
        return v ? qsTr("%1 kb/s").arg(Math.round(v / 1000)) : ""
    }

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        anchors.topMargin: 4
        spacing: 12

        FishUI.Label {
            Layout.fillWidth: true
            leftPadding: 16
            rightPadding: 16
            text: dialog.name
            textFormat: Text.PlainText
            font.pointSize: 12
            font.weight: Font.Medium
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        ListCard {
            InfoRow {
                text: qsTr("Location")
                value: dialog.value("path")
            }
            InfoRow {
                text: qsTr("Format")
                value: dialog.value("file-format")
                visible: value.length > 0
            }
            InfoRow {
                text: qsTr("Size")
                value: dialog.value("file-size") ? App.formatBytes(dialog.value("file-size")) : ""
                visible: value.length > 0
            }
            InfoRow {
                text: qsTr("Duration")
                value: dialog.value("duration") ? App.formatTime(dialog.value("duration")) : ""
                visible: value.length > 0
            }
        }

        SectionLabel {
            text: qsTr("Video")
            visible: videoCard.visible
        }

        ListCard {
            id: videoCard
            visible: dialog.value("video-codec") !== ""

            InfoRow {
                text: qsTr("Codec")
                value: dialog.value("video-codec")
            }
            InfoRow {
                text: qsTr("Resolution")
                value: dialog.value("width") ? dialog.value("width") + " × " + dialog.value("height") : ""
                visible: value.length > 0
            }
            InfoRow {
                text: qsTr("Frame rate")
                value: dialog.value("container-fps") ? qsTr("%1 fps").arg(Number(dialog.value("container-fps")).toLocaleString(Qt.locale(), "f", 2)) : ""
                visible: value.length > 0
            }
            InfoRow {
                text: qsTr("Bit rate")
                value: dialog.bitrate("video-bitrate")
                visible: value.length > 0
            }
        }

        SectionLabel {
            text: qsTr("Audio")
            visible: audioCard.visible
        }

        ListCard {
            id: audioCard
            visible: dialog.value("audio-codec-name") !== ""

            InfoRow {
                text: qsTr("Codec")
                value: dialog.value("audio-codec-name")
            }
            InfoRow {
                text: qsTr("Channels")
                value: dialog.value("audio-params/channel-count")
                visible: String(value).length > 0
            }
            InfoRow {
                text: qsTr("Sample rate")
                value: dialog.value("audio-params/samplerate") ? qsTr("%1 Hz").arg(dialog.value("audio-params/samplerate")) : ""
                visible: value.length > 0
            }
            InfoRow {
                text: qsTr("Bit rate")
                value: dialog.bitrate("audio-bitrate")
                visible: value.length > 0
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: dialog.close()
    }
}
