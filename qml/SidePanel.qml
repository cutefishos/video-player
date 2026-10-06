// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// What does not fit the floating controls, over the right side of the video:
// the playlist, the audio and subtitle tracks, and playback settings.
Item {
    id: control
    objectName: "sidePanel"

    required property var root
    required property var player
    // 0 playlist, 1 tracks, 2 playback.
    property int tab: 0
    readonly property bool busy: dragging || listView.activeFocus
    property bool dragging: false
    readonly property var tabNames: [qsTr("Playlist"), qsTr("Tracks"), qsTr("Playback")]

    function focusList() {
        if (listView.currentIndex < 0 && listView.count > 0)
            listView.currentIndex = 0
        listView.forceActiveFocus()
    }

    VideoGlass {
        anchors.fill: parent
        source: control.player
        radius: 20
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: function (wheel) { wheel.accepted = true }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // The tabs, as a segmented row.
        FishUI.RoundedRectangle {
            continuousCorners: false
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            radius: FishUI.Theme.smallRadius
            color: Qt.rgba(1, 1, 1, 0.08)

            Row {
                id: tabRow
                anchors.fill: parent
                anchors.margins: 2

                Repeater {
                    model: control.tabNames

                    Item {
                        id: tabItem
                        required property int index
                        required property string modelData
                        objectName: "panelTab" + index
                        width: tabRow.width / control.tabNames.length
                        height: tabRow.height

                        FishUI.RoundedRectangle {
                            continuousCorners: false
                            anchors.fill: parent
                            radius: FishUI.Theme.smallRadius - 2
                            color: Qt.rgba(1, 1, 1, control.tab === tabItem.index ? 0.2 : tabHover.hovered ? 0.06 : 0)
                        }

                        FishUI.Label {
                            anchors.centerIn: parent
                            text: tabItem.modelData
                            color: "white"
                            font.weight: control.tab === tabItem.index ? Font.Medium : Font.Normal
                        }

                        HoverHandler {
                            id: tabHover
                        }
                        TapHandler {
                            onTapped: control.root.panelTab = tabItem.index
                        }
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: control.tab

            // Playlist
            ColumnLayout {
                spacing: 4

                ListView {
                    id: listView
                    objectName: "playlistView"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.playlist
                    currentIndex: App.playlist.currentIndex
                    boundsBehavior: Flickable.StopAtBounds
                    keyNavigationEnabled: true

                    FishUI.WheelHandler {
                        target: listView
                    }
                    FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

                    Keys.onDeletePressed: if (currentIndex >= 0) App.playlist.remove(currentIndex)
                    Keys.onReturnPressed: if (currentIndex >= 0) control.root.playIndex(currentIndex)
                    Keys.onEnterPressed: if (currentIndex >= 0) control.root.playIndex(currentIndex)
                    Keys.onEscapePressed: control.root.togglePanel(0, false)

                    delegate: Item {
                        id: row

                        required property int index
                        required property string title
                        required property bool current

                        width: listView.width
                        height: 36

                        FishUI.RoundedRectangle {
                            continuousCorners: false
                            anchors.fill: parent
                            radius: FishUI.Theme.mediumRadius
                            color: row.current ? Qt.alpha(FishUI.Theme.highlightColor, 0.55)
                                               : Qt.rgba(1, 1, 1, rowArea.containsMouse || listView.currentIndex === row.index && listView.activeFocus ? 0.08 : 0)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            Item {
                                Layout.preferredWidth: 18
                                Layout.fillHeight: true

                                FishUI.IconItem {
                                    anchors.centerIn: parent
                                    width: 14
                                    height: 14
                                    visible: row.current
                                    source: "qrc:/icons/play.svg"
                                    color: "white"
                                }
                                FishUI.Label {
                                    anchors.centerIn: parent
                                    visible: !row.current
                                    text: row.index + 1
                                    color: Qt.rgba(1, 1, 1, 0.5)
                                    font.pointSize: 8
                                    font.features: { "tnum": 1 }
                                }
                            }

                            FishUI.Label {
                                Layout.fillWidth: true
                                text: row.title
                                textFormat: Text.PlainText
                                color: "white"
                                elide: Text.ElideMiddle
                            }
                        }

                        MouseArea {
                            id: rowArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            preventStealing: true

                            property real pressY: 0

                            onPressed: function (mouse) {
                                listView.currentIndex = row.index
                                listView.forceActiveFocus()
                                pressY = mouse.y
                                if (mouse.button === Qt.RightButton)
                                    control.root.openPlaylistMenu(row.index)
                            }
                            onDoubleClicked: function (mouse) {
                                if (mouse.button === Qt.LeftButton)
                                    control.root.playIndex(row.index)
                            }
                            onPositionChanged: function (mouse) {
                                if (!pressed || !(pressedButtons & Qt.LeftButton))
                                    return
                                if (!control.dragging && Math.abs(mouse.y - pressY) < 6)
                                    return
                                control.dragging = true
                                const point = mapToItem(listView.contentItem, mouse.x, mouse.y)
                                const target = listView.indexAt(10, point.y)
                                if (target >= 0 && target !== row.index)
                                    App.playlist.move(row.index, target)
                            }
                            onReleased: control.dragging = false
                            onCanceled: control.dragging = false
                        }
                    }
                }

                FishUI.Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: listView.count === 0
                    text: qsTr("Drop files here to add them.")
                    color: Qt.rgba(1, 1, 1, 0.6)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    ControlButton {
                        objectName: "repeatButton"
                        iconName: App.playlist.repeatMode === 1 ? "repeat-one" : "repeat"
                        checked: App.playlist.repeatMode !== 0
                        toolTip: App.playlist.repeatMode === 0 ? qsTr("Repeat Off")
                                 : App.playlist.repeatMode === 1 ? qsTr("Repeat One") : qsTr("Repeat All")
                        onClicked: App.playlist.repeatMode = (App.playlist.repeatMode + 1) % 3
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    ControlButton {
                        objectName: "addFilesButton"
                        iconName: "add"
                        toolTip: qsTr("Add Files…")
                        onClicked: control.root.openFiles(true)
                    }
                }
            }

            // Tracks
            Flickable {
                id: tracksView
                clip: true
                contentHeight: tracksColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                FishUI.WheelHandler {
                    target: tracksView
                }

                ColumnLayout {
                    id: tracksColumn
                    width: tracksView.width
                    spacing: 0

                    PanelSection {
                        Layout.topMargin: 0
                        text: qsTr("Subtitles")
                    }

                    PanelRow {
                        text: qsTr("Off")
                        checkable: true
                        checked: control.player.subtitleTrack === 0
                        onClicked: control.root.selectSubtitle(0)
                    }

                    Repeater {
                        model: control.player.subtitleTracks

                        PanelRow {
                            required property var modelData
                            text: modelData.label
                            checkable: true
                            checked: control.player.subtitleTrack === modelData.id
                            onClicked: control.root.selectSubtitle(modelData.id)
                        }
                    }

                    PanelRow {
                        text: qsTr("Add Subtitle File…")
                        checkable: true
                        onClicked: control.root.openSubtitleFile()
                    }

                    // Timing: earlier, later, and back to none.
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        Layout.leftMargin: 10
                        spacing: 2
                        enabled: control.player.subtitleTrack > 0
                        opacity: enabled ? 1 : 0.45

                        FishUI.Label {
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            text: qsTr("Timing")
                            color: "white"
                        }

                        FishUI.Label {
                            Layout.rightMargin: 4
                            text: control.root.delayText(control.player.subtitleDelay)
                            color: Qt.rgba(1, 1, 1, 0.55)
                            font.features: { "tnum": 1 }
                        }

                        ControlButton {
                            implicitWidth: 30
                            implicitHeight: 30
                            iconSize: 16
                            iconName: "minus"
                            toolTip: qsTr("Show Earlier")
                            onClicked: control.root.shiftSubtitles(-0.1)
                        }

                        ControlButton {
                            implicitWidth: 30
                            implicitHeight: 30
                            iconSize: 16
                            iconName: "add"
                            toolTip: qsTr("Show Later")
                            onClicked: control.root.shiftSubtitles(0.1)
                        }
                    }

                    PanelSection {
                        text: qsTr("Audio")
                    }

                    PanelRow {
                        text: qsTr("No Audio Tracks")
                        enabled: false
                        visible: control.player.audioTracks.length === 0
                    }

                    Repeater {
                        model: control.player.audioTracks

                        PanelRow {
                            required property var modelData
                            text: modelData.label
                            checkable: true
                            checked: control.player.audioTrack === modelData.id
                            onClicked: control.root.selectAudio(modelData.id)
                        }
                    }
                }
            }

            // Playback
            Flickable {
                id: playbackView
                clip: true
                contentHeight: playbackColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                FishUI.WheelHandler {
                    target: playbackView
                }

                ColumnLayout {
                    id: playbackColumn
                    width: playbackView.width
                    spacing: 0

                    PanelSection {
                        Layout.topMargin: 0
                        text: qsTr("Speed")
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10
                        Layout.topMargin: 4
                        Layout.bottomMargin: 4
                        spacing: 6

                        Repeater {
                            model: control.root.speeds

                            PillButton {
                                required property real modelData
                                text: control.root.speedText(modelData)
                                prominent: Math.abs(control.player.speed - modelData) < 0.001
                                onClicked: control.root.setSpeed(modelData)
                            }
                        }
                    }

                    PanelSection {
                        text: qsTr("Options")
                    }

                    PanelRow {
                        text: qsTr("Repeat")
                        checkable: true
                        checked: control.player.loopFile
                        onClicked: control.player.loopFile = !control.player.loopFile
                    }

                    PanelRow {
                        text: qsTr("Rotate Left")
                        checkable: true
                        enabled: control.player.hasVideo
                        onClicked: control.player.rotation = control.player.rotation - 90
                    }

                    PanelRow {
                        text: qsTr("Rotate Right")
                        checkable: true
                        enabled: control.player.hasVideo
                        onClicked: control.player.rotation = control.player.rotation + 90
                    }

                    PanelSection {
                        text: qsTr("Chapters")
                        visible: control.player.chapters.length > 0
                    }

                    Repeater {
                        model: control.player.chapters

                        PanelRow {
                            required property var modelData
                            required property int index
                            text: modelData.title || qsTr("Chapter %1").arg(index + 1)
                            detail: App.formatTime(modelData.time, control.player.duration)
                            checkable: true
                            checked: control.player.chapter === index
                            onClicked: control.player.chapter = index
                        }
                    }
                }
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: function (drop) {
            if (drop.hasUrls) {
                App.playlist.append(drop.urls)
                drop.acceptProposedAction()
            }
        }
    }
}
