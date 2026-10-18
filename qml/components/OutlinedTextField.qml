pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// a material outlined text field, with its label as the placeholder.
Rectangle {
    id: root

    property alias text: input.text
    property string placeholder
    property int maximumLines: 1
    /// a description field starts this tall, and grows to maximumLines
    property int minimumLines: 1
    property alias inputItem: input
    property bool tabChangesFocus: true
    signal accepted()
    signal textEdited()

    implicitWidth: 280
    implicitHeight: Math.max(48, Math.min(Math.max(input.lineCount, minimumLines), maximumLines) * metrics.lineSpacing + 26)
    radius: Appearance.rounding.verySmall
    color: "transparent"
    border.width: input.activeFocus ? 2 : 1
    border.color: input.activeFocus ? Appearance.colors.primary : Appearance.colors.outline

    FontMetrics {
        id: metrics
        font: input.font
    }

    TextEdit {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 13
        anchors.bottomMargin: 13
        wrapMode: root.maximumLines > 1 ? TextEdit.Wrap : TextEdit.NoWrap
        clip: true
        selectByMouse: true
        activeFocusOnTab: true
        Keys.onTabPressed: (event) => {
            if (!root.tabChangesFocus) {
                event.accepted = false
                return
            }
            if (event.modifiers & Qt.ShiftModifier) {
                let prev = root.KeyNavigation.backtab ? root.KeyNavigation.backtab : input.nextItemInFocusChain(false)
                if (prev) {
                    if (prev.inputItem) {
                        prev = prev.inputItem
                    }
                    prev.forceActiveFocus(Qt.BacktabFocusReason)
                    event.accepted = true
                }
                return
            }
            let next = root.KeyNavigation.tab ? root.KeyNavigation.tab : input.nextItemInFocusChain(true)
            if (next) {
                if (next.inputItem) {
                    next = next.inputItem
                }
                next.forceActiveFocus(Qt.TabFocusReason)
                event.accepted = true
            }
        }
        Keys.onBacktabPressed: (event) => {
            if (!root.tabChangesFocus) {
                event.accepted = false
                return
            }
            let prev = root.KeyNavigation.backtab ? root.KeyNavigation.backtab : input.nextItemInFocusChain(false)
            if (prev) {
                if (prev.inputItem) {
                    prev = prev.inputItem
                }
                prev.forceActiveFocus(Qt.BacktabFocusReason)
                event.accepted = true
            }
        }
        color: Appearance.colors.onSurface
        selectionColor: Appearance.colors.primary
        selectedTextColor: Appearance.colors.onPrimary
        font.family: Appearance.fontFamily
        font.pixelSize: Appearance.fontSize.small
        onTextChanged: if (activeFocus) root.textEdited()
        Keys.onReturnPressed: (event) => {
            if (root.maximumLines > 1 && (event.modifiers & Qt.ShiftModifier)) {
                event.accepted = false
            } else {
                root.accepted()
            }
        }
        Keys.onEnterPressed: root.accepted()

        StyledText {
            Reveal.shown: input.text.length === 0
            text: root.placeholder
            color: Appearance.colors.onSurfaceVariant
            font.pixelSize: Appearance.fontSize.small
        }
    }

    HoverHandler {
        cursorShape: Qt.IBeamCursor
    }
}
