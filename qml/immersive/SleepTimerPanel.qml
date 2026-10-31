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

// the sleep timer (SleepTimerPanel): hours and minutes to pick, then a
// countdown in their place while it runs, how the music fades, and the button
// that starts or cancels it. seconds are offered only with debugging on.
Item {
    id: root

    property MaterialScheme scheme: PlayerStyle.darkScheme
    readonly property color secondaryText: Qt.rgba(1, 1, 1, 0.6)
    readonly property list<string> fades: ["none", "finalSeconds", "entire"]
    readonly property list<string> explanations: [
        qsTr("track just stops when the timer runs out"),
        qsTr("track fades out at the last 10 seconds"),
        qsTr("track steadily fades out along with the timers whole duration, volume cant be adjusted during this time...")
    ]
    readonly property int fadeIndex: Math.max(0, fades.indexOf(Settings.sleepTimerFade))

    property int hours: 0
    property int minutes: 30
    property int seconds: 0
    readonly property int duration: hours * 3600 + minutes * 60 + (Settings.debuggingEnabled ? seconds : 0)

    // the countdown, refreshed each second it is on screen
    property int remaining: 0

    function refresh() {
        const deadline = Player.sleepTimerDeadline
        remaining = Player.sleepTimerRunning ? Math.max(0, Math.ceil((deadline.getTime() - Date.now()) / 1000)) : 0
    }

    function positional(total: int): string {
        const h = Math.floor(total / 3600)
        const m = Math.floor(total % 3600 / 60)
        const s = total % 60
        return h + ":" + String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0")
    }

    implicitHeight: column.implicitHeight + 44

    Timer {
        interval: 1000
        repeat: true
        running: Player.sleepTimerRunning && root.visible
        triggeredOnStart: true
        onTriggered: root.refresh()
    }
    Connections {
        target: Player
        function onSleepTimerChanged() { root.refresh() }
    }

    component Stepper: Rectangle {
        id: stepper

        property int value
        property int maximum
        property string unit
        signal stepped(int value)

        implicitWidth: row.implicitWidth + 8
        implicitHeight: 44
        radius: height / 2
        color: Qt.rgba(1, 1, 1, 0.08)

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 2

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                scheme: root.scheme
                symbol: "remove"
                size: 36
                iconSize: 20
                iconColor: "white"
                colBackgroundHover: Qt.rgba(1, 1, 1, 0.1)
                enabled: stepper.value > 0
                autoRepeat: true
                onClicked: stepper.stepped(stepper.value - 1)
            }
            StyledText {
                anchors.verticalCenter: parent.verticalCenter
                width: 64
                horizontalAlignment: Text.AlignHCenter
                text: stepper.value + " " + stepper.unit
                tabularNumbers: true
                font.weight: Font.Medium
                color: "white"
            }
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                scheme: root.scheme
                symbol: "add"
                size: 36
                iconSize: 20
                iconColor: "white"
                colBackgroundHover: Qt.rgba(1, 1, 1, 0.1)
                enabled: stepper.value < stepper.maximum
                autoRepeat: true
                onClicked: stepper.stepped(stepper.value + 1)
            }
        }

        WheelHandler {
            onWheel: (event) => {
                const step = event.angleDelta.y > 0 ? 1 : (event.angleDelta.y < 0 ? -1 : 0)
                const next = Math.min(stepper.maximum, Math.max(0, stepper.value + step))
                if (next !== stepper.value) {
                    stepper.stepped(next)
                }
            }
        }
    }

    Column {
        id: column
        y: 44
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 380)
        spacing: 18

        // the picker and the countdown share a place and cross-fade; the
        // place grows with a spring to hold the countdown's ring
        Item {
            width: parent.width
            height: Player.sleepTimerRunning ? countdown.implicitHeight : pickers.implicitHeight

            Behavior on height {
                ElementMove {}
            }

            Row {
                id: pickers
                anchors.centerIn: parent
                spacing: 12
                opacity: Player.sleepTimerRunning ? 0 : 1
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: Appearance.duration(240); easing.type: Easing.InOutQuad }
                }

                Stepper {
                    value: root.hours
                    maximum: 12
                    unit: qsTr("h")
                    onStepped: (value) => root.hours = value
                }
                Stepper {
                    value: root.minutes
                    maximum: 59
                    unit: qsTr("min")
                    onStepped: (value) => root.minutes = value
                }
                Stepper {
                    visible: Settings.debuggingEnabled
                    value: root.seconds
                    maximum: 59
                    unit: qsTr("s")
                    onStepped: (value) => root.seconds = value
                }
            }

            // what is left, in figures and as a wavy ring that drains
            Item {
                id: countdown
                anchors.centerIn: parent
                implicitWidth: 236
                implicitHeight: 236
                width: implicitWidth
                height: implicitHeight
                opacity: Player.sleepTimerRunning ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: Appearance.duration(240); easing.type: Easing.InOutQuad }
                }

                SquiggleRing {
                    anchors.fill: parent
                    color: root.scheme.primary
                    trackColor: Qt.rgba(1, 1, 1, 0.14)
                    thickness: 5
                    amplitude: 3
                    wavelength: 24
                    progress: Player.sleepTimerDuration > 0 ? Math.min(1, root.remaining / Player.sleepTimerDuration) : 0

                    // the seconds tick; the ring glides between them
                    Behavior on progress {
                        NumberAnimation { duration: 1000 }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    RollingText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.positional(root.remaining)
                        pixelSize: 40
                        weight: Font.Bold
                        rounded: true
                        color: "white"
                    }
                    StyledText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("until playback stops")
                        color: root.secondaryText
                    }
                }
            }
        }

        SegmentedControl {
            id: fadePicker
            objectName: "sleepTimerFadePicker"
            width: parent.width
            segmentWidth: (width - 2 * inset) / 3
            scheme: root.scheme
            trackColor: Qt.rgba(1, 1, 1, 0.08)
            indicatorColor: Qt.rgba(1, 1, 1, 0.92)
            contentColor: Qt.rgba(1, 1, 1, 0.8)
            selectedContentColor: "#141414"
            currentIndex: root.fadeIndex
            enabled: !Player.sleepTimerRunning
            opacity: enabled ? 1 : 0.38
            segments: [
                Segment { text: qsTr("No fade") },
                Segment { text: qsTr("Last 10 s") },
                Segment { text: qsTr("Whole Timer") }
            ]
            onActivated: (index) => Settings.sleepTimerFade = root.fades[index]
        }

        Column {
            width: parent.width
            spacing: 8

            TextButton {
                anchors.horizontalCenter: parent.horizontalCenter
                scheme: root.scheme
                symbol: Player.sleepTimerRunning ? "close" : "bedtime"
                text: Player.sleepTimerRunning ? qsTr("Cancel Timer") : qsTr("Start Timer")
                colBackground: Qt.rgba(1, 1, 1, 0.14)
                colBackgroundHover: Qt.rgba(1, 1, 1, 0.2)
                colRipple: Qt.rgba(1, 1, 1, 0.24)
                labelColor: Player.sleepTimerRunning ? root.scheme.error : "white"
                enabled: Player.sleepTimerRunning || root.duration > 0
                onClicked: {
                    if (Player.sleepTimerRunning) {
                        Player.cancelSleepTimer()
                    } else {
                        Player.startSleepTimer(root.duration, Settings.sleepTimerFade)
                    }
                }
            }

            StyledText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: root.explanations[root.fadeIndex]
                wrapMode: Text.Wrap
                font.pixelSize: Appearance.fontSize.smaller
                color: root.secondaryText
                animateChange: true
                changeDistanceY: 0
            }
        }
    }
}
