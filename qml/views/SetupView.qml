pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Swiftify

// connecting the library and playback for the first time (SpotifySetupView)
Flickable {
    id: root

    // pages run up under the top bar, which floats over them with no
    // background of its own; they start below it
    topMargin: Appearance.sizes.topBarHeight

    contentHeight: Math.max(height, column.y + column.implicitHeight + 36)
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: StyledScrollBar { topPadding: 3 + Appearance.sizes.topBarHeight }

    component Step: Row {
        id: step
        property int number
        property string title
        property string detail

        spacing: 12
        width: parent ? parent.width : 560

        Rectangle {
            width: 26
            height: 26
            radius: 13
            color: Appearance.colors.primary

            StyledText {
                anchors.centerIn: parent
                text: step.number
                font.weight: Font.Bold
                font.pixelSize: Appearance.fontSize.smallie
                color: Appearance.colors.onPrimary
            }
        }

        Column {
            width: step.width - 38
            spacing: 2

            StyledText {
                width: parent.width
                text: step.title
                wrapMode: Text.Wrap
                font.pixelSize: Appearance.fontSize.normal
                font.weight: Font.DemiBold
            }
            StyledText {
                width: parent.width
                text: step.detail
                wrapMode: Text.Wrap
                color: Appearance.colors.onSurfaceVariant
                font.pixelSize: Appearance.fontSize.smallie
            }
        }
    }

    Rectangle {
        objectName: "setupInstructionPanel"
        x: column.x - 24
        y: column.y - 24
        width: column.width + 48
        height: column.implicitHeight + 48
        radius: 28
        color: Appearance.colors.surfaceContainer
        border.width: 1
        border.color: Qt.alpha(Appearance.colors.outlineVariant, 0.4)
    }

    Column {
        id: column
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(36, (root.height - implicitHeight) / 2)
        width: Math.min(560, root.width - 2 * Appearance.sizes.pageMargin)
        spacing: 20

        MaterialSymbol {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 64
            height: 64
            text: "library_music"
            iconSize: 56
            fill: 1
            color: Appearance.colors.primary
        }

        TitleText {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Connect Your Music")
            font.pixelSize: Appearance.fontSize.display
            font.weight: Font.Bold
        }

        StyledText {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Swiftify needs a Client ID from your own Spotify developer app. You only have to set this up once.")
            color: Appearance.colors.onSurfaceVariant
        }

        Column {
            width: parent.width
            spacing: 14

            Step {
                number: 1
                title: qsTr("Create a Spotify app")
                detail: qsTr("Open the Spotify Developer Dashboard, choose Create app, and select Web API if Spotify asks.")
            }

            TextButton {
                x: 38
                variant: TextButton.Text
                symbol: "open_in_new"
                text: qsTr("Open Spotify Developer Dashboard")
                onClicked: App.openUrl("https://developer.spotify.com/dashboard")
            }

            Step {
                number: 2
                title: qsTr("Add Swiftify's redirect URI")
                detail: qsTr("In the app settings, add this exact address under Redirect URIs, then save:")
            }

            Rectangle {
                x: 38
                width: redirect.implicitWidth + 24
                height: redirect.implicitHeight + 16
                radius: Appearance.rounding.verySmall
                color: Appearance.colors.surfaceContainerHigh

                TextEdit {
                    id: redirect
                    anchors.centerIn: parent
                    text: Session.redirectUri
                    readOnly: true
                    selectByMouse: true
                    font.family: Appearance.monoFontFamily
                    font.pixelSize: Appearance.fontSize.smallie
                    color: Appearance.colors.onSurface
                    selectionColor: Appearance.colors.primary
                    selectedTextColor: Appearance.colors.onPrimary
                }
            }

            Step {
                number: 3
                title: qsTr("Paste your Client ID")
                detail: qsTr("Copy the Client ID from the app settings. You do not need the Client Secret.")
            }
        }

        OutlinedTextField {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(460, parent.width)
            placeholder: qsTr("Spotify Client ID")
            text: Session.libraryClientId
            onTextEdited: Session.libraryClientId = text
            onAccepted: Session.beginLibraryLogin()
        }

        // the screen stays until both are connected (UiState.needsSetup):
        // each button says when its half is done, and the next one leads
        Flow {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width, libraryButton.implicitWidth + playbackButton.implicitWidth + spacing)
            spacing: 12

            TextButton {
                id: libraryButton
                variant: Session.libraryConnected ? TextButton.Tonal : TextButton.Filled
                symbol: Session.libraryConnected ? "check" : ""
                text: Session.libraryConnected ? qsTr("Library Connected") : qsTr("Connect Library")
                enabled: !Session.libraryConnected && Session.libraryClientId.trim().length > 0
                onClicked: Session.beginLibraryLogin()
            }
            TextButton {
                id: playbackButton
                variant: Session.libraryConnected && !Session.connected ? TextButton.Filled : TextButton.Tonal
                symbol: Session.connected ? "check" : ""
                text: Session.connected ? qsTr("Playback Connected") : qsTr("Connect Playback")
                enabled: !Session.connected
                onClicked: Session.beginPlaybackLogin()
            }
        }

        StyledText {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: Session.libraryConnected && !Session.connected
                  ? qsTr("Your library is connected. Now connect playback so Swiftify can play music.")
                  : Session.connected && !Session.libraryConnected
                    ? qsTr("Playback is connected. Now connect your library to see your music.")
                    : qsTr("Connect Library first, then Connect Playback so Swiftify can play music.")
            font.pixelSize: Appearance.fontSize.smaller
            color: Session.libraryConnected !== Session.connected ? Appearance.colors.primary
                                                                  : Appearance.colors.onSurfaceVariant
        }
    }
}
