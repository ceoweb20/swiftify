pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Swiftify

// Search (GlobalSearchView): categories and history when empty, results after.
Flickable {
    id: root

    // pages run up under the top bar, which floats over them with no
    // background of its own; they start below it
    topMargin: Appearance.sizes.topBarHeight
    contentWidth: width
    flickableDirection: Flickable.VerticalFlick

    PageWheelHandler {
        id: pageWheel
        page: root
    }

    readonly property real margin: Appearance.sizes.pageMargin
    readonly property bool empty: Library.searchQuery.trim().length === 0

    contentHeight: column.implicitHeight + 24 + Appearance.sizes.playerBarClearance
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: StyledScrollBar { topPadding: 3 + Appearance.sizes.topBarHeight }

    /// whether something `top` down the page and `height` tall is in sight,
    /// or within `slack` of it
    function reaches(top: real, height: real, slack: real): bool {
        return top + height > root.contentY - slack && top < root.contentY + root.height + slack
    }

    Genres {
        id: genres
    }

    Stickers {
        width: root.width
        height: 220
        key: "search"
        count: 4
    }

    Column {
        id: column
        y: 24
        width: root.width
        spacing: 0

        // nothing typed: what was played from here, and categories
        Column {
            width: root.width
            Reveal.shown: root.empty
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0

            Item {
                objectName: "searchHistoryHeading"
                x: root.margin
                width: root.width - 2 * root.margin
                height: 48
                Reveal.shown: Library.searchHistory.count > 0
                Reveal.scaleFrom: 1
                Reveal.duration: Appearance.duration(220)
                Reveal.overshoot: 0

                TitleText {
                    width: Math.max(0, clearHistory.x - x - 12)
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Played from Search")
                    font.pixelSize: Appearance.fontSize.title
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                TextButton {
                    id: clearHistory
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    variant: TextButton.Text
                    text: qsTr("Clear")
                    onClicked: Library.clearSearchHistory()
                }
            }

            Repeater {
                model: root.empty ? Library.searchHistoryPreview : null

                SongRow {
                    x: root.margin
                    width: root.width - 2 * root.margin
                    songs: Library.searchHistoryPreview
                    showsNumber: false
                    fromSearch: true
                }
            }

            TitleText {
                objectName: "searchBrowseHeading"
                x: root.margin
                width: root.width - 2 * root.margin
                topPadding: Library.searchHistory.count > 0 ? 24 : 0
                bottomPadding: 16
                text: qsTr("Browse Categories")
                font.pixelSize: Appearance.fontSize.headline
                wrapMode: Text.Wrap
            }

            Flow {
                id: categories
                x: root.margin
                width: root.width - 2 * root.margin
                spacing: 16
                readonly property int columns: Math.max(1, Math.floor((width + spacing) / 192))
                readonly property real tileWidth: (width - spacing * (columns - 1)) / columns

                Repeater {
                    model: genres.featured

                    GenreCard {
                        required property var modelData
                        width: categories.tileWidth
                        name: modelData.name
                        symbol: modelData.symbol
                        startColor: modelData.start
                        endColor: modelData.end
                        onClicked: Library.searchQuery = name
                    }
                }
            }
        }

        // the phrase
        Item {
            width: root.width
            height: 360
            Reveal.shown: !root.empty && Library.searchIsCatchphrase
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0

            Column {
                anchors.centerIn: parent
                spacing: 16

                MaterialSymbol {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 48
                    height: 48
                    text: "auto_awesome"
                    iconSize: 42
                    fill: 1
                    color: Appearance.colors.primary
                }
                TitleText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: UiState.catchphrase
                    font.pixelSize: Appearance.fontSize.headline
                    font.weight: Font.Bold
                }
                StyledText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("You found it.")
                    color: Appearance.colors.onSurfaceVariant
                }
            }
        }

        Item {
            width: root.width
            height: 280
            Reveal.shown: !root.empty && !Library.searchIsCatchphrase && Library.searching
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0

            Column {
                anchors.centerIn: parent
                spacing: 12
                LoadingSpinner { anchors.horizontalCenter: parent.horizontalCenter }
                StyledText {
                    text: qsTr("Searching Spotify…")
                    color: Appearance.colors.onSurfaceVariant
                }
            }
        }

        Item {
            width: root.width
            height: 360
            Reveal.shown: !root.empty && !Library.searchIsCatchphrase && !Library.searching && !Library.hasSearchResults
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0

            EmptyState {
                anchors.centerIn: parent
                width: Math.min(420, parent.width - 2 * root.margin)
                symbol: "search_off"
                title: qsTr("No Results for “%1”").arg(Library.searchQuery.trim())
                description: qsTr("Check the spelling or try a new search.")
            }
        }

        // results
        Column {
            id: results
            width: root.width
            Reveal.shown: !root.empty && !Library.searchIsCatchphrase && !Library.searching && Library.hasSearchResults
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0
            spacing: 0

            TitleText {
                objectName: "searchSongsHeading"
                x: root.margin
                width: root.width - 2 * root.margin
                Reveal.shown: Library.searchSongs.count > 0
                Reveal.scaleFrom: 1
                Reveal.duration: Appearance.duration(220)
                Reveal.overshoot: 0
                bottomPadding: 10
                text: qsTr("Songs")
                font.pixelSize: Appearance.fontSize.title
            }

            Repeater {
                model: Library.searchSongs

                SongRow {
                    x: root.margin
                    width: root.width - 2 * root.margin
                    songs: Library.searchSongs
                    showsNumber: false
                    fromSearch: true
                }
            }

            Shelf {
                pageScroller: pageWheel
                width: root.width
                onScreen: root.reaches(column.y + results.y + y, height, 0)
                nearScreen: root.reaches(column.y + results.y + y, height, root.height / 2)
                Reveal.shown: Library.searchAlbums.count > 0
                Reveal.scaleFrom: 1
                Reveal.duration: Appearance.duration(220)
                Reveal.overshoot: 0
                topPadding: 28
                title: qsTr("Albums")
                symbol: "album"
                iconShape: "sunny"
                model: Library.searchAlbums
                delegate: MediaCard {
                    required property album album
                    artworkUrl: album.artworkUrl
                    title: album.name
                    subtitle: album.artists
                    onClicked: Library.selectAlbum(album)
                }
            }

            Shelf {
                pageScroller: pageWheel
                width: root.width
                onScreen: root.reaches(column.y + results.y + y, height, 0)
                nearScreen: root.reaches(column.y + results.y + y, height, root.height / 2)
                Reveal.shown: Library.searchArtists.count > 0
                Reveal.scaleFrom: 1
                Reveal.duration: Appearance.duration(220)
                Reveal.overshoot: 0
                topPadding: 28
                title: qsTr("Artists")
                symbol: "person"
                iconShape: "cookie12"
                model: Library.searchArtists
                delegate: MediaCard {
                    required property artist artist
                    artworkUrl: artist.artworkUrl
                    title: artist.name
                    circular: true
                    titleLines: 1
                    badgeSymbol: "chevron_right"
                    onClicked: Library.selectArtist(artist)
                }
            }

            Shelf {
                pageScroller: pageWheel
                width: root.width
                onScreen: root.reaches(column.y + results.y + y, height, 0)
                nearScreen: root.reaches(column.y + results.y + y, height, root.height / 2)
                Reveal.shown: Library.searchPlaylists.count > 0
                Reveal.scaleFrom: 1
                Reveal.duration: Appearance.duration(220)
                Reveal.overshoot: 0
                topPadding: 28
                title: qsTr("Playlists")
                symbol: "queue_music"
                iconShape: "bun"
                model: Library.searchPlaylists
                delegate: MediaCard {
                    required property playlist playlist
                    artworkUrl: playlist.artworkUrl
                    fallbackKey: playlist.id.length > 0 ? playlist.id : playlist.name
                    title: playlist.name
                    subtitle: playlist.ownerDisplayName
                    onClicked: Library.selectPlaylist(playlist)
                }
            }
        }
    }
}
