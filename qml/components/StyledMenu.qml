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

// a material menu, as end4 draws its popup menus: a surface-container card
// with rounded items.
T.Menu {
    id: root

    /// the immersive player's menus are dark and follow the cover
    property MaterialScheme scheme: UiState.immersiveOpen ? PlayerStyle.darkScheme : Appearance.colors

    // drawn in the window it opens over. a menu in a window of its own got a
    // render thread and a set of window-sized buffers on its first opening,
    // and kept them for the rest of the session
    popupType: T.Popup.Item
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    padding: 8
    margins: 8
    overlap: 4

    delegate: StyledMenuItem {}

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Appearance.duration(150); easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Appearance.duration(100) }
    }

    contentItem: ListView {
        // as wide as its widest entry, so none is cut short, up to a width
        // past which a name is better ellipsized than the menu spread
        implicitWidth: {
            let widest = 0
            for (let index = 0; index < root.count; ++index) {
                const entry = root.itemAt(index)
                if (entry && entry.visible) {
                    widest = Math.max(widest, entry.implicitWidth)
                }
            }
            return Math.min(widest, 440)
        }
        implicitHeight: contentHeight
        model: root.contentModel
        interactive: Window.window ? contentHeight + root.topPadding + root.bottomPadding > Window.window.height / Settings.zoom : false
        clip: true
        currentIndex: root.currentIndex
        spacing: 2
    }

    background: Rectangle {
        implicitWidth: 220
        radius: Appearance.rounding.normal
        color: root.scheme.surfaceContainer
        border.width: 1
        border.color: root.scheme.layer0Border
    }
}
