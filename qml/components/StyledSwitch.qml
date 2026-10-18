pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Swiftify

// the material 3 switch: the thumb grows when on, and squeezes while held.
T.Switch {
    id: root

    property MaterialScheme scheme: Appearance.colors

    implicitWidth: 52
    implicitHeight: 32
    hoverEnabled: true
    opacity: enabled ? 1 : 0.38

    indicator: Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? root.scheme.primary : root.scheme.surfaceContainerHighest
        border.width: root.visualFocus ? 2 : (root.checked ? 0 : 2)
        border.color: root.visualFocus ? root.scheme.secondary : root.scheme.outline

        Behavior on color {
            ElementMoveFastColor {}
        }

        Rectangle {
            property real size: root.pressed ? 28 : (root.checked ? 24 : 16)
            width: size
            height: size
            radius: size / 2
            anchors.verticalCenter: parent.verticalCenter
            x: root.checked ? parent.width - width - (parent.height - width) / 2 : (parent.height - width) / 2
            color: root.checked ? root.scheme.onPrimary : root.scheme.outline

            Behavior on x {
                ElementMoveSmall {}
            }
            Behavior on size {
                ElementMoveFast {}
            }
            Behavior on color {
                ElementMoveFastColor {}
            }
        }
    }

    contentItem: Item {}

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
