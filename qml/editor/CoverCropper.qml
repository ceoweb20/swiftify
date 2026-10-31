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

// framing a picked photo as a square (CoverCropper): drag to move it, scroll,
// pinch or slide to zoom, and the photo always covers the square. what it
// shows is the crop that gets encoded, measured by the same arithmetic.
StyledDialog {
    id: root

    required property CoverMaker maker
    /// the framed square as a jpeg, ready to upload
    signal used(var jpeg)

    readonly property real viewport: 380
    readonly property real maximumZoom: 6
    property real zoom: 1
    property real committedZoom: 1
    property size offset: Qt.size(0, 0)
    property size committedOffset: Qt.size(0, 0)
    readonly property rect crop: maker.photoValid ? maker.cropRect(viewport, zoom, offset) : Qt.rect(0, 0, 1, 1)

    function begin() {
        zoom = 1
        committedZoom = 1
        offset = Qt.size(0, 0)
        committedOffset = Qt.size(0, 0)
        open()
    }

    function clamped(proposed: size): size {
        return maker.clampedOffset(proposed, viewport, zoom)
    }

    function zoomTo(value: real) {
        zoom = Math.min(maximumZoom, Math.max(1, value))
        offset = clamped(offset)
    }

    function settle() {
        committedZoom = zoom
        committedOffset = offset
    }

    function use() {
        const jpeg = maker.cropPhoto(viewport, zoom, offset)
        if (jpeg.byteLength > 0) {
            used(jpeg)
        }
        accept()
    }

    headline: qsTr("Adjust Cover")
symbol: "crop"
iconShape: "square"
    preferredWidth: viewport + 48

    Item {
        anchors.horizontalCenter: parent.horizontalCenter
        width: root.viewport
        height: root.viewport

        Image {
            id: photo
            visible: false
            source: root.maker.photo
            autoTransform: true
            sourceSize: Qt.size(2048, 2048)
            asynchronous: true
        }

        ShaderEffect {
            anchors.fill: parent
            Reveal.shown: photo.status === Image.Ready

            property variant source: photo
            property size itemSize: Qt.size(width, height)
            property real radius: 14
            property real borderWidth: 0
            property color borderColor: "transparent"
            property size uvScale: Qt.size(root.crop.width / Math.max(1, root.maker.photoSize.width),
                                           root.crop.height / Math.max(1, root.maker.photoSize.height))
            property point uvOffset: Qt.point(root.crop.x / Math.max(1, root.maker.photoSize.width),
                                              root.crop.y / Math.max(1, root.maker.photoSize.height))

            property real waviness: 0
            property real wavelength: 22
            property real phase: 0
            property real ringWaves: 0
            property real ringAmplitude: 0
            property real ringThickness: 0
            property real ringPhase: 0
            property color ringColor: "transparent"
            property real reveal: 1

            fragmentShader: "qrc:/shaders/rounded_image.frag.qsb"
        }

        LoadingSpinner {
            anchors.centerIn: parent
            Reveal.shown: photo.status === Image.Loading
            width: 32
            height: 32
        }

        DragHandler {
            target: null
            cursorShape: Qt.ClosedHandCursor
            onTranslationChanged: root.offset = root.clamped(Qt.size(root.committedOffset.width + translation.x,
                                                                     root.committedOffset.height + translation.y))
            onActiveChanged: if (!active) root.settle()
        }
        PinchHandler {
            target: null
            onActiveScaleChanged: root.zoomTo(root.committedZoom * activeScale)
            onActiveChanged: if (!active) root.settle()
        }
        WheelHandler {
            onWheel: (event) => {
                root.zoomTo(root.zoom * Math.pow(1.0015, event.angleDelta.y))
                root.settle()
            }
        }
        HoverHandler {
            cursorShape: Qt.OpenHandCursor
        }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        width: root.viewport
        spacing: 8

        MaterialSymbol {
            id: smaller
            anchors.verticalCenter: parent.verticalCenter
            text: "zoom_out"
            iconSize: 20
            color: Appearance.colors.onSurfaceVariant
        }
        StyledSlider {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - smaller.width - larger.width - 2 * parent.spacing
            from: 1
            to: root.maximumZoom
            value: root.zoom
            trackThickness: 10
            onMoved: {
                root.zoomTo(value)
                root.settle()
            }
        }
        MaterialSymbol {
            id: larger
            anchors.verticalCenter: parent.verticalCenter
            text: "zoom_in"
            iconSize: 20
            color: Appearance.colors.onSurfaceVariant
        }
    }

    buttons: [
        TextButton {
            text: qsTr("Use")
            enabled: root.maker.photoValid
            onClicked: root.use()
        },
        TextButton {
            variant: TextButton.Text
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    ]
}
