// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FishUI as FishUI

// A recently played file in a ListCard: its name and folder, and where it
// would resume.
ItemDelegate {
    id: control

    readonly property bool isInfoRow: true
    property var file: ({})
    readonly property real resumeAt: App.resume.position(file.url)
    readonly property bool lastItem: {
        var rows = parent ? parent.children : []
        for (var i = rows.length - 1; i >= 0; --i) {
            if (rows[i].isInfoRow && rows[i].visible)
                return rows[i] === control
        }
        return true
    }

    Layout.fillWidth: true
    implicitHeight: 52
    padding: 0
    leftPadding: 16
    rightPadding: 16
    hoverEnabled: true

    background: FishUI.RoundedRectangle {
        continuousCorners: false
        anchors.fill: parent
        anchors.margins: 4
        radius: FishUI.Theme.mediumRadius
        color: Qt.rgba(1, 1, 1, control.pressed ? 0.12 : 0.06)
        visible: control.hovered || control.pressed
    }

    contentItem: RowLayout {
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            FishUI.Label {
                Layout.fillWidth: true
                text: control.file.name || ""
                textFormat: Text.PlainText
                color: "white"
                elide: Text.ElideMiddle
            }
            FishUI.Label {
                Layout.fillWidth: true
                text: control.file.folder || ""
                textFormat: Text.PlainText
                color: Qt.rgba(1, 1, 1, 0.5)
                font.pointSize: 9
                elide: Text.ElideMiddle
            }
        }

        FishUI.Label {
            visible: control.resumeAt > 0
            text: App.formatTime(control.resumeAt)
            color: Qt.rgba(1, 1, 1, 0.5)
            font.features: { "tnum": 1 }
        }
    }

    Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        color: "white"
        opacity: 0.12
        visible: !control.lastItem
    }
}
