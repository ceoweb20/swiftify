pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import Swiftify

StyledDialog {
    id: root
    objectName: "credentialVaultDialog"
    preferredWidth: 520
    closePolicy: T.Popup.NoAutoClose
    headline: CredentialVault.exists ? qsTr("Unlock saved logins") : qsTr("Protect your Spotify logins")
    symbol: "lock"
    readonly property string intro: CredentialVault.exists
          ? CredentialVault.automaticUnlock
            ? qsTr("Enter your existing vault password once. Swiftify will remember it using this computer's secure storage and unlock automatically on later launches.")
            : qsTr("Enter your vault password. It stays in memory until Swiftify quits; no Keychain access is used.")
          : qsTr("Choose a separate password, not your Spotify password (at least 12 characters). Saved logins are encrypted on disk. You will enter it once at launch.")

    function submit() {
        if (CredentialVault.busy || password.text.length === 0) return
        if (!CredentialVault.exists && (password.text.length < 12 || password.text !== confirmation.text)) return
        CredentialVault.unlock(password.text)
        password.clear()
        confirmation.clear()
    }

    Component.onCompleted: {
        if (CredentialVault.inUse && !CredentialVault.unlocked && !CredentialVault.busy) Qt.callLater(root.open)
    }
    onOpened: password.forceActiveFocus()
    Connections {
        target: CredentialVault
        function onChanged() {
            if (CredentialVault.unlocked) root.accept()
            else if (CredentialVault.inUse && !CredentialVault.busy && !root.visible) root.open()
        }
    }

    Flickable {
        id: formScroll
        width: parent.width
        implicitHeight: Math.min(contentHeight, Math.max(110, (root.parent ? root.parent.height : 780) - 250))
        contentWidth: width
        contentHeight: form.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: StyledScrollBar {}

        Column {
            id: form
            width: parent.width
            spacing: 12
            StyledText {
                width: parent.width
                text: root.intro
                wrapMode: Text.Wrap
                color: Appearance.colors.onSurfaceVariant
            }
            TextField {
                id: password
                objectName: "vaultPassword"
                width: parent.width
                enabled: !CredentialVault.busy
                placeholderText: qsTr("Vault password")
                echoMode: TextInput.Password
                passwordMaskDelay: 0
                maximumLength: 256
                inputMethodHints: Qt.ImhHiddenText | Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                color: Appearance.colors.onSurface
                font.family: Appearance.fontFamily
                font.pixelSize: Appearance.fontSize.small
                padding: 14
                background: Rectangle {
                    radius: Appearance.rounding.verySmall
                    color: Appearance.colors.surface
                    border.color: password.activeFocus ? Appearance.colors.primary : Appearance.colors.outline
                }
                onAccepted: {
                    if (CredentialVault.exists) root.submit()
                    else confirmation.forceActiveFocus()
                }
            }
            TextField {
                id: confirmation
                objectName: "vaultPasswordConfirmation"
                width: parent.width
                visible: !CredentialVault.exists
                enabled: !CredentialVault.busy
                placeholderText: qsTr("Repeat vault password")
                echoMode: TextInput.Password
                passwordMaskDelay: 0
                maximumLength: 256
                inputMethodHints: Qt.ImhHiddenText | Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                color: Appearance.colors.onSurface
                font.family: Appearance.fontFamily
                font.pixelSize: Appearance.fontSize.small
                padding: 14
                background: Rectangle {
                    radius: Appearance.rounding.verySmall
                    color: Appearance.colors.surface
                    border.color: confirmation.activeFocus ? Appearance.colors.primary : Appearance.colors.outline
                }
                onAccepted: root.submit()
            }
            StyledText {
                width: parent.width
                visible: !CredentialVault.exists
                text: qsTr("Old Keychain entries are left untouched. Sign in to Spotify again once. If you forget this password, the saved logins cannot be recovered.")
                wrapMode: Text.Wrap
                font.pixelSize: Appearance.fontSize.smaller
                color: Appearance.colors.onSurfaceVariant
            }
            StyledText {
                width: parent.width
                visible: CredentialVault.busy || CredentialVault.error.length > 0
                text: CredentialVault.busy ? qsTr("Unlocking encrypted logins…") : qsTranslate("Errors", CredentialVault.error)
                wrapMode: Text.Wrap
                color: CredentialVault.busy ? Appearance.colors.onSurfaceVariant : Appearance.colors.error
            }
        }
    }
    buttons: [
        TextButton {
            objectName: "unlockCredentialVault"
            text: CredentialVault.exists ? qsTr("Unlock") : qsTr("Create vault")
            enabled: !CredentialVault.busy && password.text.length > 0
                     && (CredentialVault.exists || (password.text.length >= 12 && password.text === confirmation.text))
            onClicked: root.submit()
        },
        TextButton {
            variant: TextButton.Text
            text: qsTr("Quit")
            onClicked: App.quit()
        }
    ]
}
