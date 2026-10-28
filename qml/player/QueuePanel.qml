pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Swiftify

// Playing Next (QueuePanel): the current song, then what follows. clicking a
// row jumps to it.
Item {
    id: root

    property MaterialScheme scheme: Appearance.colors

    StyledText {
        id: header
        x: 18
        y: 16
        text: qsTr("Playing Next")
        font.pixelSize: Appearance.fontSize.normal
        font.weight: Font.DemiBold
        color: root.scheme.onSurfaceVariant
    }

    ListView {
        id: list
        PageWheelHandler { page: list }
        anchors.fill: parent
        anchors.topMargin: header.y + header.height + 10
        leftMargin: 12
        rightMargin: 12
        bottomMargin: 20
        clip: true
        model: Player.upNext
        spacing: 2
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        ScrollBar.vertical: StyledScrollBar {}

        header: Column {
            width: list.width - list.leftMargin - list.rightMargin
            spacing: 8

            StyledText {
                Reveal.shown: Player.hasSong
                leftPadding: 6
                text: qsTr("Now Playing")
                font.pixelSize: Appearance.fontSize.smaller
                font.weight: Font.Bold
                color: root.scheme.onSurfaceVariant
            }
            QueueRow {
                width: parent.width
                Reveal.shown: Player.hasSong
                song: Player.currentSong
                current: true
                scheme: root.scheme
            }
            StyledText {
                Reveal.shown: Player.upNext.count > 0
                leftPadding: 6
                topPadding: 8
                bottomPadding: 2
                text: qsTr("Up Next")
                font.pixelSize: Appearance.fontSize.smaller
                font.weight: Font.Bold
                color: root.scheme.onSurfaceVariant
            }
        }

        delegate: QueueRow {
            required property int index
            width: list.width - list.leftMargin - list.rightMargin
            song: Player.upNext.at(index)
            scheme: root.scheme
            onClicked: Player.playFromQueue(song)
        }
    }

    EmptyState {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 40
        width: parent.width - 32
        Reveal.shown: Player.upNext.count === 0
        symbol: "queue_music"
        title: qsTr("Queue Empty")
        description: UiState.catchphrase
    }
}
