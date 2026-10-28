pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// Favourite Songs (SavedSongsView)
SongListPage {
    id: root

    songs: Library.savedSongs
    loading: Library.loadingSavedSongs && Library.savedSongs.count === 0
    showsEmpty: Library.savedSongs.count === 0
    emptySymbol: "star"
    emptyTitle: qsTr("No Saved Songs")
    emptyDescription: qsTr("Save tracks from Search or a playlist.")

    hero: CollectionHero {
        kind: "PLAYLIST"
        title: qsTr("Favourite Songs")
        facts: Format.joined([Format.songCount(Library.savedSongs.count), Format.totalDuration(Library.savedSongs.totalDurationMs)], "  •  ")

        artwork: Rectangle {
            anchors.fill: parent
            radius: Appearance.sizes.artworkRadius
            // in the selected theme's colours, as everything without a cover
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: Appearance.colors.primary
                }
                GradientStop {
                    position: 1
                    color: Appearance.colors.tertiary
                }
            }

            MaterialSymbol {
                anchors.centerIn: parent
                text: "favorite"
                iconSize: Math.min(96, parent.width * 0.5)
                fill: 1
                color: Appearance.colors.onPrimary
            }
        }

        actions: [
            TextButton {
                iconOnly: parent ? parent.compactActions : false
                compactSize: parent ? parent.actionSize : 48
                implicitHeight: iconOnly ? compactSize : 48
                wavy: true
                symbol: "play_arrow"
                burst: "play"
                text: qsTr("Play")
                enabled: Library.savedSongs.count > 0
                onClicked: Player.playFromModel(Library.savedSongs, 0)
            }
        ]
    }
}
