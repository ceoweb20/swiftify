/*
 * Swiftify, a spotify player for desktop
 * Copyright (C) 2026  shred0s
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Swiftify

// My Playlists, or the pinned ones (PlaylistGridView / PinnedPlaylistsView):
// an adaptive grid of 180-240 px tiles.
GridView {
    id: root

    property bool pinnedOnly: false
    property bool personalized: false
    readonly property PlaylistListModel source: personalized ? Library.personalizedPlaylists
                                                : pinnedOnly ? Library.pinnedPlaylists : Library.playlists
    /// the tiles ahead of the playlists: a new playlist's
    readonly property int leading: pinnedOnly || personalized ? 0 : 1
    readonly property real margin: Appearance.sizes.pageMargin
    // whole columns at least 200 px wide that fill the width exactly: the
    // grid lays columns out by cellWidth, so tile plus gap has to divide it.
    // each cell's 20 px gap is split either side of its tile, and the grid
    // reaches 10 px past the margin so the tiles themselves line up with it
    readonly property real available: width - 2 * margin + 20
    readonly property int columns: Math.max(1, Math.floor(available / 200))
    readonly property real tile: available / columns - 20
    property playlist menuPlaylist

    leftMargin: margin - 10
    rightMargin: margin - 10
    // under the top bar, which floats over the page, and a little more
    topMargin: Appearance.sizes.topBarHeight + 24
    bottomMargin: Appearance.sizes.playerBarClearance
    cellWidth: available / columns
    cellHeight: tile + 86
    boundsBehavior: Flickable.StopAtBounds
    reuseItems: true
    ScrollBar.vertical: StyledScrollBar { topPadding: 3 + Appearance.sizes.topBarHeight }
    PageWheelHandler { page: root }

    // the new-playlist tile comes first on the full grid
    model: source.count + leading

    header: Item {
        width: root.available
        height: headingPanel.height + 20
        // Keep the heading behind the tiles' hover lift.
        z: 0.5

        Rectangle {
            id: headingPanel
            objectName: "playlistsHeadingPanel"
            x: 10
            width: parent.width - 20
            height: Math.max(104, heading.implicitHeight + 40)
            radius: 24
            color: Appearance.colors.surfaceContainerHigh
            border.width: 1
            border.color: Qt.alpha(Appearance.colors.outlineVariant, 0.4)

            Stickers {
                anchors.fill: parent
                key: root.personalized ? "personalized" : root.pinnedOnly ? "pinned" : "playlists"
                count: 4
                maximumSize: 150
                clipRadius: headingPanel.radius
            }

            TitleText {
                id: heading
                x: 24
                width: Math.max(0, (refresh.visible ? refresh.x - 16 : parent.width - 20) - x)
                anchors.verticalCenter: parent.verticalCenter
                text: root.pinnedOnly ? qsTr("Pinned Playlists") : qsTr("My Playlists")
                font.pixelSize: headingPanel.width < 500 ? Appearance.fontSize.headline : Appearance.fontSize.display
                font.weight: Font.Bold
                wrapMode: Text.Wrap
            }
            IconButton {
                id: refresh
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                visible: root.personalized
                symbol: "refresh"
                tooltip: qsTr("Refresh collections")
                enabled: !Library.loadingDiscovery
                onClicked: Library.refreshDiscovery()
            }
        }
    }

    delegate: Item {
        id: cell

        required property int index
        readonly property bool isNewTile: !root.pinnedOnly && !root.personalized && index === 0
        readonly property playlist playlist: root.source.at(index - root.leading)

        width: root.cellWidth
        height: root.cellHeight

        MediaCard {
            x: 10
            visible: !cell.isNewTile
            artworkSize: root.tile
            artworkUrl: cell.playlist.artworkUrl
            fallbackKey: cell.playlist.id.length > 0 ? cell.playlist.id : cell.playlist.name
            placeholderSymbol: "queue_music"
            title: cell.playlist.name
            subtitle: cell.playlist.songCount > 0 ? Format.songCount(cell.playlist.songCount) : cell.playlist.ownerDisplayName
            onClicked: Library.selectPlaylist(cell.playlist)
            onSecondaryClicked: {
                root.menuPlaylist = cell.playlist
                pinMenu.popupHere()
            }

            Rectangle {
                x: root.tile - width - 8
                y: 8
                Reveal.shown: Library.pinsRevision >= 0 && Library.isPinned(cell.playlist.id)
                width: 28
                height: 28
                radius: 14
                color: Qt.rgba(0, 0, 0, 0.55)

                MaterialSymbol {
                    anchors.centerIn: parent
                    text: "push_pin"
                    iconSize: 15
                    fill: 1
                    color: "white"
                }
            }
        }

        // a dashed tile that starts a new playlist
        RippleButton {
            x: 10
            visible: cell.isNewTile
            width: root.tile
            height: root.tile
            buttonRadius: Appearance.sizes.artworkRadius
            colBackground: Appearance.colors.surfaceContainerLow
            colBackgroundHover: Appearance.colors.surfaceContainer
            onClicked: UiState.beginCreatePlaylist()

            contentItem: Column {
                spacing: 10

                Item { width: 1; height: (root.tile - 80) / 2 }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 44
                    height: 44
                    radius: 22
                    color: Appearance.colors.primaryContainer

                    MaterialSymbol {
                        anchors.centerIn: parent
                        text: "add"
                        iconSize: 26
                        color: Appearance.colors.onPrimaryContainer
                    }
                }
                StyledText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("New Playlist")
                    font.weight: Font.DemiBold
                }
            }
        }
    }


    EmptyState {
        objectName: "personalizedEmptyState"
        anchors.centerIn: parent
        width: Math.max(0, parent.width - 2 * root.margin)
        Reveal.shown: root.personalized && Library.personalizedPlaylists.count === 0
        Reveal.scaleFrom: 1
        Reveal.duration: Appearance.duration(220)
        Reveal.overshoot: 0
        symbol: "auto_awesome"
        title: Library.loadingDiscovery ? qsTr("Loading your mixes…") : qsTr("No personalized playlists yet")
        description: Library.discoveryStatus.length > 0 ? qsTranslate("Discovery", Library.discoveryStatus)
            : qsTr("Your mixes, blends and Spotify collections appear here when available for your account.")
    }

    EmptyState {
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 2 * root.margin)
        Reveal.shown: root.pinnedOnly && Library.pinnedPlaylists.count === 0
        Reveal.scaleFrom: 1
        Reveal.duration: Appearance.duration(220)
        Reveal.overshoot: 0
        symbol: "push_pin"
        title: qsTr("No Pinned Playlists")
        description: qsTr("Right-click a playlist to pin it here.")
    }

    MenuLoader {
        id: pinMenu
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
        }
    }
}
