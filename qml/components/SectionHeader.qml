pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// a shelf's heading: a symbol on a shape, the title drawn round, and a
// squiggle running on to "See All". hovering the heading turns the shape.
Item {
    id: root

    property string title
    property string symbol
    property string iconShape: "cookie9"
    property bool showsSeeAll: false
    signal seeAll()

    implicitHeight: 44
    implicitWidth: titleText.x + titleText.implicitWidth

    HoverHandler {
        id: hover
    }

    ShapeIcon {
        id: badge
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        visible: root.symbol.length > 0
        size: 30
        iconSize: 18
        shape: root.iconShape
        symbol: root.symbol
        hovered: hover.hovered
    }

    TitleText {
        id: titleText
        objectName: "sectionTitle"
        x: badge.visible ? badge.width + 10 : 0
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, Math.max(0, (seeAll.visible ? seeAll.x - 12 : root.width) - x))
        text: root.title
        elide: Text.ElideRight
        font.pixelSize: root.width < 320 ? Appearance.fontSize.larger : Appearance.fontSize.title
        font.weight: Font.Bold
    }

    WavyLine {
        anchors.left: titleText.right
        anchors.leftMargin: 16
        anchors.right: seeAll.visible ? seeAll.left : parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        height: 8
        visible: width > 24
        color: Appearance.colors.outlineVariant
    }

    TextButton {
        id: seeAll
        objectName: "sectionSeeAll"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        visible: root.showsSeeAll
        variant: TextButton.Tonal
        symbol: "arrow_forward"
        text: qsTr("See All")
        iconOnly: root.width < titleText.x + titleText.implicitWidth + naturalWidth + 24
        compactSize: root.width < 280 ? 32 : 40
        onClicked: root.seeAll()
    }
}
