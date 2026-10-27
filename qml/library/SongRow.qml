pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Swiftify

// one song in a list (SongListRow): number or play glyph, artwork, title and
// artists, the star and more buttons, and the time. hovered, the card grows
// a little on a spring, and it stays grown while its song is the one playing.
T.AbstractButton {
    id: root

    required property int index
    required property song song
    property SongListModel songs
    property bool showsNumber: true
    property bool fromSearch: false
    property bool allowsPlaylistRemoval: false

    readonly property bool isCurrent: Player.hasSong && Player.currentSong.instanceId === song.instanceId
    readonly property bool saved: Library.savedRevision >= 0 && Library.isSaved(song.uri)
    readonly property bool expanded: hovered || isCurrent

    implicitHeight: 58
    hoverEnabled: true
    // click focus would let other mouse buttons grab and press it
    focusPolicy: Qt.TabFocus
    // a few pixels wider whatever the row's width, over its neighbours. a
    // list puts its delegates and its header at 1; at rest a row stays
    // there, or the header's stickers would be drawn over it
    scale: (expanded ? 1 + Math.min(0.025, 10 / Math.max(width, 1)) : 1) - (down ? 0.012 : 0)
    z: expanded ? 2 : 1

    Behavior on scale {
        enabled: !Appearance.performanceMode
        Bounce {}
    }
    // the row's card starts at the page margin and keeps its content clear
    // of its rounded edge
    leftPadding: 14
    rightPadding: 14

    onClicked: Player.playFromModel(songs, index, fromSearch)

    background: Rectangle {
        radius: 8
        color: root.isCurrent ? Qt.alpha(Appearance.colors.primary, 0.07)
             : root.hovered ? Qt.alpha(Appearance.colors.onSurface, 0.05)
             : Qt.alpha(Appearance.colors.onSurface, 0)

        Behavior on color {
            ColorAnimation { duration: Appearance.duration(120) }
        }
    }

    contentItem: Item {
        Item {
            id: number
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: root.showsNumber ? 22 : 0
            visible: root.showsNumber
            height: parent.height

            // the playing song's mark sits on a little cookie that pulses
            // with the music. built for the playing row alone, and kept
            // only until it has shrunk away: every row had one. whether it
            // still shows is passed on a moment later: read straight off it,
            // letting it go changed what decided to, a binding loop
            Loader {
                id: cookieHolder
                property bool showing: false
                Binding on showing {
                    value: (cookieHolder.item as Item)?.visible ?? false
                    delayed: true
                }
                anchors.centerIn: playingMark
                active: root.isCurrent || showing
                sourceComponent: MaterialShape {
                    id: cookie

                    // made hidden and shown a moment later, so it springs in
                    property bool arrived: false

                    width: 24
                    height: 24
                    Reveal.shown: arrived && root.isCurrent && !root.hovered
                    shape: "cookie6"
                    color: Appearance.colors.primaryContainer
                    spinRate: 30
                    scale: visible ? 1 + 0.12 * Math.sin(UiState.spin * 7) : 1
                    Component.onCompleted: Qt.callLater(() => {
                        // gone already, if the row stopped playing first
                        if (cookie) {
                            cookie.arrived = true
                        }
                    })
                }
            }
            MaterialSymbol {
                id: playingMark
                anchors.right: parent.right
                anchors.rightMargin: -3
                anchors.verticalCenter: parent.verticalCenter
                Reveal.shown: root.hovered || root.isCurrent
                text: root.hovered ? "play_arrow" : "volume_up"
                iconSize: 16
                fill: 1
                color: root.hovered ? Appearance.colors.onSurface : Appearance.colors.onPrimaryContainer
            }

            StyledText {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                Reveal.shown: !root.hovered && !root.isCurrent
                text: root.index + 1
                tabularNumbers: true
                font.pixelSize: Appearance.fontSize.smaller
                color: Appearance.colors.onSurfaceVariant
            }
        }

        Item {
            id: artwork
            anchors.left: number.right
            anchors.leftMargin: root.showsNumber ? 12 : 0
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            height: 44
            opacity: root.song.isPlayable ? 1 : 0.38

            ArtworkImage {
                anchors.fill: parent
                radius: 6
                source: root.song.artworkUrl
            }

            // the playing row's meter, built for it alone, and kept as the
            // cookie is
            Loader {
                id: meterHolder
                property bool showing: false
                Binding on showing {
                    value: (meterHolder.item as Item)?.visible ?? false
                    delayed: true
                }
                anchors.fill: parent
                active: (root.isCurrent && Settings.visualizersEnabled && Settings.trackListVisualizer) || showing
                sourceComponent: Rectangle {
                    id: shade

                    property bool arrived: false

                    Reveal.shown: arrived && root.isCurrent && Settings.visualizersEnabled && Settings.trackListVisualizer
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.34)
                    Component.onCompleted: Qt.callLater(() => {
                        // gone already, if the row stopped playing first
                        if (shade) {
                            shade.arrived = true
                        }
                    })

                    CompactMeter {
                        anchors.centerIn: parent
                        width: 26
                        height: 22
                        active: Player.playing
                    }
                }
            }
        }

        Column {
            anchors.left: artwork.right
            anchors.leftMargin: 12
            anchors.right: actions.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            opacity: root.song.isPlayable ? 1 : 0.38

            Row {
                width: parent.width
                spacing: 5

                StyledText {
                    width: Math.min(implicitWidth, parent.width - (explicitBadge.visible ? 20 : 0))
                    text: root.song.name
                    elide: Text.ElideRight
                    font.pixelSize: Appearance.fontSize.small
                    font.weight: root.isCurrent ? Font.DemiBold : Font.Normal
                    color: root.isCurrent ? Appearance.colors.primary : Appearance.colors.onSurface
                }

                Rectangle {
                    id: explicitBadge
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.song.isExplicit
                    width: 14
                    height: 14
                    radius: 3
                    color: Appearance.colors.surfaceContainerHighest

                    StyledText {
                        anchors.centerIn: parent
                        text: "E"
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        color: Appearance.colors.onSurfaceVariant
                    }
                }
            }

            StyledText {
                width: parent.width
                text: Format.joined([root.song.artists, root.song.albumName], " — ")
                elide: Text.ElideRight
                font.pixelSize: Appearance.fontSize.smaller
                color: Appearance.colors.onSurfaceVariant
            }
        }

        StyledText {
            id: duration
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: root.song.durationMs > 0 ? 42 : 0
            horizontalAlignment: Text.AlignRight
            visible: root.song.durationMs > 0
            text: Format.playbackTime(root.song.durationMs)
            tabularNumbers: true
            font.pixelSize: Appearance.fontSize.smaller
            color: Appearance.colors.onSurfaceVariant
        }

        // the actions, always there, laid out from the time leftwards
        Row {
            id: actions
            anchors.right: parent.right
            anchors.rightMargin: root.song.durationMs > 0 ? 54 : 0
            anchors.verticalCenter: parent.verticalCenter
            layoutDirection: Qt.RightToLeft
            spacing: 2

            move: Transition {
                enabled: !Appearance.performanceMode
                NumberAnimation { properties: "x"; duration: 240; easing.type: Easing.OutBack; easing.overshoot: 0.8 }
            }

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                symbol: "more_horiz"
                size: 28
                iconSize: 18
                tooltip: qsTr("More")
                onClicked: SongMenus.open(root.song, root.allowsPlaylistRemoval)
            }

            IconButton {
                id: star
                anchors.verticalCenter: parent.verticalCenter
                symbol: "star"
                toggled: root.saved
                size: 28
                iconSize: 18
                iconColor: root.saved ? Appearance.colors.primary : Appearance.colors.onSurfaceVariant
                onClicked: {
                    if (!root.saved) {
                        UiState.burst(star, "hearts")
                        if (!Appearance.performanceMode) {
                            starPop.restart()
                        }
                    }
                    Library.toggleSaved(root.song)
                }

                // a sunny pops up behind the star as it is saved
                MaterialShape {
                    id: starBurst
                    anchors.centerIn: parent
                    width: 40
                    height: 40
                    z: -2
                    opacity: 0
                    shape: "sunny"
                    color: Appearance.colors.primaryContainer
                }
                ParallelAnimation {
                    id: starPop
                    NumberAnimation { target: starBurst; property: "scale"; from: 0.3; to: 1.15; duration: 420; easing.type: Easing.OutBack; easing.overshoot: 2 }
                    NumberAnimation { target: starBurst; property: "spinAngle"; from: -40; to: 40; duration: 520; easing.type: Easing.OutCubic }
                    SequentialAnimation {
                        NumberAnimation { target: starBurst; property: "opacity"; from: 0; to: 1; duration: 90 }
                        PauseAnimation { duration: 200 }
                        NumberAnimation { target: starBurst; property: "opacity"; to: 0; duration: 260; easing.type: Easing.InCubic }
                    }
                }

                tooltip: root.saved ? qsTr("Remove from Favourite Songs") : qsTr("Add to Favourite Songs")
            }

        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: SongMenus.open(root.song, root.allowsPlaylistRemoval)
    }
}
