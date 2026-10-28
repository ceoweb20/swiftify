pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// A page's vertical wheel scrolling. A mouse wheel moves in coarse notches
// (always on Windows), so each notch glides to its place and the notches that
// follow extend the same glide; a touchpad's pixel deltas are already smooth
// and move the page directly. It also keeps a wheel gesture on one scroll path
// when it crosses horizontal shelves: mixing their explicit offsets with
// Flickable's native touchpad timeline could restore an older offset.
WheelHandler {
    id: handler

    required property Flickable page
    /// how far one wheel notch moves the page
    property real notch: 96
    /// where the glide in progress ends
    property real destination: 0

    target: null
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

    function bounded(contentY: real): real {
        const start = page.originY - page.topMargin
        const end = Math.max(start, page.originY + page.contentHeight - page.height + page.bottomMargin)
        return Math.max(start, Math.min(end, contentY))
    }

    /// moves the page by `delta`, gliding unless the movement is already smooth
    function scrollBy(delta: real, smooth: bool) {
        page.cancelFlick()
        const from = glide.running ? destination : page.contentY
        destination = bounded(from + delta)
        glide.stop()
        if (!smooth || Appearance.performanceMode) {
            page.contentY = destination
            return
        }
        glide.to = destination
        glide.start()
    }

    /// a wheel event's vertical movement: pixels for a touchpad, a glide for a
    /// mouse notch (or a high-resolution wheel's fraction of one)
    function scrollWithWheel(event: WheelEvent): bool {
        const pixels = event.pixelDelta.x !== 0 || event.pixelDelta.y !== 0
        const horizontal = pixels ? Math.abs(event.pixelDelta.x) > Math.abs(event.pixelDelta.y)
                                  : Math.abs(event.angleDelta.x) > Math.abs(event.angleDelta.y)
        if (horizontal || (event.modifiers & Qt.ShiftModifier) !== 0 || !page.interactive) {
            return false
        }
        if (pixels) {
            scrollBy(-event.pixelDelta.y, false)
        } else {
            // small fractions of a notch come from touchpads and fine wheels,
            // which send many: their glides are shorter so they keep up
            glide.duration = Appearance.motion(Math.abs(event.angleDelta.y) >= 120 ? 240 : 120)
            scrollBy(-event.angleDelta.y / 120 * notch, true)
        }
        return true
    }

    onWheel: (event) => {
        if ((event.modifiers & Qt.ControlModifier) !== 0) {
            event.accepted = false
            return
        }
        event.accepted = true
        scrollWithWheel(event)
    }

    // a pointer handler has no default property, so these are held as properties
    readonly property NumberAnimation glide: NumberAnimation {
        target: handler.page
        property: "contentY"
        duration: Appearance.motion(240)
        easing.type: Easing.OutCubic
    }

    readonly property Connections movement: Connections {
        target: handler.page
        // a drag or a fling takes over from the glide
        function onMovementStarted() { handler.glide.stop() }
    }
}
