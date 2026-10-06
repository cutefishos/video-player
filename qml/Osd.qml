// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// A short message over the video (volume, seek target, speed, tracks), with
// an optional action such as "Show".
FishUI.RoundedRectangle {
    continuousCorners: false
    id: control
    objectName: "osd"

    property string text
    property string actionText
    property var action: null
    // From the plate's edge to its text when there is no action.
    readonly property real textInset: 12
    readonly property bool hovered: hover.hovered

    width: row.implicitWidth + (actionText.length > 0 ? 8 : textInset * 2)
    height: 36
    radius: height / 2
    color: Qt.rgba(0.09, 0.09, 0.1, 0.78)
    border.width: 1 / FishUI.Dpi.ratio
    border.color: Qt.rgba(1, 1, 1, 0.1)
    opacity: 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation { duration: 180 }
    }

    function show(message, timeout, actionLabel, callback) {
        text = message
        actionText = actionLabel || ""
        action = callback || null
        hideTimer.interval = timeout || (actionText.length > 0 ? 5000 : 1500)
        hideTimer.restart()
        opacity = 1
    }

    function hide() {
        hideTimer.stop()
        opacity = 0
    }

    Timer {
        id: hideTimer
        onTriggered: if (control.hovered) restart(); else control.opacity = 0
    }

    HoverHandler {
        id: hover
    }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 8

        FishUI.Label {
            Layout.leftMargin: control.actionText.length > 0 ? 8 : 0
            text: control.text
            color: "white"
            textFormat: Text.PlainText
            font.features: { "tnum": 1 }
        }

        PillButton {
            visible: control.actionText.length > 0
            text: control.actionText
            onClicked: {
                if (control.action)
                    control.action()
                control.hide()
            }
        }
    }
}
