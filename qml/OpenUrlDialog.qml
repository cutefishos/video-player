// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// Asks for a web address, filled in from the clipboard when it holds one.
FishUI.Window {
    id: control
    objectName: "openUrlDialog"

    signal accepted(url url)

    readonly property url enteredUrl: App.urlFromInput(field.text)
    readonly property int dialogWidth: 420
    readonly property int dialogHeight: layout.implicitHeight + 16 + 12

    title: qsTr("Open URL")
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.WindowModal
    visible: false
    minimizeButtonVisible: false
    contentTopMargin: 0
    windowButtonsTopMargin: 8
    windowButtonsRightMargin: 8
    header.height: 44
    blurEnabled: false
    restorationBehavior: FishUI.Window.Disabled
    background.color: FishUI.Theme.secondBackgroundColor
    color: background.color
    width: dialogWidth
    height: dialogHeight
    minimumWidth: dialogWidth
    maximumWidth: dialogWidth
    minimumHeight: dialogHeight
    maximumHeight: dialogHeight

    function open() {
        field.text = App.clipboardUrl()
        field.selectAll()
        show()
        raise()
        requestActivate()
        field.forceActiveFocus()
    }

    function accept() {
        if (enteredUrl.toString().length === 0)
            return
        accepted(enteredUrl)
        close()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: control.close()
    }

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 16

        FishUI.Label {
            Layout.rightMargin: 32
            text: control.title
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        FishUI.TextField {
            id: field
            objectName: "urlField"
            Layout.fillWidth: true
            placeholderText: "https://"
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
            onAccepted: control.accept()
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8

            FishUI.ConfirmDialogButton {
                text: qsTr("Cancel")
                onClicked: control.close()
            }

            FishUI.ConfirmDialogButton {
                text: qsTr("Open")
                primary: true
                enabled: control.enteredUrl.toString().length > 0
                onClicked: control.accept()
            }
        }
    }
}
