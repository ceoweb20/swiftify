pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Swiftify

// the navigation drawer (MusicSidebar): destinations, the two playlist groups
// and the account. the list of every playlist is virtualized, so a library of
// hundreds costs only the rows on screen.
Item {
    id: root

    property playlist menuPlaylist
    readonly property bool menuPlaylistEditable: menuPlaylist.isCollaborative
        || (Library.profileId.length > 0 && menuPlaylist.ownerId === Library.profileId)

    function isSelected(page: int): bool {
        return Library.page === page
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.bottomMargin: settingsButton.height + 16
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: 2
        model: Settings.allPlaylistsExpanded ? Library.playlists : null
        ScrollBar.vertical: StyledScrollBar {}

        header: Column {
            width: list.width
            spacing: 2
            bottomPadding: 2

            Item { width: 1; height: 8 }

            NavigationItem {
                width: parent.width
                text: qsTr("Home")
                iconShape: "cookie9"
                symbol: "home"
                selected: root.isSelected(Library.Page.Home)
                onClicked: Library.showHome()
            }
            NavigationItem {
                width: parent.width
                text: qsTr("Search")
                iconShape: "clover4"
                symbol: "search"
                selected: root.isSelected(Library.Page.Search)
                onClicked: Library.showSearch()
            }

            SectionLabel {
                width: parent.width
                text: qsTr("Library")
            }
            NavigationItem {
                width: parent.width
                text: qsTr("Songs")
                iconShape: "heart"
                keepsShape: true
                symbol: "music_note"
                selected: root.isSelected(Library.Page.Songs)
                onClicked: Library.showSavedSongs()
            }

            NavigationItem {
                width: parent.width
                text: qsTr("Stats")
                iconShape: "softBurst"
                symbol: "insights"
                selected: root.isSelected(Library.Page.Stats)
                onClicked: Library.showStats()
            }

            SectionLabel {
                width: parent.width
                text: qsTr("Playlists")
                trailing: IconButton {
                    symbol: "add"
                    size: 32
                    iconSize: 20
                    iconColor: Appearance.colors.primary
                    onClicked: UiState.beginCreatePlaylist()

                    tooltip: qsTr("New playlist")
                }
            }

            NavigationItem {
                width: parent.width
                text: qsTr("Pinned Playlists")
                iconShape: "flower"
                symbol: "push_pin"
                selected: root.isSelected(Library.Page.PinnedPlaylists)
                onClicked: Library.showPinnedPlaylists()
                trailing: IconButton {
                    symbol: "expand_more"
                    size: 28
                    iconSize: 20
                    rotation: Settings.pinnedPlaylistsExpanded ? 0 : -90
                    onClicked: Settings.pinnedPlaylistsExpanded = !Settings.pinnedPlaylistsExpanded

                    Behavior on rotation {
                        ElementMoveSmall {}
                    }
                }
            }


            // the pinned group opens and shuts on a spring: past its height
            // and back opening, and a rebound shutting
            Item {
                width: parent.width
                height: Settings.pinnedPlaylistsExpanded ? pinned.implicitHeight : 0
                visible: height > 0.5
                clip: true

                Behavior on height {
                    enabled: !Appearance.performanceMode
                    SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.5 }
                }

                Column {
                    id: pinned
                    width: parent.width
                    spacing: 2
                    opacity: Settings.pinnedPlaylistsExpanded ? 1 : 0

                    Behavior on opacity {
                        NumberAnimation { duration: Appearance.duration(220); easing.type: Easing.OutCubic }
                    }

                    StyledText {
                        Reveal.shown: Library.pinnedPlaylists.count === 0
                        leftPadding: 28
                        height: 28
                        text: qsTr("No playlists")
                        font.pixelSize: Appearance.fontSize.smaller
                        color: Appearance.colors.onSurfaceVariant
                    }

                    Repeater {
                        model: Library.pinnedPlaylists

                        PlaylistNavItem {
                            width: parent.width
                        }
                    }
                }
            }

            NavigationItem {
                width: parent.width
                text: qsTr("All Playlists")
                iconShape: "bun"
                symbol: "grid_view"
                selected: root.isSelected(Library.Page.Playlists)
                onClicked: Library.showPlaylists()
                trailing: IconButton {
                    symbol: "expand_more"
                    size: 28
                    iconSize: 20
                    rotation: Settings.allPlaylistsExpanded && !root.collapsingAll ? 0 : -90
                    onClicked: root.toggleAllPlaylists()

                    Behavior on rotation {
                        ElementMoveSmall {}
                    }
                }
            }

            StyledText {
                Reveal.shown: Settings.allPlaylistsExpanded && Library.playlists.count === 0
                leftPadding: 28
                height: 28
                text: qsTr("No playlists")
                font.pixelSize: Appearance.fontSize.smaller
                color: Appearance.colors.onSurfaceVariant
            }
        }

        // expanded, the rows cascade in on a staggered spring
        populate: Transition {
            SequentialAnimation {
                PauseAnimation { duration: Appearance.motion(Math.min(Math.max(ViewTransition.index, 0), 12) * 24) }
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Appearance.duration(220); easing.type: Easing.OutCubic }
                    NumberAnimation { property: "scale"; from: 0.7; to: 1; duration: Appearance.motion(520); easing.type: Easing.OutBack; easing.overshoot: 2.2 }
                }
            }
        }

        delegate: PlaylistNavItem {
            id: row

            required property int index

            width: list.width

            // collapsing, they shrink away first, the higher ones a moment before
            Connections {
                target: root
                function onCollapsingAllChanged() {
                    if (root.collapsingAll) {
                        leave.restart()
                    }
                }
            }
            SequentialAnimation {
                id: leave
                PauseAnimation { duration: Math.min(row.index, 12) * 12 }
                ParallelAnimation {
                    NumberAnimation { target: row; property: "opacity"; to: 0; duration: 150; easing.type: Easing.InCubic }
                    NumberAnimation { target: row; property: "scale"; to: 0.75; duration: 170; easing.type: Easing.InBack }
                }
            }
        }
    }

    /// the all-playlists rows are leaving; the list drops them once they have
    property bool collapsingAll: false

    function toggleAllPlaylists() {
        if (!Settings.allPlaylistsExpanded) {
            Settings.allPlaylistsExpanded = true
            return
        }
        if (Appearance.performanceMode) {
            Settings.allPlaylistsExpanded = false
            return
        }
        collapsingAll = true
        collapseTimer.restart()
    }

    Timer {
        id: collapseTimer
        interval: 320
        onTriggered: {
            Settings.allPlaylistsExpanded = false
            root.collapsingAll = false
        }
    }

    component PlaylistNavItem: NavigationItem {
        id: item

        required property playlist playlist

        text: playlist.name
        showArtwork: true
        artworkUrl: playlist.artworkUrl
        fallbackKey: playlist.id.length > 0 ? playlist.id : playlist.name
        indent: 12
        selected: Library.page === Library.Page.Playlist && Library.selectedPlaylistId === playlist.id
        onClicked: Library.selectPlaylist(item.playlist)
        trailing: MaterialSymbol {
            Reveal.shown: Library.pinsRevision >= 0 && Library.isPinned(item.playlist.id)
            text: "push_pin"
            iconSize: 14
            fill: 1
            color: Appearance.colors.primary
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: (point) => {
                root.menuPlaylist = item.playlist
                playlistMenu.popupHere()
            }
        }
    }

    MenuLoader {
        id: playlistMenu
        sourceComponent: StyledMenu {
            StyledMenuItem {
                id: pinItem
                readonly property bool pinned: Library.pinsRevision >= 0 && Library.isPinned(root.menuPlaylist.id)
                text: pinned ? qsTr("Unpin Playlist") : qsTr("Pin Playlist")
                symbol: pinned ? "keep_off" : "keep"
                onTriggered: {
                    if (!pinItem.pinned) {
                        UiState.burst(pinItem, "pin")
                    }
                    Library.togglePinned(root.menuPlaylist)
                }
            }
            StyledMenuSeparator {
                visible: root.menuPlaylistEditable
                height: visible ? implicitHeight : 0
            }
            StyledMenuItem {
                visible: root.menuPlaylistEditable
                height: visible ? implicitHeight : 0
                text: qsTr("Delete Playlist")
                symbol: "delete"
                destructive: true
                onTriggered: UiState.deletePlaylistRequested(root.menuPlaylist)
            }
        }
    }

    // the settings at the sidebar's foot, the account among them
    NavigationItem {
        id: settingsButton
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        text: qsTr("Settings")
        iconShape: "cookie12"
        symbol: "settings"
        onClicked: UiState.settingsRequested()
    }
}
