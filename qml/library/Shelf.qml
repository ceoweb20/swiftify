pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Swiftify

// a titled horizontal row of cards (ResultShelf). it scrolls under the page
// margins to the window's edges.
Column {
    id: root

    property string title
    /// the symbol and shape its heading wears
    property string symbol
    property string iconShape: "cookie9"
    property bool showsSeeAll: false
    property var model
    property Component delegate
    property real pageMargin: Appearance.sizes.pageMargin
    property real rowHeight: Appearance.sizes.cardSize + 64
    /// whether any of it is in sight on its page (the page's reaches()). out
    /// of it, it keeps its place but draws nothing: every frame the window
    /// drew went through each card of every shelf down the page
    property bool onScreen: true
    /// whether it is in sight or nearly, half a screen off: its cards are
    /// built ahead, so a scroll never shows a shelf arriving. further off
    /// they are let go of, as a card hidden rather than gone kept its cover
    /// decoded in memory, never drawn and so never handed to the gpu
    property bool nearScreen: onScreen
    /// where the row was scrolled to, kept while its cards are gone
    property real scrolled: 0

    signal seeAll()

    spacing: 13

    // the heading and the row are hidden inside holders that keep their
    // size, so the page's layout is the same either way
    Item {
        width: root.width
        height: header.implicitHeight

        SectionHeader {
            id: header
            x: root.pageMargin
            width: root.width - 2 * root.pageMargin
            visible: root.onScreen
            title: root.title
            symbol: root.symbol
            iconShape: root.iconShape
            showsSeeAll: root.showsSeeAll
            onSeeAll: root.seeAll()
        }
    }

    Item {
        width: root.width
        height: root.rowHeight

        // built over a few frames as it nears the screen, rather than in the
        // middle of one
        Loader {
            anchors.fill: parent
            active: root.nearScreen
            visible: root.onScreen
            asynchronous: true
            sourceComponent: ListView {
                id: cards
                objectName: "shelfCards"
                orientation: ListView.Horizontal
                spacing: Appearance.sizes.shelfGap
                leftMargin: root.pageMargin
                rightMargin: root.pageMargin
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true
                // no cards built ahead off the edge: each held its cover
                // decoded in memory, never drawn, for as long as the page was kept
                cacheBuffer: 0
                model: root.model
                delegate: root.delegate
                ScrollBar.horizontal: StyledScrollBar {}

                WheelHandler {
                    target: null
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (event) => {
                        const pixel = Math.abs(event.pixelDelta.x) > Math.abs(event.pixelDelta.y)
                                      ? event.pixelDelta.x : event.pixelDelta.y
                        const angle = Math.abs(event.angleDelta.x) > Math.abs(event.angleDelta.y)
                                      ? event.angleDelta.x : event.angleDelta.y
                        const delta = pixel !== 0 ? pixel : angle / 120 * Math.max(48, Appearance.sizes.cardSize / 2)
                        const start = -cards.leftMargin
                        const end = Math.max(start, cards.contentWidth - cards.width + cards.rightMargin)
                        const next = Math.max(start, Math.min(end, cards.contentX - delta))
                        event.accepted = Math.abs(next - cards.contentX) > 0.01
                        if (event.accepted) {
                            cards.cancelFlick()
                            cards.contentX = next
                        }
                    }
                }

                // back where it was; what it reports before then is its start
                property bool restored: false
                Component.onCompleted: {
                    contentX = root.scrolled - leftMargin
                    restored = true
                }
                onContentXChanged: {
                    if (restored) {
                        root.scrolled = contentX + leftMargin
                    }
                }
            }
        }
    }
}
