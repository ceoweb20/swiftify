pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Swiftify

// a collection page: a header, then its songs as list rows, with room for a
// loading or empty state in between.
ListView {
    id: root

    // pages run up under the top bar, which floats over them with no
    // background of its own; they start below it
    topMargin: Appearance.sizes.topBarHeight

    property SongListModel songs
    property Component hero
    property bool loading: false
    property string loadingText: qsTr("Loading songs…")
    property bool showsEmpty: false
    property string emptySymbol: "music_note"
    property string emptyTitle
    property string emptyDescription
    property bool allowsPlaylistRemoval: false
    property bool reorderable: false

    readonly property real margin: Appearance.sizes.pageMargin

    model: loading ? null : songs
    boundsBehavior: Flickable.StopAtBounds
    reuseItems: true
    cacheBuffer: 600
    bottomMargin: Appearance.sizes.playerBarClearance
    ScrollBar.vertical: StyledScrollBar { topPadding: 3 + Appearance.sizes.topBarHeight }

    header: Column {
        width: root.width
        // Keep the header behind the rows' hover lift.
        z: 0.5

        Loader {
            width: root.width
            sourceComponent: root.hero
        }

        Rectangle {
            x: root.margin
            width: root.width - 2 * root.margin
            height: 1
            color: Qt.alpha(Appearance.colors.outlineVariant, 0.5)
        }

        Item { width: 1; height: 12 }

        Item {
            width: root.width
            height: 240
            Reveal.shown: root.loading
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0

            Column {
                anchors.centerIn: parent
                spacing: 12
                LoadingSpinner { anchors.horizontalCenter: parent.horizontalCenter }
                StyledText {
                    text: root.loadingText
                    color: Appearance.colors.onSurfaceVariant
                }
            }
        }

        Item {
            width: root.width
            height: 280
            Reveal.shown: !root.loading && root.showsEmpty
            Reveal.scaleFrom: 1
            Reveal.duration: Appearance.duration(220)
            Reveal.overshoot: 0

            EmptyState {
                anchors.centerIn: parent
                width: Math.min(420, parent.width - 2 * root.margin)
                symbol: root.emptySymbol
                title: root.emptyTitle
                description: root.emptyDescription
            }
        }
    }

    delegate: SongRow {
        id: row
        objectName: "collectionSongRow"

        // Full-width delegates keep ListView's cross-axis origin stable;
        // padding and background insets provide the visible page margin.
        x: 0
        width: root.width
        leftPadding: root.margin + 14
        rightPadding: root.margin + 14
        leftInset: root.margin
        rightInset: root.margin
        songs: root.songs
        allowsPlaylistRemoval: root.allowsPlaylistRemoval

        // reordering: hold and drag a row; a line shows where it will land
        DragHandler {
            id: drag
            enabled: root.reorderable
            target: null
            yAxis.enabled: true
            xAxis.enabled: false
            dragThreshold: 6
            onActiveChanged: {
                if (active) {
                    dropMarker.from = row.index
                } else if (dropMarker.from >= 0) {
                    const to = dropMarker.to
                    const from = dropMarker.from
                    dropMarker.from = -1
                    if (to !== from && to !== from + 1) {
                        Library.moveSongs(from, 1, to)
                    }
                }
            }
            onCentroidChanged: {
                if (!active) {
                    return
                }
                const point = row.mapToItem(root.contentItem, centroid.position.x, centroid.position.y)
                const target = root.indexAt(root.width / 2, point.y)
                const targetItem = root.itemAtIndex(target)
                if (target >= 0 && targetItem) {
                    const local = point.y - targetItem.y
                    dropMarker.to = local > targetItem.height / 2 ? target + 1 : target
                    dropMarker.y = (dropMarker.to > target ? targetItem.y + targetItem.height : targetItem.y) - root.contentY + root.originY
                }
            }
        }
    }

    Rectangle {
        id: dropMarker

        property int from: -1
        property int to: -1

        x: root.margin
        width: root.width - 2 * root.margin
        height: 2
        radius: 1
        visible: from >= 0
        color: Appearance.colors.primary
    }
}
