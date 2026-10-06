// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import FishUI as FishUI

FishUI.DesktopMenu {
    id: control

    required property var root

    Repeater {
        model: control.root.player.chapters

        FishUI.MenuItem {
            required property var modelData
            required property int index
            text: (modelData.title || qsTr("Chapter %1").arg(index + 1)) + "    " + App.formatTime(modelData.time, control.root.player.duration)
            checked: control.root.player.chapter === index
            onTriggered: control.root.player.chapter = index
        }
    }
}
