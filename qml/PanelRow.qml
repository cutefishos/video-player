// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// A row of the side panel: an optional check mark, the text, and an optional
// detail on the right. White on the panel's dark glass.
Item {
    id: control

    property string text
    property string detail
    property bool checkable: false
    property bool checked: false
    // Highlighted as the item being played, e.g. the current playlist entry.
    property bool current: false
    readonly property bool hovered: mouseArea.containsMouse

    signal clicked()
    signal doubleClicked()
    signal rightClicked()

    implicitHeight: 34
    Layout.fillWidth: true
    opacity: enabled ? 1 : 0.45

    FishUI.RoundedRectangle {
        continuousCorners: false
        anchors.fill: parent
        radius: FishUI.Theme.mediumRadius
        color: control.current ? Qt.alpha(FishUI.Theme.highlightColor, 0.55)
                               : Qt.rgba(1, 1, 1, mouseArea.pressed ? 0.14 : control.hovered ? 0.08 : 0)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        Item {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            visible: control.checkable

            FishUI.IconItem {
                anchors.fill: parent
                visible: control.checked
                source: "qrc:/icons/check.svg"
                color: "white"
            }
        }

        FishUI.Label {
            Layout.fillWidth: true
            text: control.text
            textFormat: Text.PlainText
            color: "white"
            elide: Text.ElideMiddle
        }

        FishUI.Label {
            visible: text.length > 0
            text: control.detail
            color: Qt.rgba(1, 1, 1, 0.55)
            font.features: { "tnum": 1 }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: function (mouse) {
            if (mouse.button === Qt.RightButton)
                control.rightClicked()
            else
                control.clicked()
        }
        onDoubleClicked: function (mouse) {
            if (mouse.button === Qt.LeftButton)
                control.doubleClicked()
        }
    }
}
