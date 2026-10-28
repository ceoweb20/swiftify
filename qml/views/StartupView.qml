pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// while the saved login is checked (SpotifyStartupLoadingView)
Item {
    Rectangle {
        id: panel
        objectName: "startupStatusPanel"
        anchors.centerIn: parent
        width: Math.min(484, parent.width - 2 * Appearance.sizes.pageMargin)
        height: status.implicitHeight + 64
        radius: 28
        color: Appearance.colors.surfaceContainerHigh
        border.width: 1
        border.color: Qt.alpha(Appearance.colors.outlineVariant, 0.4)
    }

    Column {
        id: status
        anchors.centerIn: parent
        width: panel.width - 48
        spacing: 20

        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 64
            height: 64

            Image {
                anchors.fill: parent
                source: "qrc:/qt/qml/Swiftify/resources/swiftify-icon.png"
                sourceSize: Qt.size(128, 128)
                smooth: true
            }
        }

        TitleText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Swiftify"
            font.pixelSize: Appearance.fontSize.headline
            font.weight: Font.Bold
        }

        LoadingSpinner {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 44
            height: 44
        }

        StyledText {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Checking your saved Spotify login…")
            color: Appearance.colors.onSurfaceVariant
        }
    }
}
