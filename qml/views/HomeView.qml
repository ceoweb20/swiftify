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

// Listen Now (MusicHomeView)
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

        Item {
            id: intro
            x: root.margin
            width: root.width - 2 * root.margin
            readonly property bool wide: width >= 520
            implicitHeight: homeTitle.height + (wide ? 0 : greeting.height + 8)

            TitleText {
                id: homeTitle
                text: "Swiftify"
                font.pixelSize: Appearance.fontSize.display
                font.weight: Font.Bold
                font.letterSpacing: -0.8
            }
            // a greeting for the time of day, on a shape to match it
            Row {
                id: greeting
                x: intro.wide ? intro.width - width : 0
                y: intro.wide ? (homeTitle.height - height) / 2 : homeTitle.height + 8

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
                    text: [qsTr("Good morning"), qsTr("Good afternoon"), qsTr("Good evening"), qsTr("Up late...")][greeting.part]
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
            Reveal.shown: Library.loadingHome && !root.hasSpotlight
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0

            Column {
                anchors.centerIn: parent
                spacing: 12
                LoadingSpinner { anchors.horizontalCenter: parent.horizontalCenter }
                StyledText {
                    text: qsTr("Loading your music…")
                    color: Appearance.colors.onSurfaceVariant
                }
            }
        }

        // the spotlight: what is playing, or what played last
        Rectangle {
            id: spotlight
            objectName: "homeListeningWorkspace"
            readonly property bool hasFocus: root.hasSpotlight
            readonly property bool stacked: width < 460
            readonly property real inset: stacked ? 20 : 26
            readonly property real musicHeight: stacked ? spotlightArt.height + spotlightDetails.implicitHeight + 3 * inset
                                                      : Math.max(spotlightArt.height, spotlightDetails.implicitHeight) + 2 * inset
            x: root.margin
            width: root.width - 2 * root.margin
            height: djContent.y + djContent.implicitHeight + inset
            radius: 28
            color: Appearance.colors.surfaceContainerHigh
            border.width: 0.75
            border.color: Qt.alpha(Appearance.colors.outlineVariant, 0.35)

            Item {
                id: listeningFocus
                objectName: "homeListeningFocus"
                width: parent.width
                height: spotlight.musicHeight
                Reveal.shown: spotlight.hasFocus
                Reveal.scaleFrom: 1
                Reveal.duration: Appearance.duration(220)
                Reveal.overshoot: 0

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: SongMenus.open(root.spotlightSong, false)
                }

                // a wave along the bottom, flowing only while this plays
                WavyLine {
                    anchors.left: parent.left
                    anchors.leftMargin: spotlight.stacked ? spotlight.inset : spotlightDetails.x
                    anchors.right: parent.right
                    anchors.rightMargin: 26
                    y: spotlight.musicHeight - 20
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
                                         : (spotlight.musicHeight - implicitHeight) / 2
                    width: Math.max(0, spotlight.width - x - spotlight.inset)
                    spacing: 8

                    StyledText {
                        width: parent.width
                        elide: Text.ElideRight
                        text: root.spotlightIsCurrent && Player.playing ? qsTr("NOW PLAYING") : qsTr("CONTINUE LISTENING")
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
                        color: Appearance.colors.onSurfaceVariant
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
                            width: Math.ceil(Math.max(labelMetrics.advanceWidth(qsTr("Pause")), labelMetrics.advanceWidth(qsTr("Resume")),
                                                      labelMetrics.advanceWidth(qsTr("Play"))))
                            text: root.spotlightIsCurrent ? (Player.playing ? qsTr("Pause") : qsTr("Resume")) : qsTr("Play")
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

            Rectangle {
                x: spotlight.inset
                y: spotlight.musicHeight
                width: spotlight.width - 2 * spotlight.inset
                height: 1
                visible: spotlight.hasFocus
                color: Qt.alpha(Appearance.colors.outlineVariant, 0.5)
            }

            Column {
                id: djContent
                objectName: "homeDjControls"
                x: spotlight.inset
                y: spotlight.hasFocus ? spotlight.musicHeight + 16 : spotlight.inset
                width: spotlight.width - 2 * spotlight.inset
                spacing: 8

                Flow {
                    width: parent.width
                    spacing: 8
                    TextButton {
                        objectName: "startDjMix"
                        variant: TextButton.Tonal
                        symbol: "auto_awesome"
                        text: DJMix.busy ? qsTr("Building your mix…")
                              : DJMix.active ? qsTr("New DJ Mix") : qsTr("Start DJ Mix")
                        enabled: !DJMix.busy
                        onClicked: DJMix.start()
                    }
                    TextButton {
                        visible: DJMix.busy
                        variant: TextButton.Text
                        text: qsTr("Cancel")
                        onClicked: DJMix.cancel()
                    }
                }
                StyledText {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("A personal queue from your Spotify mixes, favourites and recent listening. Swiftify's mix, not Spotify AI DJ.")
                    color: Appearance.colors.onSurfaceVariant
                    font.pixelSize: Appearance.fontSize.smaller
                }
                StyledText {
                    width: parent.width
                    visible: DJMix.status.length > 0
                    wrapMode: Text.Wrap
                    text: qsTranslate("DJMix", DJMix.status)
                    color: Appearance.colors.onSurfaceVariant
                }
            }
        }

        Rectangle {
            objectName: "homeDestinations"
            x: root.margin
            width: root.width - 2 * root.margin
            implicitHeight: shortcuts.implicitHeight + 24
            radius: 24
            color: Appearance.colors.surfaceContainerLow
            border.width: 1
            border.color: Qt.alpha(Appearance.colors.outlineVariant, 0.4)
            Reveal.shown: root.hasSpotlight || !Library.loadingHome
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0

            Flow {
                id: shortcuts
                x: 12
                y: 12
                width: parent.width - 24
                spacing: 8
                readonly property bool sideBySide: width >= 420

                HomeShortcut {
                    width: shortcuts.sideBySide ? (shortcuts.width - shortcuts.spacing) / 2 : shortcuts.width
                    implicitHeight: 56
                    title: qsTr("Favourite Songs")
                    // spotify's count, until the list itself has loaded
                    subtitle: Library.savedSongTotal >= 0 ? qsTr("Favourited: %1").arg(Library.savedSongTotal) : qsTr("Made for repeat")
                    symbol: "star"
                    iconShape: "heart"
                    keepsShape: true
                    onClicked: Library.showSavedSongs()
                }
                HomeShortcut {
                    width: shortcuts.sideBySide ? (shortcuts.width - shortcuts.spacing) / 2 : shortcuts.width
                    implicitHeight: 56
                    title: qsTr("Your Playlists")
                    subtitle: qsTr("Collections to explore: %1").arg(Library.playlists.count)
                    symbol: "library_music"
                    iconShape: "bun"
                    onClicked: Library.showPlaylists()
                }
            }
        }



        Shelf {
            pageScroller: pageWheel
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            Reveal.shown: Library.pinnedPlaylists.count > 0
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0
            title: qsTr("Pinned Playlists")
            symbol: "push_pin"
            iconShape: "flower"
            showsSeeAll: true
            onSeeAll: Library.showPinnedPlaylists()
            model: Math.min(8, Library.pinnedPlaylists.count)
            delegate: MediaCard {
                required property int index
                readonly property playlist playlist: Library.pinnedPlaylists.at(index)
                artworkUrl: playlist.artworkUrl
                fallbackKey: playlist.id.length > 0 ? playlist.id : playlist.name
                title: playlist.name
                subtitle: playlist.ownerDisplayName
                onClicked: Library.selectPlaylist(playlist)
            }
        }

        Shelf {
            pageScroller: pageWheel
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            Reveal.shown: Library.playlists.count > 0
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0
            title: qsTr("Your Playlists")
            symbol: "queue_music"
            iconShape: "bun"
            showsSeeAll: true
            onSeeAll: Library.showPlaylists()
            model: Math.min(8, Library.playlists.count)
            delegate: MediaCard {
                required property int index
                readonly property playlist playlist: Library.playlists.at(index)
                artworkUrl: playlist.artworkUrl
                fallbackKey: playlist.id.length > 0 ? playlist.id : playlist.name
                title: playlist.name
                subtitle: playlist.ownerDisplayName
                onClicked: Library.selectPlaylist(playlist)
            }
        }

        Shelf {
            pageScroller: pageWheel
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            Reveal.shown: Library.recentlyPlayed.count > 0
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0
            title: qsTr("Recently Played")
            symbol: "history"
            iconShape: "clamShell"
            model: Library.recentlyPlayed
            delegate: MediaCard {
                required property int index
                required property song song
                artworkUrl: song.artworkUrl
                title: song.name
                songTitle: true
                subtitle: song.artists
                current: Player.hasSong && Player.currentSong.instanceId === song.instanceId
                badgeSymbol: current && Player.playing ? "graphic_eq" : "play_arrow"
                onClicked: Player.playFromModel(Library.recentlyPlayed, index)
                onSecondaryClicked: SongMenus.open(song, false)
            }
        }

        Shelf {
            pageScroller: pageWheel
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            Reveal.shown: Library.recommendedArtists.count > 0
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0
            title: qsTr("Artists in Your Rotation")
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
            pageScroller: pageWheel
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            Reveal.shown: Library.recommendedAlbums.count > 0
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0
            title: qsTr("Albums From Your Listening")
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
            pageScroller: pageWheel
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            title: qsTr("Explore Genres")
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
