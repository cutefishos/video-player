// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import FishUI as FishUI

// A text button on the dark overlay plates.
AbstractButton {
    id: control

    property bool prominent: false

    implicitWidth: label.implicitWidth + 24
    implicitHeight: 28
    focusPolicy: Qt.NoFocus
    hoverEnabled: true

    background: FishUI.RoundedRectangle {
        continuousCorners: false
        radius: height / 2
        color: control.prominent ? FishUI.Theme.highlightColor : "white"
        opacity: control.prominent ? (control.pressed ? 0.8 : 1)
                                   : control.pressed ? 0.3 : control.hovered ? 0.24 : 0.16
    }

    contentItem: FishUI.Label {
        id: label
        text: control.text
        color: "white"
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
