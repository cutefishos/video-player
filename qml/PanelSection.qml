// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// Names a group of side panel rows; inset to the rows' text.
FishUI.Label {
    Layout.fillWidth: true
    Layout.topMargin: 8
    leftPadding: 10
    rightPadding: 10
    color: Qt.rgba(1, 1, 1, 0.55)
    font.pointSize: 8
    font.weight: Font.Medium
    textFormat: Text.PlainText
}
