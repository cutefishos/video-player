// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// The speaker button mutes, its wheel and the slider set the volume. Past the
// notch at 100% the slider boosts up to 150%, and only a drag goes there.
RowLayout {
    id: control

    property int volume: 100
    property bool muted: false
    readonly property int maximum: 150
    readonly property bool dragging: sliderArea.pressed
    property bool sliderVisible: true

    signal volumeRequested(int volume)
    signal muteToggled()

    spacing: 2

    ControlButton {
        objectName: "muteButton"
        iconName: control.muted || control.volume === 0 ? "volume-muted"
                  : control.volume < 34 ? "volume-low"
                  : control.volume < 67 ? "volume-medium" : "volume-high"
        toolTip: control.muted ? qsTr("Unmute") : qsTr("Mute")
        onClicked: control.muteToggled()

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function (event) {
                const step = event.angleDelta.y > 0 ? 5 : event.angleDelta.y < 0 ? -5 : 0
                if (step !== 0)
                    control.volumeRequested(Math.max(0, Math.min(Math.max(100, control.volume), control.volume + step)))
            }
        }
    }

    Item {
        id: slider
        objectName: "volumeSlider"
        visible: control.sliderVisible
        Layout.preferredWidth: 72
        Layout.preferredHeight: 36

        readonly property real shown: control.muted ? 0 : control.volume / control.maximum
        readonly property real notch: 100 / control.maximum

        Item {
            id: track
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            height: 4

            FishUI.RoundedRectangle {
                continuousCorners: false
                    anchors.fill: parent
                radius: 2
                color: Qt.rgba(1, 1, 1, 0.25)
            }

            FishUI.RoundedRectangle {
                continuousCorners: false
                    width: Math.max(height, track.width * Math.min(slider.shown, slider.notch))
                height: parent.height
                radius: 2
                color: "white"
                visible: slider.shown > 0
            }

            // The boost above 100% is drawn in a warning colour.
            FishUI.RoundedRectangle {
                continuousCorners: false
                    x: track.width * slider.notch
                width: track.width * Math.max(0, slider.shown - slider.notch)
                height: parent.height
                radius: 2
                color: FishUI.Theme.orangeColor
                visible: slider.shown > slider.notch
            }

            FishUI.RoundedRectangle {
                continuousCorners: false
                    x: Math.round(track.width * slider.notch) - 1
                anchors.verticalCenter: parent.verticalCenter
                width: 2
                height: 10
                radius: 1
                color: Qt.rgba(1, 1, 1, 0.6)
            }

            FishUI.RoundedRectangle {
                continuousCorners: false
                width: 12
                height: 12
                radius: 6
                color: "white"
                anchors.verticalCenter: parent.verticalCenter
                x: track.width * slider.shown - width / 2
                visible: sliderArea.containsMouse || sliderArea.pressed
            }
        }

        MouseArea {
            id: sliderArea
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true

            function apply(x) {
                var value = Math.round(Math.max(0, Math.min(1, (x - track.x) / track.width)) * control.maximum)
                // The notch holds a little, so 100% is easy to hit.
                if (Math.abs(value - 100) <= 4)
                    value = 100
                control.volumeRequested(value)
            }

            onPressed: function (mouse) { apply(mouse.x) }
            onPositionChanged: function (mouse) { if (pressed) apply(mouse.x) }
        }

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function (event) {
                const step = event.angleDelta.y > 0 ? 5 : event.angleDelta.y < 0 ? -5 : 0
                if (step !== 0)
                    control.volumeRequested(Math.max(0, Math.min(Math.max(100, control.volume), control.volume + step)))
            }
        }
    }
}
