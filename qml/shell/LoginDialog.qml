/*
 * Swiftify, a spotify player for desktop
 * Copyright (C) 2026  shred0s
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// while the spotify login is open in the browser. the browser lands on
// swiftify's local redirect address, and this closes by itself.
StyledDialog {
    id: root

    // the callback ending the wait is not a cancel; only Esc or Cancel is.
    Connections {
        target: Session
        function onLoginChanged() {
            if (Session.loginInProgress && !root.opened) {
                root.open()
            } else if (!Session.loginInProgress && root.opened) {
                root.accept()
            }
        }
    }

    headline: qsTr("Waiting for your browser…")
symbol: "key"
iconShape: "clover4"
    body: Session.loginIsForLibrary
        ? qsTr("Sign in to Spotify in the browser window that just opened to connect your library.")
        : qsTr("Sign in to Spotify in the browser window that just opened to connect playback.")
    onRejected: Session.cancelLogin()

    buttons: [
        TextButton {
            variant: TextButton.Text
            text: qsTr("Cancel")
            onClicked: Session.cancelLogin()
        },
        TextButton {
            variant: TextButton.Text
            text: qsTr("Copy Link")
            onClicked: {
                App.copyToClipboard(Session.authorizationUrl.toString())
                UiState.toastRequested(qsTr("Login link copied"))
            }
        },
        TextButton {
            variant: TextButton.Tonal
            text: qsTr("Open Again")
            onClicked: Session.reopenLoginPage()
        }
    ]
}
