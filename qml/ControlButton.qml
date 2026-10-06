// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import FishUI as FishUI

// A glyph button of the overlay controls: white on the dark plate, with a
// round hover plate behind it.
AbstractButton {
    id: control

    property string iconName
    property int iconSize: 20
    property string toolTip

    implicitWidth: 36
    implicitHeight: 36
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4

    Accessible.name: control.toolTip

    background: FishUI.RoundedRectangle {
        continuousCorners: false
        radius: height / 2
        color: "white"
        opacity: control.pressed ? 0.22 : control.hovered || control.checked ? 0.12 : 0
    }

    contentItem: Item {
        FishUI.IconItem {
            anchors.centerIn: parent
            width: control.iconSize
            height: control.iconSize
            source: control.iconName ? "qrc:/icons/" + control.iconName + ".svg" : ""
            color: "white"
        }
    }

    FishUI.ToolTip {
        visible: control.hovered && control.toolTip.length > 0 && !control.pressed
        text: control.toolTip
        delay: 800
    }
}
