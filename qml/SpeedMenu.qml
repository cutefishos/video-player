// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import FishUI as FishUI

FishUI.DesktopMenu {
    id: control

    required property var root

    Repeater {
        model: control.root.speeds

        FishUI.MenuItem {
            required property real modelData
            text: modelData === 1 ? qsTr("Normal") : control.root.speedText(modelData)
            checked: Math.abs(control.root.player.speed - modelData) < 0.001
            onTriggered: control.root.setSpeed(modelData)
        }
    }
}
