// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// The floating control panel: a fixed-width plate of blurred video centred at
// the bottom. Volume on the left, the transport in the middle, the side panel,
// picture in picture and full screen on the right; the timeline below.
// Tracks, speed and the rest live in the side panel and the menus.
Item {
    id: control
    objectName: "controlBar"

    required property var root
    required property var player
    property bool showRemaining: false
    readonly property bool busy: seekBar.dragging || volume.dragging
    // Free space around the plate's rows.
    readonly property int sidePadding: 14
    readonly property int topPadding: 10
    readonly property int bottomPadding: 12

    implicitWidth: 460
    implicitHeight: layout.implicitHeight + topPadding + bottomPadding

    VideoGlass {
        objectName: "controlGlass"
        anchors.fill: parent
        source: control.player
        radius: 20
    }

    // Presses on the plate stay off the video below it.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: function (wheel) { wheel.accepted = true }
    }

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: control.sidePadding
        anchors.rightMargin: control.sidePadding
        anchors.topMargin: control.topPadding
        spacing: 2

        // Equal side columns keep the transport centred on the plate.
        RowLayout {
            id: buttonRow
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            spacing: 0

            readonly property real side: Math.max(volume.implicitWidth, trailing.implicitWidth)

            Item {
                Layout.preferredWidth: buttonRow.side
                Layout.fillHeight: true

                VolumeControl {
                    id: volume
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    volume: control.player.volume
                    muted: control.player.muted
                    onVolumeRequested: function (value) { control.root.setVolume(value) }
                    onMuteToggled: control.root.toggleMute()
                }
            }

            Item {
                Layout.fillWidth: true
            }

            RowLayout {
                id: transport
                spacing: 6

                ControlButton {
                    objectName: "skipBackButton"
                    iconName: "skip-back"
                    enabled: control.player.seekable
                    toolTip: qsTr("Back 10 Seconds")
                    onClicked: control.root.seekBy(-10)
                }

                ControlButton {
                    objectName: "playButton"
                    implicitWidth: 40
                    implicitHeight: 40
                    iconSize: 24
                    iconName: control.player.endReached ? "replay" : control.player.paused ? "play" : "pause"
                    toolTip: control.player.endReached ? qsTr("Play Again")
                             : control.player.paused ? qsTr("Play") : qsTr("Pause")
                    onClicked: control.root.togglePause()
                }

                ControlButton {
                    objectName: "skipForwardButton"
                    iconName: "skip-forward"
                    enabled: control.player.seekable
                    toolTip: qsTr("Forward 10 Seconds")
                    onClicked: control.root.seekBy(10)
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Item {
                Layout.preferredWidth: buttonRow.side
                Layout.fillHeight: true

                RowLayout {
                    id: trailing
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    ControlButton {
                        objectName: "sidePanelButton"
                        iconName: "sidebar"
                        toolTip: qsTr("Playlist, Tracks and Playback")
                        checked: control.root.panelOpen
                        onClicked: control.root.togglePanel(-1, false)
                    }

                    ControlButton {
                        objectName: "pipButton"
                        iconName: "pip"
                        toolTip: qsTr("Picture in Picture")
                        enabled: control.player.hasVideo
                        onClicked: control.root.setMiniMode(true)
                    }

                    ControlButton {
                        objectName: "fullscreenButton"
                        iconName: control.root.isFullScreen ? "fullscreen-exit" : "fullscreen"
                        toolTip: control.root.isFullScreen ? qsTr("Exit Full Screen") : qsTr("Full Screen")
                        onClicked: control.root.toggleFullScreen()
                    }
                }
            }
        }

        // The times line up with the outer glyphs above them.
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            spacing: 10

            FishUI.Label {
                objectName: "elapsedLabel"
                Layout.minimumWidth: totalMetrics.width
                text: App.formatTime(seekBar.shownPosition, control.player.duration)
                color: "white"
                font.pointSize: 8
                font.features: { "tnum": 1 }
                horizontalAlignment: Text.AlignLeft
            }

            SeekBar {
                id: seekBar
                objectName: "seekBar"
                Layout.fillWidth: true
                duration: control.player.duration
                position: control.player.position
                chapters: control.player.chapters
                thumbnailer: App.thumbnailer
                enabledSeek: control.player.seekable && control.player.duration > 0
                onSeekRequested: function (seconds, exact) { control.player.seek(seconds, exact) }
            }

            FishUI.Label {
                objectName: "totalLabel"
                Layout.minimumWidth: totalMetrics.width
                text: control.showRemaining
                      ? "-" + App.formatTime(Math.max(0, control.player.duration - seekBar.shownPosition), control.player.duration)
                      : App.formatTime(control.player.duration, control.player.duration)
                color: "white"
                font.pointSize: 8
                font.features: { "tnum": 1 }
                horizontalAlignment: Text.AlignRight

                TapHandler {
                    onTapped: control.showRemaining = !control.showRemaining
                }
            }

            TextMetrics {
                id: totalMetrics
                font.pointSize: 8
                font.features: { "tnum": 1 }
                text: "-" + App.formatTime(control.player.duration, control.player.duration)
            }
        }
    }
}
