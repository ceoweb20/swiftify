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
import QtQuick.Templates as T
import Swiftify

// a material 3 dialog over a scrim: a headline, a line of text, anything else
// the dialog needs, and its buttons right-aligned. Esc dismisses it.
T.Popup {
    id: root

    property string headline
    /// the symbol and shape beside the headline; none, and it stands alone
    property string symbol
    property string iconShape: "cookie9"
    property string body
    property real preferredWidth: 440
    default property alias content: extra.data
    property alias buttons: buttonRow.data
    signal rejected()

    property bool settled: false
    function accept() {
        settled = true
        close()
    }

    parent: T.Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape
    padding: 24
    width: Math.min(preferredWidth, parent ? parent.width - 48 : preferredWidth)
    implicitHeight: column.implicitHeight + topPadding + bottomPadding

    onOpened: settled = false
    onClosed: {
        if (!settled) {
            rejected()
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Appearance.duration(150) }
        NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: Appearance.motion(350); easing.type: Easing.BezierSpline; easing.bezierCurve: Appearance.curves.expressiveFastSpatial }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Appearance.duration(120) }
    }

    T.Overlay.modal: Rectangle {
        color: Appearance.colors.scrimOverlay
    }

    background: Rectangle {
        radius: 28
        color: Appearance.colors.surfaceContainerHigh
    }

    contentItem: Column {
        id: column
        spacing: 16

        Row {
            width: column.width
            visible: root.headline.length > 0
            spacing: 12

            ShapeIcon {
                id: headlineIcon
                anchors.verticalCenter: parent.verticalCenter
                visible: root.symbol.length > 0
                size: 40
                iconSize: 21
                shape: root.iconShape
                symbol: root.symbol
            }
            TitleText {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - (headlineIcon.visible ? headlineIcon.width + parent.spacing : 0)
                text: root.headline
                wrapMode: Text.Wrap
                font.pixelSize: Appearance.fontSize.huge
            }
        }

        StyledText {
            width: column.width
            visible: root.body.length > 0
            text: root.body
            wrapMode: Text.Wrap
            color: Appearance.colors.onSurfaceVariant
        }

        Column {
            id: extra
            width: column.width
            spacing: 12
        }

        Row {
            id: buttonRow
            anchors.right: parent.right
            topPadding: 8
            spacing: 8
            layoutDirection: Qt.RightToLeft
        }
    }
}
