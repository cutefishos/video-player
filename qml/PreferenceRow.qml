// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// One row of a ListCard: the label, then its control on the right.
Item {
    id: control

    readonly property bool isInfoRow: true
    property string text
    default property alias content: _row.data
    readonly property bool lastItem: {
        var rows = parent ? parent.children : []
        for (var i = rows.length - 1; i >= 0; --i) {
            if (rows[i].isInfoRow && rows[i].visible)
                return rows[i] === control
        }
        return true
    }

    Layout.fillWidth: true
    implicitHeight: 48

    RowLayout {
        id: _row
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        FishUI.Label {
            Layout.fillWidth: true
            text: control.text
            textFormat: Text.PlainText
            elide: Text.ElideRight
        }
    }

    Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        visible: !control.lastItem
    }
}
