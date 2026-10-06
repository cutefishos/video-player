// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI

// The window before anything is opened: how to open a video, and the
// recently watched ones.
Item {
    id: control
    objectName: "emptyPage"

    signal openFileRequested()
    signal openUrlRequested()
    signal recentRequested(url url)

    // Re-read on each showing, which leaves out files deleted since.
    readonly property var recent: visible ? App.settings.recentFiles : []

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 440)
        spacing: 0

        FishUI.IconItem {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            source: "multimedia-video-player"
        }

        FishUI.Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 16
            text: qsTr("Drop a Video Here")
            color: "white"
            font.pointSize: 15
            font.weight: Font.Medium
        }

        FishUI.Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 6
            text: qsTr("Or open a file or a web address to start watching.")
            color: Qt.rgba(1, 1, 1, 0.65)
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 20
            spacing: 8

            PillButton {
                objectName: "openFileButton"
                text: qsTr("Open File…")
                prominent: true
                onClicked: control.openFileRequested()
            }

            PillButton {
                objectName: "openUrlButton"
                text: qsTr("Open URL…")
                onClicked: control.openUrlRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 32
            Layout.bottomMargin: 6
            visible: control.recent.length > 0

            SectionLabel {
                Layout.fillWidth: true
                text: qsTr("Recent")
                color: Qt.rgba(1, 1, 1, 0.5)
            }

            FishUI.Label {
                Layout.rightMargin: 16
                text: qsTr("Clear")
                color: clearHover.hovered ? Qt.lighter(FishUI.Theme.highlightColor, 1.2) : FishUI.Theme.highlightColor

                HoverHandler {
                    id: clearHover
                }
                TapHandler {
                    onTapped: App.settings.clearRecentFiles()
                }
            }
        }

        ListCard {
            objectName: "recentList"
            visible: control.recent.length > 0
            color: Qt.rgba(1, 1, 1, 0.08)

            Repeater {
                model: control.recent.slice(0, 5)

                RecentRow {
                    required property var modelData
                    file: modelData
                    onClicked: control.recentRequested(modelData.url)
                }
            }
        }
    }
}
