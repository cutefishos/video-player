// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import FishUI as FishUI

FishUI.DialogWindow {
    id: dialog
    objectName: "preferencesDialog"

    // ISO 639-1 codes; the names are the languages' own.
    readonly property var languages: [
        { code: "", name: qsTr("Display Language") },
        { code: "en", name: "English" },
        { code: "zh", name: "中文" },
        { code: "ja", name: "日本語" },
        { code: "ko", name: "한국어" },
        { code: "es", name: "Español" },
        { code: "fr", name: "Français" },
        { code: "de", name: "Deutsch" },
        { code: "it", name: "Italiano" },
        { code: "pt", name: "Português" },
        { code: "ru", name: "Русский" }
    ]
    readonly property var subtitleSizes: [
        { scale: 80, name: qsTr("Small") },
        { scale: 100, name: qsTr("Medium") },
        { scale: 125, name: qsTr("Large") },
        { scale: 150, name: qsTr("Extra Large") }
    ]

    title: qsTr("Settings")
    width: 460
    // The layout's 4px top inset, and the same 16px below as at the sides.
    contentHeight: Math.ceil(layout.implicitHeight) + 4 + 16
    blurEnabled: false
    background.color: FishUI.Theme.backgroundColor
    background.opacity: 1

    function indexOf(list, key, value) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i][key] === value)
                return i
        }
        return 0
    }

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        anchors.topMargin: 4
        spacing: 12

        SectionLabel {
            text: qsTr("Playback")
        }

        ListCard {
            PreferenceRow {
                text: qsTr("Resume where you left off")

                FishUI.Switch {
                    objectName: "resumeSwitch"
                    checked: App.settings.resumePlayback
                    rightPadding: 0
                    rightInset: 0
                    onToggled: App.settings.resumePlayback = checked
                }
            }

            PreferenceRow {
                text: qsTr("Play the other videos in the folder")

                FishUI.Switch {
                    objectName: "folderPlaylistSwitch"
                    checked: App.settings.folderPlaylist
                    rightPadding: 0
                    rightInset: 0
                    onToggled: App.settings.folderPlaylist = checked
                }
            }

            PreferenceRow {
                text: qsTr("Audio language")

                FishUI.ComboBox {
                    Layout.preferredWidth: 180
                    model: dialog.languages
                    textRole: "name"
                    currentIndex: dialog.indexOf(dialog.languages, "code", App.settings.audioLanguage)
                    onActivated: function (index) { App.settings.audioLanguage = dialog.languages[index].code }
                }
            }
        }

        SectionLabel {
            text: qsTr("Subtitles")
        }

        ListCard {
            PreferenceRow {
                text: qsTr("Subtitle language")

                FishUI.ComboBox {
                    Layout.preferredWidth: 180
                    model: dialog.languages
                    textRole: "name"
                    currentIndex: dialog.indexOf(dialog.languages, "code", App.settings.subtitleLanguage)
                    onActivated: function (index) { App.settings.subtitleLanguage = dialog.languages[index].code }
                }
            }

            PreferenceRow {
                text: qsTr("Subtitle size")

                FishUI.ComboBox {
                    Layout.preferredWidth: 180
                    model: dialog.subtitleSizes
                    textRole: "name"
                    currentIndex: dialog.indexOf(dialog.subtitleSizes, "scale", App.settings.subtitleScale)
                    onActivated: function (index) { App.settings.subtitleScale = dialog.subtitleSizes[index].scale }
                }
            }
        }

        SectionLabel {
            text: qsTr("Screenshots")
        }

        ListCard {
            PreferenceRow {
                text: qsTr("Save to")

                FishUI.Label {
                    Layout.maximumWidth: 200
                    text: App.settings.screenshotFolder.replace(/^.*\//, "")
                    color: FishUI.Theme.disabledTextColor
                    elide: Text.ElideMiddle
                }

                FishUI.Button {
                    text: qsTr("Change…")
                    onClicked: folderDialog.open()
                }
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Save Screenshots To")
        currentFolder: "file://" + App.settings.screenshotFolder
        onAccepted: App.settings.screenshotFolder = decodeURIComponent(selectedFolder.toString().replace("file://", ""))
    }

    Shortcut {
        sequence: "Escape"
        onActivated: dialog.close()
    }
}
