pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Swiftify

// a place in a top list: its rank, its picture, what it is, and how much it
// was heard, over a bar as long as its share of the first place's
ItemDelegate {
    id: root

    property int rank
    /// shown in place of the rank, when set ("2h", for a recent play)
    property string rankLabel
    /// shown in place of the plays and time, when set
    property string detail
    property string name
    property string subtitle
    property url artworkUrl
    property int plays
    property real listenedMs
    property real share
    property bool circular: false
    property bool current: false
    property string placeholderSymbol: "music_note"
    /// "8 plays" first, or the time first
    property bool timeFirst: false

    signal secondaryClicked()

    implicitHeight: 60
    hoverEnabled: true
    leftPadding: 12
    rightPadding: 14

    background: Rectangle {
        radius: 12
        color: root.current ? Qt.alpha(Appearance.colors.primary, 0.08)
             : root.hovered ? Qt.alpha(Appearance.colors.onSurface, 0.05)
             : Qt.alpha(Appearance.colors.onSurface, 0)

        Behavior on color {
            ColorAnimation { duration: Appearance.duration(120) }
        }

        // the share, a soft wash from the left
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * Math.max(0, Math.min(1, root.share))
            radius: parent.radius
            color: Qt.alpha(Appearance.colors.primary, root.rank <= 3 && root.rankLabel.length === 0 ? 0.10 : 0.06)

            Behavior on width {
                enabled: !Appearance.performanceMode
                NumberAnimation { duration: Appearance.motion(500); easing.type: Easing.OutCubic }
            }
        }
    }

    contentItem: Item {
        StyledText {
            id: rankText
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 30
            horizontalAlignment: Text.AlignHCenter
            readonly property bool podium: root.rankLabel.length === 0 && root.rank <= 3
            text: root.rankLabel.length > 0 ? root.rankLabel : root.rank
            font.pixelSize: podium ? Appearance.fontSize.larger
                          : root.rankLabel.length > 0 ? Appearance.fontSize.smallest : Appearance.fontSize.normal
            font.weight: podium ? Font.Bold : Font.DemiBold
            color: podium ? Appearance.colors.primary : Appearance.colors.onSurfaceVariant
        }

        ArtworkImage {
            id: art
            anchors.left: rankText.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 42
            height: 42
            radius: 8
            circular: root.circular
            source: root.artworkUrl
            placeholderSymbol: root.placeholderSymbol
            squiggly: false
        }

        Column {
            anchors.left: art.right
            anchors.leftMargin: 14
            anchors.right: figures.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            StyledText {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                font.weight: Font.DemiBold
                color: root.current ? Appearance.colors.primary : Appearance.colors.onSurface
            }
            StyledText {
                width: parent.width
                visible: root.subtitle.length > 0
                text: root.subtitle
                elide: Text.ElideRight
                font.pixelSize: Appearance.fontSize.smaller
                color: Appearance.colors.onSurfaceVariant
            }
        }

        Column {
            id: figures
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            // a count the source left out (-1) shows as nothing, not as none
            readonly property string playsText: root.plays < 0 ? ""
                : root.plays === 1 ? qsTr("%n play", "", root.plays) : qsTr("%n plays", "", root.plays)
            readonly property string timeText: Format.totalDuration(root.listenedMs)
            readonly property string first: root.timeFirst && timeText.length > 0 ? timeText
                                          : playsText.length > 0 ? playsText : timeText

            StyledText {
                anchors.right: parent.right
                text: root.detail.length > 0 ? root.detail
                    : figures.first
                font.pixelSize: Appearance.fontSize.smallie
                font.weight: Font.DemiBold
                color: Appearance.colors.onSurface
            }
            StyledText {
                anchors.right: parent.right
                readonly property string other: figures.first === figures.timeText ? figures.playsText : figures.timeText
                visible: root.detail.length === 0 && other.length > 0 && other !== figures.first
                text: other
                font.pixelSize: Appearance.fontSize.smallest
                color: Appearance.colors.outline
            }
        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: root.secondaryClicked()
    }
}
