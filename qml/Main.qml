// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FishUI as FishUI
import Cutefish.Application
import Cutefish.Media
import Cutefish.VideoPlayer

FishUI.Window {
    id: rootWindow
    objectName: "playerWindow"

    readonly property alias player: player
    readonly property bool hasMedia: player.source.toString().length > 0
    // The file's own title when it has one, else its name without the extension.
    readonly property string mediaName: {
        const fileName = decodeURIComponent(player.source.toString().split("/").pop())
        return player.mediaTitle.length > 0 && player.mediaTitle !== fileName ? player.mediaTitle
                                                                              : App.displayName(player.source)
    }
    property string errorMessage: ""
    // The side panel: playlist, tracks and playback settings.
    property bool panelOpen: false
    property int panelTab: 0
    // Menus are popup windows of their own, made on first use.
    property var mainMenuPopup: null
    property var contextMenuPopup: null
    property var playlistMenuPopup: null
    readonly property bool menuOpen: (!!mainMenuPopup && mainMenuPopup.visible)
                                     || (!!contextMenuPopup && contextMenuPopup.visible)
                                     || (!!playlistMenuPopup && playlistMenuPopup.visible)
    // The pointer moved over the window lately.
    property bool pointerActive: true
    readonly property bool controlsVisible: !hasMedia || errorMessage.length > 0 || player.paused || player.endReached
                                            || pointerActive || menuOpen || controlBar.busy || controlsHover.hovered
                                            || headerHover.hovered || (panelOpen && sidePanel.busy)
    readonly property var speeds: [0.5, 0.75, 1, 1.25, 1.5, 2]
    // Set on load, so the window fits the first frame of each file.
    property bool fitPending: false
    property var pendingSubtitles: []
    // Set on load; the file goes to Recent once it shows a picture or sound.
    property bool recentPending: false
    property int lastSubtitle: 0
    property bool wasMaximized: false

    title: hasMedia ? mediaName : qsTr("Video Player")
    width: 960
    height: 600
    minimumWidth: 480
    minimumHeight: 320
    contentTopMargin: 0
    header.height: 44
    header.visible: chromeOpacity > 0
    header.opacity: chromeOpacity
    windowButtonsTopMargin: (header.height - header.buttonSize) / 2
    // Dark with or without a file, so opening one changes nothing but the picture.
    headerDarkMode: true
    blurEnabled: false
    background.color: "black"
    background.opacity: 1
    // Full screen is the video alone: no window buttons, only the fading title.
    minimizeButtonVisible: !isFullScreen
    closeButtonVisible: !isFullScreen
    WindowHints.resizeAroundCenter: true

    // A window opened with a file starts fitted to it instead of shrinking to
    // it on the first frame.
    property bool launchFitPending: App.launchVideoSize.width > 0 && App.launchVideoSize.height > 0
    visible: false

    Component.onCompleted: {
        fitToLaunch(FishUI.Dpi.ratio)
        visible = true
    }

    // Until the window is shown its ratio is the output's whole one; the
    // fractional scale follows, still ahead of the first frame.
    Connections {
        target: App
        enabled: rootWindow.launchFitPending
        function onWindowRatioChanged(ratio) { rootWindow.fitToLaunch(ratio) }
    }

    Connections {
        target: rootWindow
        enabled: rootWindow.launchFitPending
        function onFrameSwapped() { rootWindow.launchFitPending = false }
    }

    // The title bar fades with the controls.
    property real chromeOpacity: hasMedia && !controlsVisible ? 0 : 1

    Behavior on chromeOpacity {
        NumberAnimation { duration: 200 }
    }

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    headerItem: Item {
        HoverHandler {
            id: headerHover
            enabled: rootWindow.hasMedia
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: rootWindow.windowButtonsSpacing
            spacing: 12

            FishUI.Label {
                objectName: "windowTitle"
                Layout.fillWidth: true
                text: rootWindow.title
                textFormat: Text.PlainText
                font.pointSize: 11
                elide: Text.ElideMiddle
                color: rootWindow.active ? "white" : Qt.rgba(1, 1, 1, 0.6)
            }

            FishUI.HeaderButton {
                id: menuButton
                objectName: "mainMenuButton"
                visible: !rootWindow.isFullScreen
                size: rootWindow.header.buttonSize
                artwork: "menu"
                checked: !!rootWindow.mainMenuPopup && rootWindow.mainMenuPopup.visible
                onClicked: rootWindow.menu("mainMenu").showDropDown(menuButton, 4)
                Accessible.name: qsTr("Main Menu")
            }
        }
    }

    MpvItem {
        id: player
        objectName: "player"
        anchors.fill: parent
        visible: rootWindow.hasMedia && rootWindow.errorMessage.length === 0

        onFileLoaded: {
            rootWindow.rememberRecent()
            rootWindow.fitPending = true
            rootWindow.fitToVideo()
            for (var i = 0; i < rootWindow.pendingSubtitles.length; ++i)
                player.addSubtitle(rootWindow.pendingSubtitles[i])
            rootWindow.pendingSubtitles = []
        }
        onVideoSizeChanged: rootWindow.fitToVideo()
        onTracksChanged: rootWindow.rememberRecent()
        onPlaybackFailed: function (message) {
            rootWindow.errorMessage = message
        }
        onEndReachedChanged: if (endReached) rootWindow.fileEnded()
        onScreenshotSaved: function (path, success) {
            if (success)
                osd.show(qsTr("Screenshot saved"), 4000, qsTr("Show"), function () { App.showInFolder("file://" + path) })
            else
                osd.show(qsTr("Could not save the screenshot"))
        }
        onSubtitleAdded: function (success) {
            if (!success)
                osd.show(qsTr("Could not open the subtitle file"))
        }
        onSubtitleTrackChanged: if (subtitleTrack > 0) rootWindow.lastSubtitle = subtitleTrack
        Component.onCompleted: {
            volume = App.settings.volume
            muted = App.settings.muted
            setSubtitleScale(App.settings.subtitleScale / 100)
            rootWindow.applyLanguages()
        }
    }

    Connections {
        target: App.settings
        function onSubtitleScaleChanged() { player.setSubtitleScale(App.settings.subtitleScale / 100) }
        function onAudioLanguageChanged() { rootWindow.applyLanguages() }
        function onSubtitleLanguageChanged() { rootWindow.applyLanguages() }
    }

    Connections {
        target: App.playlist
        function onCurrentIndexChanged() { rootWindow.loadCurrent() }
    }

    Connections {
        target: App
        function onOpenRequested(media, subtitles) {
            if (media.length > 0) {
                rootWindow.pendingSubtitles = subtitles
                rootWindow.saveResume()
                App.playlist.open(media, media.length === 1 && App.settings.folderPlaylist)
            } else if (rootWindow.hasMedia) {
                for (var i = 0; i < subtitles.length; ++i)
                    player.addSubtitle(subtitles[i])
            }
        }
    }

    Binding {
        target: App.inhibitor
        property: "mode"
        value: player.loaded && !player.paused ? (player.hasVideo ? SleepInhibitor.Screen : SleepInhibitor.Suspend)
                                               : SleepInhibitor.Off
    }

    Binding {
        target: App.thumbnailer
        property: "source"
        value: player.loaded && player.hasVideo ? player.source : ""
    }

    Binding {
        target: App.thumbnailer
        property: "duration"
        value: player.duration
    }

    // Pointer activity over the video.
    HoverHandler {
        id: pointerHover
        parent: rootWindow.contentItem
        cursorShape: rootWindow.controlsVisible ? Qt.ArrowCursor : Qt.BlankCursor
        property point last: Qt.point(-1, -1)
        onPointChanged: {
            // Surface changes resend motion at the same spot; that is not activity.
            if (point.position.x === last.x && point.position.y === last.y)
                return
            last = point.position
            rootWindow.poke()
        }
    }

    Timer {
        id: idleTimer
        interval: 2000
        onTriggered: rootWindow.pointerActive = false
    }

    // The video surface's own gestures.
    Item {
        id: videoGestures
        anchors.fill: parent
        enabled: rootWindow.hasMedia

        property real pendingSeek: 0
        property real wheelVolume: 0

        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.DragThreshold
            onSingleTapped: singleClick.restart()
            onDoubleTapped: {
                singleClick.stop()
                rootWindow.toggleFullScreen()
            }
        }

        // TapHandler reports the first click of a double click at once; it
        // pauses only once no second click follows.
        Timer {
            id: singleClick
            interval: Qt.styleHints.mouseDoubleClickInterval
            onTriggered: rootWindow.togglePause(true)
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: rootWindow.menu("contextMenu").popup()
        }

        TapHandler {
            acceptedButtons: Qt.MiddleButton
            onTapped: rootWindow.toggleMute()
        }

        TapHandler {
            acceptedButtons: Qt.BackButton | Qt.ForwardButton
            onTapped: function (eventPoint, button) { rootWindow.seekBy(button === Qt.BackButton ? -5 : 5) }
        }

        DragHandler {
            target: null
            enabled: !rootWindow.isFullScreen
            grabPermissions: PointerHandler.CanTakeOverFromHandlersOfDifferentType
            onActiveChanged: if (active) rootWindow.controller.startSystemMove()
        }

        // Up and down change the volume; sideways, as a touchpad swipes, seeks.
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function (event) {
                const horizontal = Math.abs(event.angleDelta.x) > Math.abs(event.angleDelta.y)
                if (horizontal) {
                    const delta = event.pixelDelta.x !== 0 ? event.pixelDelta.x / 8 : event.angleDelta.x / 24
                    videoGestures.pendingSeek -= delta
                    if (!seekFlush.running)
                        seekFlush.start()
                } else {
                    videoGestures.wheelVolume += event.angleDelta.y / 120 * 5
                    const step = Math.trunc(videoGestures.wheelVolume)
                    if (step !== 0) {
                        videoGestures.wheelVolume -= step
                        rootWindow.setVolume(Math.max(0, Math.min(Math.max(100, player.volume), player.volume + step)))
                    }
                }
            }
        }

        Timer {
            id: seekFlush
            interval: 80
            onTriggered: {
                const seconds = Math.round(videoGestures.pendingSeek)
                if (seconds !== 0) {
                    videoGestures.pendingSeek -= seconds
                    rootWindow.seekBy(seconds)
                }
            }
        }
    }

    // Shades the top of the picture so the title and window buttons read.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 96
        visible: rootWindow.hasMedia && rootWindow.chromeOpacity > 0
        opacity: rootWindow.chromeOpacity
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.55) }
            GradientStop { position: 1; color: "transparent" }
        }
    }

    EmptyPage {
        anchors.fill: parent
        anchors.topMargin: rootWindow.header.height
        visible: !rootWindow.hasMedia
        onOpenFileRequested: rootWindow.openFiles(false)
        onOpenUrlRequested: rootWindow.openDialog("openUrl")
        onRecentRequested: function (url) { App.openUrls([url]) }
    }

    // A file that cannot be played says why, in place of the picture.
    ColumnLayout {
        objectName: "errorPage"
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        visible: rootWindow.hasMedia && rootWindow.errorMessage.length > 0
        spacing: 8

        FishUI.Label {
            Layout.fillWidth: true
            text: qsTr("Unable to Play This File")
            color: "white"
            font.pointSize: 15
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
        }

        FishUI.Label {
            Layout.fillWidth: true
            text: App.displayName(player.source) + " — " + rootWindow.errorMessage
            textFormat: Text.PlainText
            color: Qt.rgba(1, 1, 1, 0.65)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 12
            spacing: 8

            PillButton {
                text: qsTr("Open Another File…")
                onClicked: rootWindow.openFiles(false)
            }

            PillButton {
                text: qsTr("Next File")
                prominent: true
                visible: App.playlist.hasNext
                onClicked: rootWindow.playNext(false)
            }
        }
    }

    // Shown after a second of buffering, so short stalls do not flicker.
    FishUI.BusyIndicator {
        anchors.centerIn: parent
        width: 40
        height: 40
        running: visible
        visible: rootWindow.hasMedia && bufferingDelay.elapsed
    }

    Timer {
        id: bufferingDelay
        property bool elapsed: false
        interval: 1000
        running: player.buffering && !player.paused
        onRunningChanged: if (!running) elapsed = false
        onTriggered: elapsed = true
    }

    // The play or pause glyph a click on the video flashes.
    FishUI.RoundedRectangle {
        continuousCorners: false
        id: flash
        anchors.centerIn: parent
        width: 72
        height: 72
        radius: 36
        color: Qt.rgba(0, 0, 0, 0.5)
        opacity: 0
        visible: opacity > 0

        property string iconName: "play"

        FishUI.IconItem {
            anchors.centerIn: parent
            width: 32
            height: 32
            source: "qrc:/icons/" + flash.iconName + ".svg"
            color: "white"
        }

        function show(name) {
            iconName = name
            flashAnimation.restart()
        }

        SequentialAnimation {
            id: flashAnimation
            NumberAnimation { target: flash; property: "opacity"; to: 1; duration: 60 }
            PauseAnimation { duration: 300 }
            ParallelAnimation {
                NumberAnimation { target: flash; property: "opacity"; to: 0; duration: 300 }
                NumberAnimation { target: flash; property: "scale"; from: 1; to: 1.3; duration: 300 }
            }
            PropertyAction { target: flash; property: "scale"; value: 1 }
        }
    }

    Osd {
        id: osd
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: rootWindow.header.height + 8
    }

    SidePanel {
        id: sidePanel
        root: rootWindow
        player: player
        tab: rootWindow.panelTab
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.rightMargin: 12
        anchors.topMargin: rootWindow.header.height + 4
        // Above the controls only where the two would meet.
        anchors.bottomMargin: controlBar.x + controlBar.width > x - 12 ? parent.height - controlBar.y + 12 : 16
        width: Math.min(300, parent.width * 0.45)
        visible: rootWindow.panelOpen && rootWindow.hasMedia
    }

    ControlBar {
        id: controlBar
        root: rootWindow
        player: player
        width: Math.min(parent.width - 24, implicitWidth)
        height: implicitHeight
        // Centred on the device pixel grid.
        x: Math.round((parent.width - width) / 2 * FishUI.Dpi.ratio) / FishUI.Dpi.ratio
        y: parent.height - height - 16
        visible: opacity > 0 && rootWindow.hasMedia
        opacity: rootWindow.controlsVisible ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }

        HoverHandler {
            id: controlsHover
        }
    }

    DropArea {
        id: dropArea
        anchors.fill: parent
        onEntered: function (drag) { drag.accepted = drag.hasUrls }
        onDropped: function (drop) {
            if (!drop.hasUrls)
                return
            App.openUrls(drop.urls)
            drop.acceptProposedAction()
            rootWindow.requestActivate()
        }
    }

    FishUI.RoundedRectangle {
        anchors.fill: parent
        anchors.margins: 8
        visible: dropArea.containsDrag
        radius: FishUI.Theme.hugeRadius
        color: Qt.alpha(FishUI.Theme.highlightColor, 0.12)
        border.width: 2
        border.color: FishUI.Theme.highlightColor

        FishUI.Label {
            anchors.centerIn: parent
            text: qsTr("Drop to Play")
            font.pointSize: 14
            font.weight: Font.Medium
            color: "white"
        }
    }

    // Menus

    Component {
        id: mainMenuComponent

        FishUI.DesktopMenu {
            id: mainMenu
            objectName: "mainMenu"

            FishUI.MenuItem {
                text: qsTr("Open File…")
                shortcut: "Ctrl+O"
                onTriggered: rootWindow.openFiles(false)
            }
            FishUI.MenuItem {
                text: qsTr("Open URL…")
                shortcut: "Ctrl+L"
                onTriggered: rootWindow.openDialog("openUrl")
            }
            FishUI.MenuItem {
                text: qsTr("Recent")
                enabled: mainMenu.visible && App.settings.recentFiles.length > 0
                childMenu: FishUI.DesktopMenu {
                    Repeater {
                        model: mainMenu.visible ? App.settings.recentFiles : []

                        FishUI.MenuItem {
                            required property var modelData
                            text: modelData.name
                            onTriggered: App.openUrls([modelData.url])
                        }
                    }
                    FishUI.MenuSeparator {}
                    FishUI.MenuItem {
                        text: qsTr("Clear Recent")
                        onTriggered: App.settings.clearRecentFiles()
                    }
                }
            }

            FishUI.MenuSeparator {}

            FishUI.MenuItem {
                text: qsTr("Show in Files")
                enabled: rootWindow.hasMedia && player.source.toString().startsWith("file:")
                onTriggered: App.showInFolder(player.source)
            }
            FishUI.MenuItem {
                text: qsTr("Take Screenshot")
                shortcut: "Ctrl+Alt+S"
                enabled: player.loaded && player.hasVideo
                onTriggered: rootWindow.takeScreenshot()
            }
            FishUI.MenuItem {
                text: qsTr("Media Info")
                shortcut: "Ctrl+I"
                enabled: player.loaded
                onTriggered: rootWindow.showMediaInfo()
            }

            FishUI.MenuSeparator {}

            FishUI.MenuItem {
                text: qsTr("Settings")
                shortcut: "Ctrl+,"
                onTriggered: rootWindow.openDialog("preferences")
            }
            FishUI.MenuItem {
                text: qsTr("Keyboard Shortcuts")
                shortcut: "Ctrl+?"
                onTriggered: rootWindow.openDialog("shortcuts")
            }
            FishUI.MenuItem {
                text: qsTr("About Video Player")
                onTriggered: rootWindow.openDialog("about")
            }
        }
    }

    Component {
        id: contextMenuComponent

        FishUI.DesktopMenu {
            id: contextMenu
            objectName: "contextMenu"

            FishUI.MenuItem {
                text: player.paused || player.endReached ? qsTr("Play") : qsTr("Pause")
                shortcut: "Space"
                onTriggered: rootWindow.togglePause(false)
            }
            FishUI.MenuItem {
                text: qsTr("Previous")
                shortcut: "B"
                visible: App.playlist.count > 1
                enabled: App.playlist.hasPrevious || player.position > 5
                onTriggered: rootWindow.playPrevious()
            }
            FishUI.MenuItem {
                text: qsTr("Next")
                shortcut: "N"
                visible: App.playlist.count > 1
                enabled: App.playlist.hasNext
                onTriggered: rootWindow.playNext(false)
            }

            FishUI.MenuSeparator {}

            FishUI.MenuItem {
                text: qsTr("Subtitles")
                childMenu: SubtitleMenu {
                    root: rootWindow
                }
            }
            FishUI.MenuItem {
                text: qsTr("Audio")
                childMenu: AudioMenu {
                    root: rootWindow
                }
            }
            FishUI.MenuItem {
                text: qsTr("Speed")
                childMenu: SpeedMenu {
                    root: rootWindow
                }
            }
            FishUI.MenuItem {
                text: qsTr("Chapters")
                visible: player.chapters.length > 0
                childMenu: ChapterMenu {
                    root: rootWindow
                }
            }
            FishUI.MenuItem {
                text: qsTr("Repeat")
                checked: player.loopFile
                onTriggered: player.loopFile = !player.loopFile
            }

            FishUI.MenuSeparator {}

            FishUI.MenuItem {
                text: rootWindow.isFullScreen ? qsTr("Exit Full Screen") : qsTr("Full Screen")
                shortcut: "F"
                onTriggered: rootWindow.toggleFullScreen()
            }
            FishUI.MenuItem {
                text: qsTr("Playlist")
                shortcut: "P"
                checked: rootWindow.panelOpen && rootWindow.panelTab === 0
                onTriggered: rootWindow.togglePanel(0, false)
            }

            FishUI.MenuSeparator {}

            FishUI.MenuItem {
                text: qsTr("Take Screenshot")
                shortcut: "Ctrl+Alt+S"
                enabled: player.loaded && player.hasVideo
                onTriggered: rootWindow.takeScreenshot()
            }
            FishUI.MenuItem {
                text: qsTr("Media Info")
                shortcut: "Ctrl+I"
                enabled: player.loaded
                onTriggered: rootWindow.showMediaInfo()
            }
            FishUI.MenuItem {
                text: qsTr("Show in Files")
                enabled: player.source.toString().startsWith("file:")
                onTriggered: App.showInFolder(player.source)
            }
        }
    }

    Component {
        id: playlistMenuComponent

        FishUI.DesktopMenu {
            id: playlistMenu
            objectName: "playlistMenu"

            property int index: -1

            FishUI.MenuItem {
                text: qsTr("Play")
                onTriggered: rootWindow.playIndex(playlistMenu.index)
            }
            FishUI.MenuItem {
                text: qsTr("Show in Files")
                onTriggered: App.showInFolder(App.playlist.urls[playlistMenu.index])
            }
            FishUI.MenuSeparator {}
            FishUI.MenuItem {
                text: qsTr("Remove from Playlist")
                onTriggered: App.playlist.remove(playlistMenu.index)
            }
        }
    }

    // Dialogs

    FileDialog {
        id: fileDialog
        property bool append: false
        title: append ? qsTr("Add to Playlist") : qsTr("Open Video")
        fileMode: FileDialog.OpenFiles
        currentFolder: App.settings.lastFolder.toString().length > 0 ? App.settings.lastFolder
                                                                      : StandardPaths.writableLocation(StandardPaths.MoviesLocation)
        nameFilters: [qsTr("Videos and audio (%1)").arg(rootWindow.mediaPatterns), qsTr("All files (*)")]
        onAccepted: {
            App.settings.lastFolder = currentFolder
            if (append)
                App.playlist.append(selectedFiles)
            else
                App.openUrls(selectedFiles)
        }
    }

    FileDialog {
        id: subtitleDialog
        title: qsTr("Add Subtitle File")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Subtitles (*.srt *.ass *.ssa *.vtt *.sub *.idx *.sup *.smi)"), qsTr("All files (*)")]
        onAccepted: player.addSubtitle(selectedFile)
    }

    readonly property string mediaPatterns: "*.mp4 *.m4v *.mkv *.webm *.avi *.mov *.wmv *.flv *.mpg *.mpeg *.ts *.m2ts *.mts *.ogv *.3gp *.vob *.rm *.rmvb *.asf *.divx *.f4v *.mp3 *.flac *.ogg *.oga *.opus *.m4a *.aac *.wav *.wma *.ape *.wv *.mka *.m3u *.m3u8 *.pls"

    // Dialogs are windows of their own, made on first use.
    property var dialogs: ({})

    function dialog(name) {
        if (!dialogs[name]) {
            const components = {
                openUrl: openUrlComponent,
                mediaInfo: mediaInfoComponent,
                preferences: preferencesComponent,
                shortcuts: shortcutsComponent,
                about: aboutComponent
            }
            dialogs[name] = components[name].createObject(rootWindow, { parentWindow: rootWindow })
        }
        return dialogs[name]
    }

    function openDialog(name) {
        name === "about" ? dialog(name).show() : dialog(name).open()
    }

    Component {
        id: openUrlComponent
        OpenUrlDialog {
            onAccepted: function (url) { App.openUrls([url]) }
        }
    }

    Component {
        id: mediaInfoComponent
        MediaInfoDialog {}
    }

    Component {
        id: preferencesComponent
        PreferencesDialog {}
    }

    Component {
        id: shortcutsComponent
        ShortcutsDialog {}
    }

    Component {
        id: aboutComponent
        FishUI.AboutDialog {
            id: about
            name: qsTr("Video Player")
            description: qsTr("Plays videos with subtitles and playlists.")
            iconSource: "image://icontheme/multimedia-video-player"
            link: "https://github.com/cutefishos/video-player"

            Shortcut {
                sequence: "Escape"
                onActivated: about.close()
            }
        }
    }

    // MPRIS, for the Control Center and other media controls.
    MprisPlayer {
        id: mpris
        serviceName: "cutefish-video-player"
        identity: qsTr("Video Player")
        desktopEntry: "cutefish-video-player"
        supportedUriSchemes: ["file", "http", "https"]
        supportedMimeTypes: []
        canControl: true
        canQuit: true
        canRaise: true
        canSetFullscreen: true
        fullscreen: rootWindow.isFullScreen
        canPlay: rootWindow.hasMedia
        canPause: rootWindow.hasMedia
        canSeek: player.seekable
        canGoNext: App.playlist.hasNext
        canGoPrevious: App.playlist.hasPrevious
        hasTrackList: false
        playbackStatus: !player.loaded ? Mpris.Stopped : player.paused ? Mpris.Paused : Mpris.Playing
        loopStatus: player.loopFile ? Mpris.Track : App.playlist.repeatMode === Playlist.RepeatAll ? Mpris.Playlist
                    : App.playlist.repeatMode === Playlist.RepeatOne ? Mpris.Track : Mpris.None
        rate: player.speed
        minimumRate: 0.5
        maximumRate: 2
        volume: player.muted ? 0 : player.volume / 100
        position: Math.round(player.position * 1000000)
        metadata: {
            var data = {}
            data[Mpris.metadataToString(Mpris.TrackId)] = "/com/cutefish/VideoPlayer/Track/" + Math.max(0, App.playlist.currentIndex)
            data[Mpris.metadataToString(Mpris.Title)] = rootWindow.hasMedia ? rootWindow.mediaName : ""
            data[Mpris.metadataToString(Mpris.Length)] = Math.round(player.duration * 1000000)
            data[Mpris.metadataToString(Mpris.Url)] = player.source.toString()
            return data
        }

        onPlayRequested: if (player.paused) rootWindow.togglePause(false)
        onPauseRequested: player.paused = true
        onPlayPauseRequested: rootWindow.togglePause(false)
        onStopRequested: player.paused = true
        onNextRequested: rootWindow.playNext(false)
        onPreviousRequested: rootWindow.playPrevious()
        onSeekRequested: function (offset) {
            player.seekRelative(offset / 1000000)
            mpris.seeked(Math.round((player.position + offset / 1000000) * 1000000))
        }
        onSetPositionRequested: function (trackId, position) {
            player.seek(position / 1000000, true)
            mpris.seeked(position)
        }
        onVolumeRequested: function (volume) { rootWindow.setVolume(Math.round(volume * 100)) }
        onRateRequested: function (rate) { rootWindow.setSpeed(rate) }
        onFullscreenRequested: function (fullscreen) { if (fullscreen !== rootWindow.isFullScreen) rootWindow.toggleFullScreen() }
        onRaiseRequested: App.activateWindow()
        onQuitRequested: rootWindow.close()
    }

    // Keyboard. Ctrl+H, Ctrl+M and Ctrl+arrows stay free: the Command key
    // convention gives them to the compositor and to text navigation.

    Shortcut { sequences: ["Space", "K", "Media Play", "Toggle Media Play/Pause"]; enabled: rootWindow.hasMedia && !playlistKeysWanted.value; onActivated: rootWindow.togglePause(true) }
    Shortcut { sequence: "Media Pause"; onActivated: player.paused = true }
    Shortcut { sequence: "Media Stop"; onActivated: player.paused = true }
    Shortcut { sequences: ["N", "Media Next", ">"]; onActivated: rootWindow.playNext(false) }
    Shortcut { sequences: ["B", "Media Previous", "<"]; onActivated: rootWindow.playPrevious() }
    Shortcut { sequence: "Left"; autoRepeat: true; onActivated: rootWindow.seekBy(-5) }
    Shortcut { sequence: "Right"; autoRepeat: true; onActivated: rootWindow.seekBy(5) }
    Shortcut { sequence: "Shift+Left"; autoRepeat: true; onActivated: rootWindow.seekBy(-30) }
    Shortcut { sequence: "Shift+Right"; autoRepeat: true; onActivated: rootWindow.seekBy(30) }
    Shortcut { sequence: "Home"; onActivated: rootWindow.seekTo(0) }
    Shortcut { sequence: "End"; onActivated: rootWindow.seekTo(Math.max(0, player.duration - 1)) }
    Shortcut { sequence: "Up"; autoRepeat: true; enabled: !playlistKeysWanted.value; onActivated: rootWindow.setVolume(Math.min(Math.max(100, player.volume), player.volume + 5)) }
    Shortcut { sequence: "Down"; autoRepeat: true; enabled: !playlistKeysWanted.value; onActivated: rootWindow.setVolume(player.volume - 5) }
    Shortcut { sequence: "M"; onActivated: rootWindow.toggleMute() }
    Shortcut { sequences: ["F", "F11"]; enabled: rootWindow.hasMedia; onActivated: rootWindow.toggleFullScreen() }
    Shortcut { sequence: "Escape"; enabled: rootWindow.isFullScreen && !sidePanel.busy; onActivated: rootWindow.toggleFullScreen() }
    Shortcut { sequence: "["; autoRepeat: true; onActivated: rootWindow.stepSpeed(-1) }
    Shortcut { sequence: "]"; autoRepeat: true; onActivated: rootWindow.stepSpeed(1) }
    Shortcut { sequence: "Backspace"; onActivated: rootWindow.setSpeed(1) }
    Shortcut { sequence: ","; autoRepeat: true; onActivated: player.frameStep(true) }
    Shortcut { sequence: "."; autoRepeat: true; onActivated: player.frameStep(false) }
    Shortcut { sequence: "PgUp"; onActivated: rootWindow.stepChapter(-1) }
    Shortcut { sequence: "PgDown"; onActivated: rootWindow.stepChapter(1) }
    Shortcut { sequence: "J"; onActivated: rootWindow.cycleSubtitle(1) }
    Shortcut { sequence: "Shift+J"; onActivated: rootWindow.cycleSubtitle(-1) }
    Shortcut { sequence: "V"; onActivated: rootWindow.toggleSubtitles() }
    Shortcut { sequence: "A"; onActivated: rootWindow.cycleAudio() }
    Shortcut { sequence: "Z"; autoRepeat: true; onActivated: rootWindow.shiftSubtitles(-0.1) }
    Shortcut { sequence: "Shift+Z"; autoRepeat: true; onActivated: rootWindow.shiftSubtitles(0.1) }
    Shortcut { sequence: "Ctrl+Alt+S"; onActivated: rootWindow.takeScreenshot() }
    Shortcut { sequence: "P"; enabled: rootWindow.hasMedia; onActivated: rootWindow.togglePanel(0, true) }
    Shortcut { sequence: "Ctrl+O"; onActivated: rootWindow.openFiles(false) }
    Shortcut { sequence: "Ctrl+L"; onActivated: rootWindow.openDialog("openUrl") }
    Shortcut { sequence: "Ctrl+I"; onActivated: rootWindow.showMediaInfo() }
    Shortcut { sequences: ["Ctrl+?", "Ctrl+Shift+/"]; onActivated: rootWindow.openDialog("shortcuts") }
    Shortcut { sequences: ["Ctrl+W", "Ctrl+Q"]; onActivated: rootWindow.close() }
    Shortcut { sequence: "Ctrl+,"; onActivated: rootWindow.openDialog("preferences") }

    // While the playlist has the keyboard, its arrows and keys go to it.
    QtObject {
        id: playlistKeysWanted
        readonly property bool value: rootWindow.panelOpen && sidePanel.busy
    }

    Timer {
        // Keeps the resume point current, should the player not quit cleanly.
        interval: 30000
        repeat: true
        running: player.loaded && !player.paused
        onTriggered: rootWindow.saveResume()
    }

    onClosing: saveResume()

    onActiveChanged: if (!active) pointerActive = false

    // Functions

    function poke() {
        pointerActive = true
        idleTimer.restart()
    }

    // From the keyboard the list takes focus, so arrows, Return and Delete work in it.
    // tab -1 toggles the panel as it was; a tab opens it there, or closes it
    // when it already shows that tab.
    function togglePanel(tab, focusList) {
        if (tab < 0 || (panelOpen && panelTab !== tab)) {
            if (tab >= 0)
                panelTab = tab
            panelOpen = tab >= 0 ? true : !panelOpen
        } else {
            panelOpen = !panelOpen
            panelTab = tab
        }
        if (panelOpen && focusList && panelTab === 0)
            sidePanel.focusList()
        else if (!panelOpen)
            rootWindow.contentItem.forceActiveFocus()
    }

    function openFiles(append) {
        fileDialog.append = append
        fileDialog.open()
    }

    function openSubtitleFile() {
        if (player.source.toString().startsWith("file:"))
            subtitleDialog.currentFolder = player.source.toString().replace(/\/[^\/]*$/, "")
        subtitleDialog.open()
    }

    function menu(name) {
        if (!rootWindow[name + "Popup"]) {
            const components = {
                mainMenu: mainMenuComponent,
                contextMenu: contextMenuComponent,
                playlistMenu: playlistMenuComponent
            }
            rootWindow[name + "Popup"] = components[name].createObject(rootWindow)
        }
        return rootWindow[name + "Popup"]
    }

    function openPlaylistMenu(index) {
        const popup = menu("playlistMenu")
        popup.index = index
        popup.popup()
    }

    function applyLanguages() {
        player.setPreferredLanguages(App.settings.languagePreference(App.settings.audioLanguage),
                                     App.settings.languagePreference(App.settings.subtitleLanguage))
    }

    function loadCurrent() {
        const url = App.playlist.currentUrl
        if (url.toString() === player.source.toString() && player.loaded && !player.endReached) {
            // The same file again: from the start, as Repeat One does.
            player.seek(0, true)
            player.paused = false
            return
        }
        saveResume()
        errorMessage = ""
        if (url.toString().length === 0) {
            player.stop()
            return
        }

        var options = {}
        var resumed = 0
        if (App.settings.resumePlayback) {
            const entry = App.resume.entry(url)
            if (entry.position > 0) {
                resumed = entry.position
                options["start"] = entry.position
                if (entry.audioTrack >= 0)
                    options["aid"] = entry.audioTrack > 0 ? entry.audioTrack : "no"
                if (entry.subtitleTrack >= 0)
                    options["sid"] = entry.subtitleTrack > 0 ? entry.subtitleTrack : "no"
                if (entry.subtitleDelay !== 0)
                    options["sub-delay"] = entry.subtitleDelay
            }
        }
        player.load(url, options, false)
        recentPending = true
        if (resumed > 0) {
            osd.show(qsTr("Resumed from %1").arg(App.formatTime(resumed)), 5000, qsTr("Start Over"), function () {
                player.seek(0, true)
            })
        }
        poke()
    }

    function rememberRecent() {
        if (!recentPending || !player.loaded || (!player.hasVideo && player.audioTracks.length === 0))
            return
        recentPending = false
        App.settings.addRecentFile(player.source)
    }

    function saveResume() {
        if (!hasMedia || !player.loaded || player.duration <= 0)
            return
        if (!App.settings.resumePlayback) {
            App.resume.forget(player.source)
            return
        }
        App.resume.save(player.source, player.endReached ? player.duration : player.position, player.duration, {
            audioTrack: player.audioTrack,
            subtitleTrack: player.subtitleTrack,
            subtitleDelay: player.subtitleDelay
        })
    }

    function fileEnded() {
        if (player.loopFile)
            return
        saveResume()
        if (!App.playlist.next(true))
            poke()
    }

    function playIndex(index) {
        if (index === App.playlist.currentIndex) {
            player.seek(0, true)
            player.paused = false
        } else {
            App.playlist.currentIndex = index
        }
    }

    function playNext(automatic) {
        if (!App.playlist.next(automatic))
            osd.show(qsTr("End of playlist"))
    }

    function playPrevious() {
        // Like a CD player: back to the start first, unless it just began.
        if (player.position > 5 || !App.playlist.hasPrevious) {
            seekTo(0)
            return
        }
        App.playlist.previous()
    }

    function togglePause(showFlash) {
        if (!hasMedia) {
            openFiles(false)
            return
        }
        const play = player.paused || player.endReached
        player.paused = !play
        if (showFlash)
            flash.show(play ? "play" : "pause")
        poke()
    }

    function seekTo(seconds) {
        if (!player.seekable)
            return
        player.seek(seconds, true)
        osd.show(App.formatTime(seconds, player.duration) + " / " + App.formatTime(player.duration, player.duration))
    }

    function seekBy(seconds) {
        if (!player.seekable)
            return
        const target = Math.max(0, Math.min(player.duration, player.position + seconds))
        player.seekRelative(seconds)
        osd.show((seconds > 0 ? "+" : "−") + Math.abs(seconds) + " s   " + App.formatTime(target, player.duration))
    }

    function setVolume(value) {
        value = Math.max(0, Math.min(150, value))
        player.volume = value
        App.settings.volume = value
        if (player.muted && value > 0)
            toggleMute()
        osd.show(qsTr("Volume %1%").arg(value))
    }

    function toggleMute() {
        // player.muted follows mpv a moment later.
        const muted = !player.muted
        player.muted = muted
        App.settings.muted = muted
        osd.show(muted ? qsTr("Muted") : qsTr("Volume %1%").arg(player.volume))
    }

    function speedText(speed) {
        return Number(speed).toLocaleString(Qt.locale(), "f", speed * 100 % 10 === 0 ? 1 : 2) + "×"
    }

    function setSpeed(speed) {
        player.speed = speed
        osd.show(qsTr("Speed %1").arg(speedText(speed)))
    }

    function stepSpeed(direction) {
        var index = 0
        for (var i = 0; i < speeds.length; ++i) {
            if (Math.abs(speeds[i] - player.speed) < 0.001 || speeds[i] < player.speed)
                index = i
        }
        const exact = Math.abs(speeds[index] - player.speed) < 0.001
        index = direction > 0 ? Math.min(speeds.length - 1, index + 1) : Math.max(0, exact ? index - 1 : index)
        setSpeed(speeds[index])
    }

    function stepChapter(direction) {
        if (player.chapters.length === 0)
            return
        player.chapter = Math.max(0, player.chapter + direction)
        const chapter = player.chapters[Math.max(0, Math.min(player.chapters.length - 1, player.chapter + direction))]
        if (chapter)
            osd.show(chapter.title || App.formatTime(chapter.time, player.duration))
    }

    function selectSubtitle(id) {
        player.subtitleTrack = id
        var label = qsTr("Subtitles off")
        for (var i = 0; i < player.subtitleTracks.length; ++i) {
            if (player.subtitleTracks[i].id === id)
                label = qsTr("Subtitles: %1").arg(player.subtitleTracks[i].label)
        }
        osd.show(label)
    }

    function selectAudio(id) {
        player.audioTrack = id
        for (var i = 0; i < player.audioTracks.length; ++i) {
            if (player.audioTracks[i].id === id)
                osd.show(qsTr("Audio: %1").arg(player.audioTracks[i].label))
        }
    }

    function cycleSubtitle(direction) {
        const ids = [0]
        for (var i = 0; i < player.subtitleTracks.length; ++i)
            ids.push(player.subtitleTracks[i].id)
        const next = (ids.indexOf(player.subtitleTrack) + direction + ids.length) % ids.length
        selectSubtitle(ids[next])
    }

    function toggleSubtitles() {
        if (player.subtitleTrack > 0)
            selectSubtitle(0)
        else if (lastSubtitle > 0)
            selectSubtitle(lastSubtitle)
        else if (player.subtitleTracks.length > 0)
            selectSubtitle(player.subtitleTracks[0].id)
    }

    function cycleAudio() {
        const tracks = player.audioTracks
        if (tracks.length < 2)
            return
        for (var i = 0; i < tracks.length; ++i) {
            if (tracks[i].id === player.audioTrack) {
                selectAudio(tracks[(i + 1) % tracks.length].id)
                return
            }
        }
        selectAudio(tracks[0].id)
    }

    function delayText(seconds) {
        const ms = Math.round(seconds * 1000)
        return (ms > 0 ? "+" : ms < 0 ? "−" : "") + qsTr("%1 ms").arg(Math.abs(ms))
    }

    function shiftSubtitles(seconds) {
        const delay = Math.round((player.subtitleDelay + seconds) * 10) / 10
        player.subtitleDelay = delay
        osd.show(qsTr("Subtitle delay %1").arg(delayText(delay)))
    }

    function takeScreenshot() {
        if (!player.loaded || !player.hasVideo)
            return
        player.screenshot(App.screenshotPath(mediaName))
    }

    function showMediaInfo() {
        if (player.loaded)
            dialog("mediaInfo").present(player.mediaInfo(), mediaName)
    }

    function toggleFullScreen() {
        if (isFullScreen) {
            if (wasMaximized)
                showMaximized()
            else
                showNormal()
        } else {
            wasMaximized = isMaximized
            showFullScreen()
        }
    }

    function fitToLaunch(ratio) {
        if (!launchFitPending)
            return
        const size = fittedSize(App.launchVideoSize, ratio)
        width = size.width
        height = size.height
    }

    // Fits the window to the picture on each file's first frame.
    function fitToVideo() {
        if (!fitPending || player.videoSize.width <= 0 || player.videoSize.height <= 0)
            return
        fitPending = false
        if (isFullScreen || isMaximized || !player.hasVideo)
            return
        const size = fittedSize(player.videoSize, FishUI.Dpi.ratio)
        if (size.width !== width || size.height !== height)
            setGeometry(x, y, size.width, size.height)
    }

    // The window size for a picture: device pixels one to one, within three
    // quarters of the work area.
    function fittedSize(videoSize, ratio) {
        var w = videoSize.width / (ratio > 0 ? ratio : 1)
        var h = videoSize.height / (ratio > 0 ? ratio : 1)
        // The window's screen, as the Screen attached type knows none before the first show.
        const scale = Math.min(1, screen.desktopAvailableWidth * 0.75 / w, screen.desktopAvailableHeight * 0.75 / h)
        return Qt.size(Math.max(minimumWidth, Math.round(w * scale)), Math.max(minimumHeight, Math.round(h * scale)))
    }
}
