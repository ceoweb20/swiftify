pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import Swiftify

// Listen Now (MusicHomeView)
Flickable {
    id: root

    // pages run up under the top bar, which floats over them with no
    // background of its own; they start below it
    topMargin: Appearance.sizes.topBarHeight

    readonly property real margin: Appearance.sizes.pageMargin
    readonly property bool hasSpotlight: Player.hasSong || Library.recentlyPlayed.count > 0
    // at() is a call, which a binding cannot watch: the list announces its
    // count with every new list, so reading that re-reads its first song
    readonly property song spotlightSong: Player.hasSong ? Player.currentSong
                                          : Library.recentlyPlayed.at(Library.recentlyPlayed.count > 0 ? 0 : -1)
    readonly property bool spotlightIsCurrent: Player.hasSong && Player.currentSong.instanceId === spotlightSong.instanceId

    contentHeight: column.implicitHeight + 24 + Appearance.sizes.playerBarClearance
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: StyledScrollBar { topPadding: 3 + Appearance.sizes.topBarHeight }

    /// whether something `top` down the page and `height` tall is in sight,
    /// or within `slack` of it
    function reaches(top: real, height: real, slack: real): bool {
        return top + height > root.contentY - slack && top < root.contentY + root.height + slack
    }

    function openGenre(name: string) {
        Library.searchQuery = name
        Library.showSearch()
    }

    Genres {
        id: genres
    }

    // a few faint shapes behind the page's heading
    Stickers {
        width: root.width
        height: 260
        key: "home"
        count: 5
    }

    Column {
        id: column
        y: 24
        width: root.width
        spacing: Appearance.sizes.sectionGap

        Column {
            x: root.margin
            spacing: 6

            TitleText {
                text: "Swiftify"
                font.pixelSize: Appearance.fontSize.display
                font.weight: Font.Bold
                font.letterSpacing: -0.8
            }
            // a greeting for the time of day, on a shape to match it
            Row {
                id: greeting

                readonly property int hour: new Date().getHours()
                readonly property int part: hour < 5 ? 3 : (hour < 12 ? 0 : (hour < 17 ? 1 : (hour < 21 ? 2 : 3)))

                spacing: 10

                ShapeIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    size: 30
                    iconSize: 17
                    shape: ["sunny", "softBurst", "flower", "ghostish"][greeting.part]
                    symbol: ["wb_sunny", "light_mode", "wb_twilight", "dark_mode"][greeting.part]
                    color: Appearance.colors.tertiaryContainer
                    iconColor: Appearance.colors.onTertiaryContainer
                    hovered: greetingHover.hovered
                }
                StyledText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: ["Good morning", "Good afternoon", "Good evening", "Up late..."][greeting.part]
                          + "."
                    color: Appearance.colors.onSurfaceVariant
                }

                HoverHandler {
                    id: greetingHover
                }
            }
        }

        Item {
            width: root.width
            height: 220
            Reveal.shown: Library.loadingHome && Library.recentlyPlayed.count === 0

            Column {
                anchors.centerIn: parent
                spacing: 12
                LoadingSpinner { anchors.horizontalCenter: parent.horizontalCenter }
                StyledText {
                    text: "Loading your music…"
                    color: Appearance.colors.onSurfaceVariant
                }
            }
        }

        // the spotlight: what is playing, or what played last
        Rectangle {
            id: spotlight
            readonly property bool stacked: width < 460
            readonly property real inset: stacked ? 20 : 26
            x: root.margin
            width: root.width - 2 * root.margin
            height: stacked ? spotlightArt.height + spotlightDetails.implicitHeight + 3 * inset
                            : Math.max(spotlightArt.height, spotlightDetails.implicitHeight) + 2 * inset
            Reveal.shown: root.hasSpotlight && !(Library.loadingHome && Library.recentlyPlayed.count === 0)
            radius: 22
            color: Appearance.colors.surfaceContainerLow
            border.width: 0.75
            border.color: Qt.rgba(1, 1, 1, 0.09)

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: SongMenus.open(root.spotlightSong, false)
            }

            // the cover, blurred, poured behind the card, as end4's media
            // controls do. drawn once a cover, not every frame, and at a
            // quarter of the pixels: under a blur this wide the rest is
            // never seen, and held on the gpu for as long as home is kept
            Item {
                id: spotlightBlurSource
                anchors.fill: parent
                visible: false
                layer.enabled: true
                layer.textureSize: Qt.size(Math.ceil(width / 2), Math.ceil(height / 2))

                // a plain image: ArtworkImage draws through shader effects of
                // its own hidden images, which a hidden layer leaves blank
                Image {
                    readonly property string cover: root.spotlightSong.artworkUrl.toString()

                    anchors.fill: parent
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                    // under that blur a small cover is as good as a big one
                    sourceSize: Qt.size(64, 64)
                    source: cover.length > 0 ? "image://art/" + encodeURIComponent(cover) : ""
                }
            }
            Rectangle {
                id: spotlightMask
                anchors.fill: parent
                radius: 22
                visible: false
                layer.enabled: true
                // a point a pixel is plenty for the edge of a faint wash
                layer.textureSize: Qt.size(Math.ceil(width), Math.ceil(height))
            }
            MultiEffect {
                anchors.fill: parent
                source: spotlightBlurSource
                blurEnabled: true
                blur: 1
                blurMax: 64
                saturation: 0.2
                maskEnabled: true
                maskSource: spotlightMask
                opacity: Appearance.colors.dark ? 0.42 : 0.3
            }

            // a wave along the bottom, flowing only while this plays
            WavyLine {
                anchors.left: parent.left
                anchors.leftMargin: spotlight.stacked ? spotlight.inset : spotlightDetails.x
                anchors.right: parent.right
                anchors.rightMargin: 26
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                height: 10
                color: Qt.alpha(Appearance.colors.primary, 0.45)
                phaseRate: root.spotlightIsCurrent ? 4 : 0
            }

            Item {
                id: spotlightArt
                x: spotlight.inset
                y: spotlight.inset
                width: spotlight.stacked ? Math.min(144, spotlight.width * 0.4) : 176
                height: width

                ArtworkGlow {
                    anchors.fill: parent
                    source: root.spotlightSong.artworkUrl
                    intensity: 0.22
                    glowRadius: 24
                }
                ArtworkImage {
                    anchors.fill: parent
                    source: root.spotlightSong.artworkUrl
                    crossfade: true
                }
            }

            Column {
                id: spotlightDetails
                x: spotlight.stacked ? spotlight.inset : spotlightArt.x + spotlightArt.width + 24
                y: spotlight.stacked ? spotlightArt.y + spotlightArt.height + spotlight.inset
                                     : (spotlight.height - implicitHeight) / 2
                width: Math.max(0, spotlight.width - x - spotlight.inset)
                spacing: 8

                StyledText {
                    width: parent.width
                    elide: Text.ElideRight
                    text: root.spotlightIsCurrent && Player.playing ? "NOW PLAYING" : "CONTINUE LISTENING"
                    font.pixelSize: parent.width < 180 ? Appearance.fontSize.smallest : Appearance.fontSize.smaller
                    font.weight: Font.Bold
                    font.letterSpacing: 0.7
                    color: Appearance.colors.primary
                }
                // one line, however long the name: the card keeps its shape
                TitleText {
                    width: parent.width
                    text: root.spotlightSong.name
                    maximumLineCount: 1
                    wrapMode: Text.NoWrap
                    elide: Text.ElideRight
                    font.pixelSize: Appearance.fontSize.headline
                    font.weight: Font.Bold
                    font.letterSpacing: -0.5
                }
                StyledText {
                    width: parent.width
                    text: root.spotlightSong.artists
                    elide: Text.ElideRight
                    font.pixelSize: Appearance.fontSize.larger
                    font.weight: Font.Medium
                    color: Appearance.colors.onSurfaceVariant
                }
                StyledText {
                    width: parent.width
                    visible: root.spotlightSong.albumName.length > 0
                    text: root.spotlightSong.albumName
                    elide: Text.ElideRight
                    color: Appearance.colors.outline
                }

                Row {
                    id: resume
                    width: parent.width

                    function play() {
                        if (root.spotlightIsCurrent) {
                            Player.togglePlayback()
                        } else if (Library.recentlyPlayed.count > 0) {
                            Player.playFromModel(Library.recentlyPlayed, Library.recentlyPlayed.indexOfInstance(root.spotlightSong.instanceId))
                        } else {
                            Player.playSong(root.spotlightSong)
                        }
                    }

                    topPadding: 6
                    spacing: 12

                    // the play button, as in the player, with what it does beside it
                    PlayButton {
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: resume.width < 160 ? 36 : 52
                        implicitHeight: implicitWidth
                        playing: root.spotlightIsCurrent && Player.playing
                        tooltip: resumeLabel.visible ? "" : resumeLabel.text
                        onClicked: resume.play()
                    }
                    // as wide as its longest word, so the button beside it
                    // stays put whichever it says
                    StyledText {
                        id: resumeLabel
                        visible: resume.width >= 176
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.ceil(Math.max(labelMetrics.advanceWidth("Pause"), labelMetrics.advanceWidth("Resume"),
                                                  labelMetrics.advanceWidth("Play")))
                        text: root.spotlightIsCurrent ? (Player.playing ? "Pause" : "Resume") : "Play"
                        font.pixelSize: Appearance.fontSize.normal
                        font.weight: Font.DemiBold
                        animateChange: true

                        FontMetrics {
                            id: labelMetrics
                            font: resumeLabel.font
                        }

                        TapHandler {
                            onTapped: resume.play()
                        }
                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }
                    }
                    IconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        symbol: "more_horiz"
                        size: resume.width < 160 ? 28 : 36
                        onClicked: SongMenus.open(root.spotlightSong, false)
                    }
                }
            }
        }

        Flow {
            x: root.margin
            width: root.width - 2 * root.margin
            spacing: 12
            Reveal.shown: !(Library.loadingHome && Library.recentlyPlayed.count === 0)

            readonly property bool sideBySide: width >= 520

            HomeShortcut {
                width: parent.sideBySide ? (parent.width - 12) / 2 : parent.width
                title: "Favourite Songs"
                // spotify's count, until the list itself has loaded
                subtitle: Library.savedSongTotal >= 0 ? Library.savedSongTotal + " favourited" : "Made for repeat"
                symbol: "star"
                iconShape: "heart"
                keepsShape: true
                onClicked: Library.showSavedSongs()
            }
            HomeShortcut {
                width: parent.sideBySide ? (parent.width - 12) / 2 : parent.width
                title: "Your Playlists"
                subtitle: Library.playlists.count + " collections to explore"
                symbol: "library_music"
                iconShape: "bun"
                onClicked: Library.showPlaylists()
            }
        }

        Column {
            x: root.margin
            width: root.width - 2 * root.margin
            spacing: 8

            Item {
                width: parent.width
                height: 40
                SectionHeader {
                    width: parent.width - refreshCollections.width - 12
                    title: qsTr("Made for You")
                    symbol: "auto_awesome"
                    showsSeeAll: Library.personalizedPlaylists.count > 0
                    onSeeAll: Library.showPersonalized()
                }
                IconButton {
                    id: refreshCollections
                    anchors.right: parent.right
                    symbol: "refresh"
                    tooltip: qsTr("Refresh collections")
                    enabled: !Library.loadingDiscovery
                    onClicked: Library.refreshDiscovery()
                }
            }
            StyledText {
                width: parent.width
                wrapMode: Text.Wrap
                color: Appearance.colors.onSurfaceVariant
                visible: Library.loadingDiscovery || Library.discoveryStatus.length > 0 || Library.homeSections.length === 0
                text: Library.loadingDiscovery ? qsTr("Loading your mixes…")
                    : Library.discoveryStatus.length > 0 ? qsTranslate("Discovery", Library.discoveryStatus)
                    : qsTr("Your mixes, blends and Spotify collections appear here when available for your account.")
            }
        }

        Repeater {
            model: Library.homeSections
            delegate: Shelf {
                required property var modelData
                width: root.width
                onScreen: root.reaches(column.y + y, height, 0)
                nearScreen: root.reaches(column.y + y, height, root.height / 2)
                title: modelData.title.length > 0 ? modelData.title : qsTr("Made for You")
                symbol: "auto_awesome"
                iconShape: "sunny"
                model: modelData.items
                delegate: MediaCard {
                    required property var modelData
                    readonly property playlist playlist: modelData
                    artworkUrl: playlist.artworkUrl
                    title: playlist.name
                    subtitle: playlist.ownerDisplayName
                    onClicked: Library.selectPlaylist(playlist)
                }
            }
        }

        Shelf {
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            Reveal.shown: Library.pinnedPlaylists.count > 0
            title: "Pinned Playlists"
            symbol: "push_pin"
            iconShape: "flower"
            showsSeeAll: true
            onSeeAll: Library.showPinnedPlaylists()
            model: Math.min(8, Library.pinnedPlaylists.count)
            delegate: MediaCard {
                required property int index
                readonly property playlist playlist: Library.pinnedPlaylists.at(index)
                artworkUrl: playlist.artworkUrl
                title: playlist.name
                subtitle: playlist.ownerDisplayName
                onClicked: Library.selectPlaylist(playlist)
            }
        }

        Shelf {
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            Reveal.shown: Library.playlists.count > 0
            title: "Your Playlists"
            symbol: "queue_music"
            iconShape: "bun"
            showsSeeAll: true
            onSeeAll: Library.showPlaylists()
            model: Math.min(8, Library.playlists.count)
            delegate: MediaCard {
                required property int index
                readonly property playlist playlist: Library.playlists.at(index)
                artworkUrl: playlist.artworkUrl
                title: playlist.name
                subtitle: playlist.ownerDisplayName
                onClicked: Library.selectPlaylist(playlist)
            }
        }

        Shelf {
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            Reveal.shown: Library.recentlyPlayed.count > 0
            title: "Recently Played"
            symbol: "history"
            iconShape: "clamShell"
            model: Library.recentlyPlayed
            delegate: MediaCard {
                required property int index
                required property song song
                artworkUrl: song.artworkUrl
                title: song.name
                subtitle: song.artists
                current: Player.hasSong && Player.currentSong.instanceId === song.instanceId
                badgeSymbol: current && Player.playing ? "graphic_eq" : "play_arrow"
                onClicked: Player.playFromModel(Library.recentlyPlayed, index)
                onSecondaryClicked: SongMenus.open(song, false)
            }
        }

        Shelf {
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            Reveal.shown: Library.recommendedArtists.count > 0
            title: "Artists in Your Rotation"
            symbol: "person"
            iconShape: "cookie12"
            model: Math.min(10, Library.recommendedArtists.count)
            delegate: MediaCard {
                required property int index
                readonly property artist artist: Library.recommendedArtists.at(index)
                artworkUrl: artist.artworkUrl
                title: artist.name
                circular: true
                titleLines: 1
                badgeSymbol: "chevron_right"
                onClicked: Library.selectArtist(artist)
            }
        }

        Shelf {
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            Reveal.shown: Library.recommendedAlbums.count > 0
            title: "Albums From Your Listening"
            symbol: "album"
            iconShape: "sunny"
            model: Math.min(10, Library.recommendedAlbums.count)
            delegate: MediaCard {
                required property int index
                readonly property album album: Library.recommendedAlbums.at(index)
                artworkUrl: album.artworkUrl
                title: album.name
                subtitle: album.artists
                onClicked: Library.selectAlbum(album)
            }
        }

        Shelf {
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            title: "Explore Genres"
            symbol: "explore"
            iconShape: "clover8"
            rowHeight: 120
            model: genres.featured
            delegate: GenreCard {
                required property var modelData
                name: modelData.name
                symbol: modelData.symbol
                startColor: modelData.start
                endColor: modelData.end
                onClicked: root.openGenre(name)
            }
        }
    }
}
