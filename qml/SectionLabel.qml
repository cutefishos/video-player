// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// Names the card below it; padded to the card's text edge.
FishUI.Label {
    Layout.fillWidth: true
    leftPadding: 16
    rightPadding: 16
    textFormat: Text.PlainText
    color: FishUI.Theme.disabledTextColor
    elide: Text.ElideRight
}
