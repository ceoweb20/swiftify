pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// the artists of a song on one line, each name a link to that artist's page.
// the first names keep all the room they need; the one that runs out of it is
// cut short, and the ones after it are left out. a song that lists no artist
// ids (only a line of names) is one link, to the first artist Spotify names
// for it.
Item {
    id: root

    property song song
    property color color: Appearance.colors.onSurfaceVariant
    property color hoverColor: Appearance.colors.onSurface
    property int pixelSize: Appearance.fontSize.smaller
    property int weight: Font.Normal

    readonly property list<artist> artists: song.artistItems
    readonly property bool linked: artists.length > 0
    /// the width of every name in full, whatever room there is
    readonly property real naturalWidth: Math.ceil(metrics.advanceWidth)
    /// the least room a name must have left to be shown at all
    readonly property real leastRoom: 36

    function joinedNames(): string {
        if (!linked) {
            return song.artists
        }
        let names = []
        for (let index = 0; index < artists.length; ++index) {
            names.push(artists[index].name)
        }
        return names.join(", ")
    }

    implicitWidth: naturalWidth
    implicitHeight: metrics.height
    clip: true

    TextMetrics {
        id: metrics
        font.family: Appearance.fontFamily
        font.pixelSize: root.pixelSize
        font.weight: root.weight
        text: root.joinedNames()
    }

    Row {
        visible: root.linked

        Repeater {
            model: root.artists

            delegate: Row {
                id: entry

                required property artist modelData
                required property int index
                /// what is left of the line from where this name would start
                readonly property real room: root.width - x
                readonly property real wanted: (index > 0 ? separator.implicitWidth : 0) + label.implicitWidth

                visible: index === 0 || room >= separator.implicitWidth + root.leastRoom
                width: visible ? Math.min(wanted, Math.max(0, room)) : 0

                StyledText {
                    id: separator
                    visible: entry.index > 0
                    text: ", "
                    color: root.color
                    font.pixelSize: root.pixelSize
                    font.weight: root.weight
                }
                StyledText {
                    id: label
                    width: Math.max(0, entry.width - (separator.visible ? separator.width : 0))
                    text: entry.modelData.name
                    elide: Text.ElideRight
                    color: link.hovered ? root.hoverColor : root.color
                    font.pixelSize: root.pixelSize
                    font.weight: root.weight
                    font.underline: link.hovered

                    HoverHandler {
                        id: link
                        enabled: entry.modelData.valid
                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        enabled: entry.modelData.valid
                        acceptedButtons: Qt.LeftButton
                        onTapped: UiState.openArtist(entry.modelData)
                    }
                }
            }
        }
    }

    StyledText {
        width: parent.width
        visible: !root.linked
        text: root.song.artists
        elide: Text.ElideRight
        color: nameLink.hovered ? root.hoverColor : root.color
        font.pixelSize: root.pixelSize
        font.weight: root.weight
        font.underline: nameLink.hovered && root.song.artists.length > 0

        HoverHandler {
            id: nameLink
            enabled: root.song.artists.length > 0
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            enabled: root.song.artists.length > 0
            acceptedButtons: Qt.LeftButton
            onTapped: UiState.goToArtist(root.song)
        }
    }
}
