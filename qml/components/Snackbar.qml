pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// the error banner (ErrorBanner in the swift app) as a material snackbar: an
// error container with selectable text and a close button. it goes on its own
// after eight seconds.
Item {
    id: root

    property string message
    property int serial: 0
    readonly property bool shown: message.length > 0
    signal dismissed()

    implicitWidth: Math.min(560, row.implicitWidth + 32)
    implicitHeight: row.implicitHeight + 24
    visible: opacity > 0
    opacity: shown ? 1 : 0
    scale: shown ? 1 : 0.94

    Behavior on opacity {
        NumberAnimation { duration: Appearance.duration(root.shown ? 200 : 150) }
    }
    Behavior on scale {
        ElementMoveSmall {}
    }

    onSerialChanged: if (shown && visible) timer.restart()

    Timer {
        id: timer
        interval: 8000
        running: root.shown && root.visible && !Session.evaluatingStoredLogin
        onTriggered: root.dismissed()
    }

    Rectangle {
        anchors.fill: parent
        radius: Appearance.rounding.normal
        color: Appearance.colors.errorContainer
    }

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        x: 16
        width: parent.width - 24
        spacing: 12

        ShapeIcon {
            anchors.verticalCenter: parent.verticalCenter
            size: 32
            iconSize: 18
            shape: "softBoom"
            symbol: "warning"
            color: Appearance.colors.error
            iconColor: Appearance.colors.onError
        }

        TextEdit {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, 440)
            text: root.message
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            color: Appearance.colors.onErrorContainer
            selectionColor: Appearance.colors.error
            selectedTextColor: Appearance.colors.onError
            font.family: Appearance.fontFamily
            font.pixelSize: Appearance.fontSize.small
        }

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            symbol: "close"
            size: 32
            iconSize: 20
            iconColor: Appearance.colors.onErrorContainer
            onClicked: root.dismissed()
        }
    }
}
