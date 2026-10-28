pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Swiftify

// a shelf or grid card: artwork, a title of up to two lines and a subtitle.
// pressing shrinks it slightly and dims it (MediaCardButtonStyle).
T.AbstractButton {
    id: root

    property url artworkUrl
    /// what shows in place of a cover it has not got
    property string placeholderSymbol: "music_note"
    property string title
    property bool songTitle: false
    property string fallbackKey: title
    property string subtitle
    property bool circular: false
    property bool current: false
    property string badgeSymbol: "arrow_forward"
    property real artworkSize: Appearance.sizes.cardSize
    property int titleLines: 2
    /// hovered, a square cover's edge turns wavy; a round one's squiggle
    /// runs round it instead
    property bool wavyOnHover: !circular

    signal secondaryClicked()

    implicitWidth: artworkSize
    implicitHeight: column.implicitHeight
    hoverEnabled: true
    // click focus would let other mouse buttons grab and press it
    focusPolicy: Qt.TabFocus
    // lifts on hover, squishes on press, and springs between them
    scale: down ? 0.95 : (hovered ? 1.03 : 1)
    opacity: down ? 0.86 : 1

    Behavior on scale {
        enabled: !Appearance.performanceMode
        Bounce {}
    }

    contentItem: Column {
        id: column
        spacing: 8

        MediaArtwork {
            id: artwork
            width: root.artworkSize
            height: root.artworkSize
            source: root.artworkUrl
            placeholderSymbol: root.placeholderSymbol
            fallbackKey: root.fallbackKey
            circular: root.circular
            current: root.current
            badgeSymbol: root.badgeSymbol
            hovered: root.hovered
            wavy: root.wavyOnHover && root.hovered
        }

        // the words are drawn before the cover and its shadow, which only
        // reaches them faintly: drawn after, every card's shadow lay over
        // the next card's words in the order things are drawn, and nothing
        // in a shelf could share a draw with its neighbour
        StyledText {
            objectName: "mediaCardTitle"
            z: -1
            width: root.artworkSize
            text: root.title
            maximumLineCount: root.titleLines
            wrapMode: Text.Wrap
            elide: Text.ElideRight
            horizontalAlignment: root.circular ? Text.AlignHCenter : Text.AlignLeft
            font.pixelSize: Appearance.fontSize.small
            font.weight: root.songTitle ? Font.Bold : Font.DemiBold
            font.variableAxes: root.songTitle ? ({ "ROND": 100, "wdth": 110, "wght": 720 }) : ({})
        }

        StyledText {
            z: -1
            width: root.artworkSize
            visible: root.subtitle.length > 0
            text: root.subtitle
            elide: Text.ElideRight
            horizontalAlignment: root.circular ? Text.AlignHCenter : Text.AlignLeft
            font.pixelSize: Appearance.fontSize.smaller
            color: Appearance.colors.onSurfaceVariant
        }
    }

    // released over the card, it is about to be clicked: the cover sets off
    // for the hero of whatever page the click opens
    onReleased: {
        if (hovered) {
            UiState.launchHero(artwork.cover, artworkUrl, circular)
        }
    }

    // leans a few degrees toward the pointer while hovered, on a spring
    property real tiltAcross: pointer.hovered && !Appearance.performanceMode
                              ? (pointer.point.position.x / Math.max(1, width) - 0.5) * 12 : 0
    property real tiltDown: pointer.hovered && !Appearance.performanceMode
                            ? (0.5 - pointer.point.position.y / Math.max(1, root.artworkSize)) * 10 : 0

    Behavior on tiltAcross {
        SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.01 }
    }
    Behavior on tiltDown {
        SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.01 }
    }

    transform: [
        Rotation {
            origin.x: root.width / 2
            origin.y: root.artworkSize / 2
            axis { x: 0; y: 1; z: 0 }
            angle: root.tiltAcross
        },
        Rotation {
            origin.x: root.width / 2
            origin.y: root.artworkSize / 2
            axis { x: 1; y: 0; z: 0 }
            angle: root.tiltDown
        }
    ]

    HoverHandler {
        id: pointer
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: root.secondaryClicked()
    }
}
