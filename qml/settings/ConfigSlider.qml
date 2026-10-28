pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Swiftify

// a setting with a range: its name and current value above, the slider below.
Column {
    id: root

    property string text
    property real value
    property real from: 0
    property real to: 1
    property real stepSize: 0
    property string valueText: String(Math.round(value))
    signal moved(real value)

    width: parent ? parent.width : 0
    topPadding: 8
    bottomPadding: 8
    spacing: 4
    opacity: enabled ? 1 : 0.38

    Item {
        width: parent.width
        height: Math.max(24, caption.implicitHeight, amount.implicitHeight)

        StyledText {
            id: caption
            x: 12
            width: Math.max(0, amount.x - x - 12)
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            wrapMode: Text.Wrap
        }
        StyledText {
            id: amount
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: root.valueText
            tabularNumbers: true
            font.pixelSize: Appearance.fontSize.smallie
            color: Appearance.colors.onSurfaceVariant
        }
    }

    StyledSlider {
        x: 12
        width: parent.width - 24
        from: root.from
        to: root.to
        stepSize: root.stepSize
        snapMode: T.Slider.SnapAlways
        value: root.value
        trackThickness: 12
        onMoved: root.moved(value)
    }
}
