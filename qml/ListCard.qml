// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// A card of InfoRows: no inner padding, the rows carry their own.
FishUI.RoundedRectangle {
    default property alias content: _layout.data

    Layout.fillWidth: true
    implicitHeight: _layout.implicitHeight
    // Large and continuous, as the floating panels.
    radius: FishUI.Theme.bigRadius
    color: FishUI.Theme.secondBackgroundColor

    ColumnLayout {
        id: _layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 0
    }
}
