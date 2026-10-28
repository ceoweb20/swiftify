pragma ComponentBehavior: Bound
import QtQuick

// Keep a wheel gesture on one scroll path when it crosses horizontal shelves.
// Mixing their explicit offsets with Flickable's native touchpad timeline can
// restore an older offset when the next update lands between shelves.
WheelHandler {
    required property Flickable page

    target: null
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    onWheel: (event) => {
        if ((event.modifiers & Qt.ControlModifier) !== 0) {
            event.accepted = false
            return
        }
        event.accepted = true
        const pixels = event.pixelDelta.x !== 0 || event.pixelDelta.y !== 0
        const horizontal = pixels ? Math.abs(event.pixelDelta.x) > Math.abs(event.pixelDelta.y)
                                  : Math.abs(event.angleDelta.x) > Math.abs(event.angleDelta.y)
        if (horizontal || (event.modifiers & Qt.ShiftModifier) !== 0 || !page.interactive) return
        const delta = pixels ? event.pixelDelta.y : event.angleDelta.y / 120 * 60
        const start = -page.topMargin
        const end = Math.max(start, page.contentHeight - page.height + page.bottomMargin)
        page.cancelFlick()
        page.contentY = Math.max(start, Math.min(end, page.contentY - delta))
    }
}
