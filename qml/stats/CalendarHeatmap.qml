pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// the last year a day a square, a week a column with monday at the top, each
// square as deep in the accent colour as that day was busy, as on a
// contribution graph
Item {
    id: root

    /// a list for each week of seven play counts, -1 for days to come
    property var weeks: []
    /// {week, label} where each month begins
    property var months: []
    property int maximum: 0

    readonly property real dayLabels: 30
    readonly property real gap: 3
    readonly property real cell: weeks.length > 0
        ? Math.max(6, Math.min(16, (width - dayLabels - gap * (weeks.length - 1)) / weeks.length)) : 12
    readonly property real gridWidth: weeks.length * (cell + gap) - gap

    /// how deep a day's square is: none, then four steps up to the busiest
    function level(count: int): int {
        if (count <= 0 || maximum <= 0) {
            return 0
        }
        return Math.min(4, Math.ceil(4 * count / maximum))
    }
    function dateOf(week: int, day: int): date {
        const today = new Date()
        // the last column is the week of today, starting monday
        const weekday = (today.getDay() + 6) % 7
        const monday = new Date(today.getFullYear(), today.getMonth(), today.getDate() - weekday)
        return new Date(monday.getFullYear(), monday.getMonth(),
                        monday.getDate() - 7 * (weeks.length - 1 - week) + day)
    }

    implicitHeight: 20 + 7 * (cell + gap) + 28

    // the months over their first week
    Repeater {
        model: root.months

        StyledText {
            required property var modelData
            x: root.dayLabels + modelData.week * (root.cell + root.gap)
            y: 0
            text: modelData.label
            font.pixelSize: Appearance.fontSize.smallest
            color: Appearance.colors.outline
        }
    }

    // monday, wednesday and friday down the side
    Repeater {
        model: [0, 2, 4]

        StyledText {
            required property int modelData
            y: 20 + modelData * (root.cell + root.gap) + (root.cell - height) / 2
            text: Qt.locale().dayName(modelData + 1, Locale.ShortFormat)
            font.pixelSize: Appearance.fontSize.smallest
            color: Appearance.colors.outline
        }
    }

    Item {
        id: grid
        x: root.dayLabels
        y: 20
        width: root.gridWidth
        height: 7 * (root.cell + root.gap) - root.gap

        Repeater {
            model: root.weeks.length

            Column {
                id: week

                required property int index
                readonly property var days: root.weeks[index]

                x: index * (root.cell + root.gap)
                spacing: root.gap

                Repeater {
                    model: 7

                    Rectangle {
                        required property int index
                        readonly property int count: week.days[index]
                        readonly property int depth: root.level(count)

                        width: root.cell
                        height: root.cell
                        radius: Math.max(2, root.cell / 4)
                        visible: count >= 0
                        color: depth === 0 ? Qt.alpha(Appearance.colors.onSurface, 0.06)
                                           : Qt.alpha(Appearance.colors.primary, [0, 0.3, 0.5, 0.75, 1][depth])
                    }
                }
            }
        }

        // one pointer over the whole grid, rather than one for each of its
        // few hundred squares: the square under it, outlined, and its day
        HoverHandler {
            id: gridHover
        }
        Item {
            id: pointed

            readonly property int week: Math.floor(gridHover.point.position.x / (root.cell + root.gap))
            readonly property int day: Math.floor(gridHover.point.position.y / (root.cell + root.gap))
            readonly property int count: week >= 0 && week < root.weeks.length && day >= 0 && day < 7
                                         ? root.weeks[week][day] : -1

            visible: gridHover.hovered && count >= 0
            x: week * (root.cell + root.gap)
            y: day * (root.cell + root.gap)
            width: root.cell
            height: root.cell

            Rectangle {
                anchors.fill: parent
                anchors.margins: -1.5
                radius: Math.max(2, root.cell / 4) + 1.5
                color: "transparent"
                border.width: 1.5
                border.color: Appearance.colors.onSurface
            }
            StyledToolTip {
                visible: pointed.visible
                delay: 0
                text: (pointed.count === 1 ? qsTr("%n play", "", pointed.count) : qsTr("%n plays", "", pointed.count)) + " · "
                      + root.dateOf(pointed.week, pointed.day).toLocaleDateString(Qt.locale(), Locale.ShortFormat)
            }
        }
    }

    // the key: less, the steps, more
    Row {
        anchors.right: grid.right
        anchors.bottom: parent.bottom
        spacing: 4

        StyledText {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Less")
            font.pixelSize: Appearance.fontSize.smallest
            color: Appearance.colors.outline
        }
        Repeater {
            model: 5

            Rectangle {
                required property int index
                anchors.verticalCenter: parent.verticalCenter
                width: 11
                height: 11
                radius: 3
                color: index === 0 ? Qt.alpha(Appearance.colors.onSurface, 0.06)
                                   : Qt.alpha(Appearance.colors.primary, [0, 0.3, 0.5, 0.75, 1][index])
            }
        }
        StyledText {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("More")
            font.pixelSize: Appearance.fontSize.smallest
            color: Appearance.colors.outline
        }
    }
}
