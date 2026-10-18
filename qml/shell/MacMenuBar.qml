pragma ComponentBehavior: Bound
import QtQuick
import Qt.labs.platform as Platform
import Swiftify

// the mac's menu bar for the main window. its key equivalents are the app's
// shortcuts, so on macos the window's own Shortcuts for these stand down
// (Main.qml); space stays the window's, since a menu would take it from text
// fields. qt moves About, Settings and Quit into the app's own menu.
Platform.MenuBar {
    id: root

    signal findRequested()
    signal closeWindowRequested()
    signal minimizeRequested()
    signal zoomRequested()
    signal showWindowRequested()

    Platform.Menu {
        title: qsTr("File")

        Platform.MenuItem {
            text: qsTr("About Swiftify")
            role: Platform.MenuItem.AboutRole
            onTriggered: UiState.settingsRequested()
        }
        Platform.MenuItem {
            text: qsTr("Settings…")
            role: Platform.MenuItem.PreferencesRole
            shortcut: "Ctrl+,"
            onTriggered: UiState.settingsRequested()
        }
        Platform.MenuItem {
            text: qsTr("Quit Swiftify")
            role: Platform.MenuItem.QuitRole
            shortcut: StandardKey.Quit
            onTriggered: App.quit()
        }

        Platform.MenuItem {
            text: qsTr("New Playlist…")
            shortcut: StandardKey.New
            enabled: Session.libraryConnected
            onTriggered: UiState.beginCreatePlaylist()
        }
        Platform.MenuSeparator {}
        Platform.MenuItem {
            text: qsTr("Close Window")
            shortcut: StandardKey.Close
            onTriggered: root.closeWindowRequested()
        }
    }

    Platform.Menu {
        title: qsTr("Edit")

        Platform.MenuItem {
            text: qsTr("Find")
            shortcut: StandardKey.Find
            onTriggered: root.findRequested()
        }
    }

    Platform.Menu {
        title: qsTr("View")

        Platform.MenuItem {
            text: qsTr("Home")
            shortcut: "Ctrl+1"
            checkable: true
            checked: Library.page === Library.Page.Home
            onTriggered: Library.showHome()
        }
        Platform.MenuItem {
            text: qsTr("Search")
            shortcut: "Ctrl+2"
            checkable: true
            checked: Library.page === Library.Page.Search
            onTriggered: Library.showSearch()
        }
        Platform.MenuItem {
            text: qsTr("Favourite Songs")
            shortcut: "Ctrl+3"
            checkable: true
            checked: Library.page === Library.Page.Songs
            onTriggered: Library.showSavedSongs()
        }
        Platform.MenuItem {
            text: qsTr("Playlists")
            shortcut: "Ctrl+4"
            checkable: true
            checked: Library.page === Library.Page.Playlists
            onTriggered: Library.showPlaylists()
        }
        Platform.MenuSeparator {}
        Platform.MenuItem {
            text: qsTr("Back")
            shortcut: StandardKey.Back
            enabled: Library.canGoBack
            onTriggered: Library.goBack()
        }
        Platform.MenuSeparator {}
        Platform.MenuItem {
            text: qsTr("Lyrics")
            shortcut: "Ctrl+Alt+L"
            checkable: true
            checked: Player.showingLyrics
            enabled: Player.hasSong
            onTriggered: Player.toggleLyricsPanel()
        }
        Platform.MenuItem {
            text: qsTr("Playing Next")
            shortcut: "Ctrl+Alt+Q"
            checkable: true
            checked: Player.showingQueue
            onTriggered: Player.toggleQueuePanel()
        }
        Platform.MenuSeparator {}
        Platform.MenuItem {
            text: UiState.immersiveOpen ? qsTr("Close Immersive Player") : qsTr("Immersive Player")
            shortcut: "Ctrl+Shift+F"
            enabled: Player.hasSong || UiState.immersiveOpen
            onTriggered: UiState.immersiveOpen = !UiState.immersiveOpen
        }
    }

    Platform.Menu {
        title: qsTr("Controls")

        Platform.MenuItem {
            text: Player.playing ? qsTr("Pause") : qsTr("Play")
            enabled: Player.hasSong
            onTriggered: Player.togglePlayback()
        }
        Platform.MenuItem {
            text: qsTr("Next")
            shortcut: "Ctrl+Right"
            enabled: Player.canSkipForward
            onTriggered: Player.skipForward()
        }
        Platform.MenuItem {
            text: qsTr("Previous")
            shortcut: "Ctrl+Left"
            enabled: Player.canSkipBackward
            onTriggered: Player.skipBackward()
        }
        Platform.MenuSeparator {}
        Platform.MenuItem {
            text: qsTr("Forward 10 Seconds")
            shortcut: "Ctrl+Alt+Right"
            enabled: Player.hasSong
            onTriggered: Player.seekBy(10000)
        }
        Platform.MenuItem {
            text: qsTr("Back 10 Seconds")
            shortcut: "Ctrl+Alt+Left"
            enabled: Player.hasSong
            onTriggered: Player.seekBy(-10000)
        }
        Platform.MenuSeparator {}
        Platform.MenuItem {
            text: qsTr("Volume Up")
            shortcut: "Ctrl+Up"
            enabled: Session.connected
            onTriggered: Player.adjustVolume(0.05)
        }
        Platform.MenuItem {
            text: qsTr("Volume Down")
            shortcut: "Ctrl+Down"
            enabled: Session.connected
            onTriggered: Player.adjustVolume(-0.05)
        }
        Platform.MenuItem {
            text: Player.volume <= 0.01 ? qsTr("Unmute") : qsTr("Mute")
            shortcut: "Ctrl+Alt+M"
            enabled: Session.connected
            onTriggered: Player.toggleMute()
        }
        Platform.MenuSeparator {}
        Platform.MenuItem {
            text: qsTr("Shuffle")
            shortcut: "Ctrl+Alt+S"
            checkable: true
            checked: Player.shuffle
            enabled: Player.hasSong
            onTriggered: Player.toggleShuffle()
        }
        Platform.MenuItem {
            text: Player.repeatMode === Player.RepeatMode.One ? qsTr("Repeat: Song")
                : Player.repeatMode === Player.RepeatMode.All ? qsTr("Repeat: Queue") : qsTr("Repeat: Off")
            shortcut: "Ctrl+Alt+R"
            enabled: Player.hasSong
            onTriggered: Player.cycleRepeatMode()
        }
    }

    Platform.Menu {
        title: qsTr("Window")

        Platform.MenuItem {
            text: qsTr("Minimize")
            shortcut: "Ctrl+M"
            onTriggered: root.minimizeRequested()
        }
        Platform.MenuItem {
            text: qsTr("Zoom")
            onTriggered: root.zoomRequested()
        }
        Platform.MenuSeparator {}
        Platform.MenuItem {
            text: "Swiftify"
            onTriggered: root.showWindowRequested()
        }
    }
}
