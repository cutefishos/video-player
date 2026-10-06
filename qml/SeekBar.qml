// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import FishUI as FishUI

// The timeline: a press jumps there, a drag scrubs on keyframes and lands
// exactly on release. Hovering shows the frame and time under the pointer.
Item {
    id: control

    property real duration: 0
    property real position: 0
    property var chapters: []
    property var thumbnailer: null
    property bool enabledSeek: duration > 0
    readonly property bool dragging: mouseArea.pressed
    readonly property bool hovered: mouseArea.containsMouse
    // What the bar shows: the drag while scrubbing, else the player.
    readonly property real shownPosition: dragging ? dragTime : position
    property real dragTime: 0

    signal seekRequested(real seconds, bool exact)

    implicitHeight: 20

    function timeAt(x) {
        return Math.max(0, Math.min(1, x / width)) * duration
    }

    function chapterAt(seconds) {
        var title = ""
        for (var i = 0; i < chapters.length; ++i) {
            if (chapters[i].time <= seconds)
                title = chapters[i].title || ""
        }
        return title
    }

    Item {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: control.hovered || control.dragging ? 6 : 4

        Behavior on height {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        FishUI.RoundedRectangle {
            continuousCorners: false
            anchors.fill: parent
            radius: height / 2
            color: Qt.rgba(1, 1, 1, 0.25)
        }

        FishUI.RoundedRectangle {
            continuousCorners: false
            width: control.duration > 0 ? Math.max(height, parent.width * Math.min(1, control.shownPosition / control.duration)) : 0
            height: parent.height
            radius: height / 2
            color: "white"
            visible: control.duration > 0
        }

        // Chapter boundaries cut thin gaps into the track.
        Repeater {
            model: control.duration > 0 ? control.chapters : []

            FishUI.RoundedRectangle {
                continuousCorners: false
                    required property var modelData
                visible: modelData.time > 0
                x: Math.round(track.width * modelData.time / control.duration) - width / 2
                width: 2
                height: track.height
                color: "#000000"
                opacity: 0.55
            }
        }
    }

    FishUI.RoundedRectangle {
        continuousCorners: false
        id: handle
        width: 14
        height: 14
        radius: 7
        color: "white"
        anchors.verticalCenter: parent.verticalCenter
        x: control.duration > 0 ? track.width * Math.min(1, control.shownPosition / control.duration) - width / 2 : -width / 2
        visible: control.duration > 0
        scale: control.hovered || control.dragging ? 1 : 0

        Behavior on scale {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        anchors.topMargin: -6
        anchors.bottomMargin: -6
        hoverEnabled: true
        enabled: control.enabledSeek
        preventStealing: true

        onPressed: function (mouse) {
            control.dragTime = control.timeAt(mouse.x)
            control.seekRequested(control.dragTime, false)
        }
        onPositionChanged: function (mouse) {
            if (!pressed)
                return
            control.dragTime = control.timeAt(mouse.x)
            control.seekRequested(control.dragTime, false)
        }
        onReleased: function (mouse) {
            control.dragTime = control.timeAt(mouse.x)
            control.seekRequested(control.dragTime, true)
        }
    }

    // The preview above the pointer.
    Item {
        id: preview
        objectName: "seekPreview"

        readonly property real time: control.timeAt(mouseArea.mouseX)
        readonly property int bucket: control.thumbnailer ? control.thumbnailer.bucket(time) : -1
        readonly property bool hasFrame: {
            if (!control.thumbnailer)
                return false
            control.thumbnailer.revision
            return bucket >= 0 && control.thumbnailer.has(bucket)
        }
        readonly property string chapter: control.chapterAt(time)
        readonly property real frameWidth: 176

        visible: (control.hovered || control.dragging) && control.duration > 0
        width: Math.max(frameItem.visible ? frameWidth : 0, timeLabel.implicitWidth + 16, chapterLabel.visible ? chapterLabel.implicitWidth + 16 : 0)
        height: column.height
        x: Math.round(Math.max(-8, Math.min(control.width - width + 8, mouseArea.mouseX - width / 2)))
        y: -height - 14

        onBucketChanged: if (visible && control.thumbnailer) control.thumbnailer.request(bucket)
        onVisibleChanged: if (visible && control.thumbnailer) control.thumbnailer.request(bucket)

        Column {
            id: column
            width: parent.width
            spacing: 6

            FishUI.RoundedRectangle {
                continuousCorners: false
                id: frameItem
                width: parent.width
                height: Math.round(width * (frameImage.implicitHeight > 0 ? frameImage.implicitHeight / Math.max(1, frameImage.implicitWidth) : 9 / 16))
                visible: preview.hasFrame
                radius: FishUI.Theme.smallRadius
                color: "black"
                border.width: 1 / FishUI.Dpi.ratio
                border.color: Qt.rgba(1, 1, 1, 0.3)
                clip: true

                Image {
                    id: frameImage
                    anchors.fill: parent
                    anchors.margins: parent.border.width
                    fillMode: Image.PreserveAspectFit
                    cache: false
                    smooth: true
                    source: preview.hasFrame ? "image://thumbnail/" + preview.bucket + "/" + control.thumbnailer.revision : ""
                }
            }

            FishUI.RoundedRectangle {
                continuousCorners: false
                anchors.horizontalCenter: parent.horizontalCenter
                width: labels.width + 16
                height: labels.height + 8
                radius: height / 2
                color: Qt.rgba(0.08, 0.08, 0.09, 0.85)

                Column {
                    id: labels
                    anchors.centerIn: parent

                    FishUI.Label {
                        id: chapterLabel
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: text.length > 0
                        text: preview.chapter
                        color: "white"
                        font.pointSize: 9
                        textFormat: Text.PlainText
                    }

                    FishUI.Label {
                        id: timeLabel
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: App.formatTime(preview.time, control.duration)
                        color: "white"
                        font.pointSize: 9
                        font.features: { "tnum": 1 }
                    }
                }
            }
        }
    }
}
