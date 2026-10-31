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

// the seek line: a wave while playing, a slim track at rest, thicker with a
// handle while hovered or dragged. a drag commits on release, and only for the
// song that was playing when it began.
Item {
    id: root

    property MaterialScheme scheme: Appearance.colors
    property color activeColor: scheme.primary
    property color trackColor: scheme.secondaryContainer
    property real positionMs: Player.livePositionMs
    property real durationMs: Player.durationMs
    property bool playing: Player.playing
    property bool expanded: hover.hovered || scrub.editing
    property real restThickness: 4
    property real expandedThickness: 6
    property real positionAnchorMs: positionMs
    property real clockAnchor: progressClock.value
    readonly property real runningPositionMs: root.playing && !Appearance.reduceMotion
                                              ? Math.min(root.durationMs, root.positionAnchorMs + (progressClock.value - root.clockAnchor) * 1000)
                                              : root.positionMs
    readonly property real displayPositionMs: scrub.editing && root.scrubPosition >= 0 ? root.scrubPosition : root.runningPositionMs
    readonly property real fraction: durationMs > 0 ? Math.min(1, Math.max(0, displayPositionMs / durationMs)) : 0
    property real scrubPosition: -1
    readonly property bool scrubbing: scrub.editing
    /// over a crossfade the line eases back from where the last song was
    property bool followsCrossfade: false
    property real handoverFrom: 0
    property real handover: 0
    readonly property real shownFraction: fraction + (handoverFrom - fraction) * handover

    implicitHeight: 24
    enabled: Player.hasSong && durationMs > 0

    onPositionMsChanged: {
        positionAnchorMs = positionMs
        clockAnchor = progressClock.value
    }
    onPlayingChanged: {
        positionAnchorMs = positionMs
        clockAnchor = progressClock.value
    }

    AmbientClock {
        id: progressClock
        speed: root.playing && root.visible && root.durationMs > 0 && !Appearance.reduceMotion ? 1 : 0
    }

    ScrubSession {
        id: scrub
    }

    Connections {
        target: Player
        function onCurrentSongChanged() {
            scrub.cancel()
            root.scrubPosition = -1
            if (root.followsCrossfade && Player.crossfadeMs > 0 && !Appearance.reduceMotion) {
                root.handoverFrom = line.progress
                handoverAnimation.duration = Player.crossfadeMs
                handoverAnimation.restart()
            } else {
                handoverAnimation.stop()
                root.handover = 0
            }
        }
    }

    NumberAnimation {
        id: handoverAnimation
        target: root
        property: "handover"
        from: 1
        to: 0
        easing.type: Easing.InOutCubic
    }

    ShaderEffect {
        id: line
        anchors.fill: parent
        property size itemSize: Qt.size(width, height)
        property real progress: root.shownFraction
        // flat with no shapes, and with reduced motion, as the wave would run
        property real amplitude: root.playing && !Appearance.reduceMotion && !Settings.noShapes
                                 ? (root.expanded ? 2.4 : 1.6) : 0
        property real wavelength: 34
        // flows along at a wavelength every 1.6 s, on the shared 30 hz clock
        // that turns with the music, rather than a frame-rate animation of
        // its own that had the window drawn at the display's rate
        property real phase: root.visible ? -(UiState.spin * 4.36) % (2 * Math.PI) : 0
        property real thickness: root.expanded ? root.expandedThickness : root.restThickness
        property real handleWidth: root.expanded ? 4 : 0
        property real handleHeight: 18
        property real gap: 4
        property color activeColor: root.activeColor
        property color trackColor: root.trackColor
        property color handleColor: root.activeColor
        fragmentShader: "qrc:/shaders/wavy_progress.frag.qsb"
        opacity: Player.hasSong ? 1 : 0.45

        Behavior on amplitude {
            NumberAnimation { duration: Appearance.motion(400); easing.type: Easing.OutCubic }
        }
        Behavior on thickness {
            ElementMoveFast {}
        }
        Behavior on handleWidth {
            ElementMoveFast {}
        }
    }

    HoverHandler {
        id: hover
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        preventStealing: true

        function positionAt(x: real): real {
            return Math.max(0, Math.min(1, x / width)) * root.durationMs
        }

        onPressed: (mouse) => {
            scrub.begin(Player.currentSong.instanceId)
            root.scrubPosition = positionAt(mouse.x)
        }
        onPositionChanged: (mouse) => {
            if (scrub.acceptsChanges(Player.currentSong.instanceId)) {
                root.scrubPosition = positionAt(mouse.x)
            }
        }
        onReleased: {
            const target = scrub.finish(Player.currentSong.instanceId, root.scrubPosition)
            if (target >= 0) {
                Player.seek(Math.round(target))
            }
            root.scrubPosition = -1
        }
        onCanceled: {
            scrub.finish(0, -1)
            root.scrubPosition = -1
        }
    }
}
