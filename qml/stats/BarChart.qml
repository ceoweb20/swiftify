pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// plays over the hours of a day or the days of a week: a rounded bar for
// each, the busiest in the accent colour, and the figure on hover
Item {
    id: root

    property list<int> values
    property list<string> labels
    /// a label under every nth bar; the rest are left out to keep it legible
    property int labelEvery: 1
    /// what a bar's figure is called on hover, after the label
    property string unit: "plays"
    property real chartHeight: 140

    readonly property int maximum: {
        let most = 0
        for (let index = 0; index < values.length; ++index) {
            most = Math.max(most, values[index])
        }
        return most
    }
    readonly property real gap: values.length > 12 ? 4 : 10
    readonly property real barWidth: values.length > 0 ? Math.max(2, (width - gap * (values.length - 1)) / values.length) : 0

    implicitHeight: chartHeight + 26

    Repeater {
        model: root.values.length

        Item {
            id: column

            required property int index
            readonly property int value: root.values[index]
            readonly property bool peak: value > 0 && value === root.maximum

            x: index * (root.barWidth + root.gap)
            width: root.barWidth
            height: root.height

            Rectangle {
                id: track
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 26
                width: parent.width
                height: root.chartHeight
                radius: Math.min(width / 2, 10)
                color: Qt.alpha(Appearance.colors.onSurface, 0.04)
            }
            Rectangle {
                anchors.bottom: track.bottom
                width: parent.width
                height: root.maximum > 0 ? Math.max(column.value > 0 ? 6 : 0, root.chartHeight * column.value / root.maximum) : 0
                radius: Math.min(width / 2, 10)
                color: column.peak ? Appearance.colors.primary
                     : hover.hovered ? Appearance.colors.tertiary
                     : Qt.alpha(Appearance.colors.primary, 0.45)

                Behavior on height {
                    enabled: !Appearance.performanceMode
                    NumberAnimation { duration: Appearance.motion(550); easing.type: Easing.OutCubic }
                }
                Behavior on color {
                    ColorAnimation { duration: Appearance.duration(120) }
                }
            }
            StyledText {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                visible: column.index % root.labelEvery === 0 && column.index < root.labels.length
                text: column.index < root.labels.length ? root.labels[column.index] : ""
                font.pixelSize: Appearance.fontSize.smallest
                color: column.peak ? Appearance.colors.primary : Appearance.colors.outline
            }

            HoverHandler {
                id: hover
            }
            // built only while hovered, as the buttons' own are
            Loader {
                active: hover.hovered
                sourceComponent: StyledToolTip {
                    visible: true
                    delay: 0
                    text: (column.index < root.labels.length ? root.labels[column.index] + " · " : "")
                          + column.value.toLocaleString(Format.locale, "f", 0) + " " + root.unit
                }
            }
        }
    }
}
