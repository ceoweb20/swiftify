pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Swiftify

// the immersive player's Sound panel (AudioShapingPanel): the equalizer, with
// the live spectrum behind its curve, and tempo and pitch.
Item {
    id: root

    property MaterialScheme scheme: PlayerStyle.darkScheme
    readonly property color accent: scheme.primary
    readonly property color secondaryText: Qt.rgba(1, 1, 1, 0.6)
    readonly property bool tempoModified: Math.abs(Sound.tempo - 1) > 0.001
    readonly property bool pitchModified: Math.abs(Sound.pitch) > 0.001

    implicitHeight: column.implicitHeight

    component Card: Rectangle {
        default property alias content: cardColumn.data

        width: column.width
        implicitHeight: cardColumn.implicitHeight + 30
        radius: 20
        color: Qt.rgba(1, 1, 1, 0.07)

        Column {
            id: cardColumn
            x: 16
            y: 15
            width: parent.width - 32
            spacing: 14
        }
    }

    component ValuePill: Rectangle {
        id: valuePill

        property string text
        property bool modified

        implicitWidth: Math.max(42, label.implicitWidth) + 16
        implicitHeight: label.implicitHeight + 8
        radius: height / 2
        color: Qt.rgba(1, 1, 1, modified ? 0.16 : 0.08)

        StyledText {
            id: label
            anchors.centerIn: parent
            text: valuePill.text
            tabularNumbers: true
            font.pixelSize: Appearance.fontSize.smaller
            font.weight: Font.DemiBold
            color: "white"
        }
    }

    component ControlRow: Item {
        id: control

        property string symbol
        property string valueText
        property bool modified
        property alias from: slider.from
        property alias to: slider.to
        property alias stepSize: slider.stepSize
        property alias value: slider.value
        signal moved(real value)

        width: parent ? parent.width : 0
        implicitHeight: Math.max(slider.implicitHeight, pill.implicitHeight)

        Behavior on opacity {
            NumberAnimation { duration: Appearance.duration(220); easing.type: Easing.InOutQuad }
        }

        MaterialSymbol {
            id: glyph
            anchors.verticalCenter: parent.verticalCenter
            width: 22
            text: control.symbol
            iconSize: 18
            weight: 600
            color: control.modified ? root.accent : root.secondaryText
        }
        StyledSlider {
            id: slider
            anchors.left: glyph.right
            anchors.leftMargin: 12
            anchors.right: pill.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            scheme: root.scheme
            trackThickness: 10
            snapMode: T.Slider.SnapAlways
            onMoved: control.moved(value)
        }
        ValuePill {
            id: pill
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: control.valueText
            modified: control.modified
        }
    }

    Column {
        id: column
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 460)
        spacing: 14

        Card {
            Item {
                width: parent.width
                height: 40
                StyledText {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - visualizerSwitch.width - 12
                    text: qsTr("Full-screen player visualizer")
                    wrapMode: Text.Wrap
                    font.pixelSize: Appearance.fontSize.small
                    font.weight: Font.DemiBold
                    color: "white"
                }
                StyledSwitch {
                    id: visualizerSwitch
                    objectName: "immersiveVisualizerQuickSwitch"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    scheme: root.scheme
                    enabled: Settings.visualizersEnabled
                    checked: Settings.immersiveVisualizer
                    onToggled: Settings.immersiveVisualizer = checked
                }
            }
            StyledText {
                width: parent.width
                visible: !Settings.visualizersEnabled
                text: qsTr("Audio visualizers are disabled in Settings.")
                wrapMode: Text.Wrap
                font.pixelSize: Appearance.fontSize.smaller
                color: root.secondaryText
            }
        }

        Card {
            Item {
                width: parent.width
                height: 32

                StyledText {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Crossfade")
                    font.pixelSize: Appearance.fontSize.small
                    font.weight: Font.DemiBold
                    color: "white"
                }
                StyledSwitch {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    scheme: root.scheme
                    checked: Settings.crossfadeEnabled
                    onToggled: Settings.crossfadeEnabled = checked
                }
            }

            Row {
                width: parent.width
                height: 26
                spacing: 8

                StyledText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Duration")
                    font.pixelSize: Appearance.fontSize.smaller
                    color: root.secondaryText
                }
                Item { width: Math.max(0, parent.width - durationSlider.width - durationValue.width - parent.spacing * 2); height: 1 }
                StyledText {
                    id: durationValue
                    anchors.verticalCenter: parent.verticalCenter
                    text: Settings.crossfadeDurationSeconds + qsTr(" s")
                    tabularNumbers: true
                    font.pixelSize: Appearance.fontSize.smaller
                    font.weight: Font.DemiBold
                    color: "white"
                }
            }
            StyledSlider {
                id: durationSlider
                width: parent.width
                scheme: root.scheme
                from: 1
                to: 12
                stepSize: 1
                value: Settings.crossfadeDurationSeconds
                enabled: Settings.crossfadeEnabled
                opacity: enabled ? 1 : 0.4
                trackThickness: 10
                snapMode: T.Slider.SnapAlways
                onMoved: Settings.crossfadeDurationSeconds = Math.round(value)
            }
        }

        Card {
            Item {
                width: parent.width
                height: 32

                StyledText {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Normalize volume")
                    font.pixelSize: Appearance.fontSize.small
                    font.weight: Font.DemiBold
                    color: "white"
                }
                StyledSwitch {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    scheme: root.scheme
                    checked: Settings.normalizeVolume
                    onToggled: Settings.normalizeVolume = checked
                }
            }
        }

        Card {
            Item {
                width: parent.width
                height: 32

                StyledText {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Equalizer")
                    font.pixelSize: Appearance.fontSize.small
                    font.weight: Font.DemiBold
                    color: "white"
                }

                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    RippleButton {
                        id: presetButton
                        anchors.verticalCenter: parent.verticalCenter
                        scheme: root.scheme
                        enabled: Sound.equalizerEnabled
                        implicitHeight: 26
                        leftPadding: 10
                        rightPadding: 8
                        topPadding: 0
                        bottomPadding: 0
                        buttonRadius: height / 2
                        colBackground: Qt.rgba(1, 1, 1, 0.12)
                        colBackgroundHover: Qt.rgba(1, 1, 1, 0.18)
                        onClicked: presetMenu.popup(presetButton, 0, presetButton.height + 4)

                        contentItem: Row {
                            spacing: 4

                            StyledText {
                                anchors.verticalCenter: parent.verticalCenter
                                text: Sound.activePresetName.length === 0 ? qsTr("Custom")
                                    : Sound.isSavedPreset(Sound.activePresetName) ? Sound.activePresetName
                                    : qsTranslate("Presets", Sound.activePresetName)
                                font.pixelSize: Appearance.fontSize.smaller
                                font.weight: Font.DemiBold
                                color: "white"
                            }
                            MaterialSymbol {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "unfold_more"
                                iconSize: 14
                                weight: 700
                                color: "white"
                            }
                        }
                    }

                    StyledSwitch {
                        anchors.verticalCenter: parent.verticalCenter
                        scheme: root.scheme
                        checked: Sound.equalizerEnabled
                        onToggled: Sound.equalizerEnabled = checked
                    }
                }
            }

            Row {
                width: parent.width
                height: 26
                spacing: 8

                StyledText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Gain")
                    font.pixelSize: Appearance.fontSize.smaller
                    color: root.secondaryText
                }
                Item { width: Math.max(0, parent.width - gainSlider.width - gainValue.width - parent.spacing * 2); height: 1 }
                StyledText {
                    id: gainValue
                    anchors.verticalCenter: parent.verticalCenter
                    text: (Sound.preampGain > 0 ? "+" : "") + Sound.preampGain.toFixed(1) + qsTr(" dB")
                    tabularNumbers: true
                    font.pixelSize: Appearance.fontSize.smaller
                    font.weight: Font.DemiBold
                    color: Math.abs(Sound.preampGain) > 0.01 ? root.accent : "white"
                }
            }
            StyledSlider {
                id: gainSlider
                objectName: "immersivePreampGainSlider"
                width: parent.width
                scheme: root.scheme
                from: -12
                to: 12
                stepSize: 0.5
                value: Sound.preampGain
                trackThickness: 10
                snapMode: T.Slider.SnapAlways
                onMoved: Sound.preampGain = value
            }

            Item {
                width: parent.width
                height: 96

                SpectrumView {
                    objectName: "immersiveEqualizerSpectrum"
                    anchors.fill: parent
                    kind: SpectrumView.Equalizer
                    sensitivity: 0.85
                    visible: Settings.visualizersEnabled && Settings.immersiveVisualizer
                    active: visible && Player.playing
                    opacity: Sound.equalizerEnabled ? 0.58 : 0.22

                    Behavior on opacity {
                        NumberAnimation { duration: Appearance.duration(220) }
                    }
                }
                EqualizerCurve {
                    anchors.fill: parent
                    gains: Sound.gains
                    maximumGain: 12
                    active: Sound.equalizerEnabled
                    tint: root.accent
                }
            }

            Row {
                width: parent.width
                enabled: Sound.equalizerEnabled
                opacity: enabled ? 1 : 0.4

                Behavior on opacity {
                    NumberAnimation { duration: Appearance.duration(220); easing.type: Easing.InOutQuad }
                }

                Repeater {
                    model: Sound.bandLabels.length

                    delegate: Column {
                        id: band

                        required property int index
                        readonly property real gain: Sound.gains[index] ?? 0

                        width: parent.width / Sound.bandLabels.length
                        spacing: 10

                        VerticalFader {
                            anchors.horizontalCenter: parent.horizontalCenter
                            value: band.gain
                            from: -12
                            to: 12
                            stepSize: 0.5
                            glowing: Sound.equalizerEnabled
                            accent: root.accent
                            onMoved: (value) => Sound.setGain(band.index, value)
                        }
                        StyledText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: Sound.bandLabels[band.index]
                            tabularNumbers: true
                            font.pixelSize: Appearance.fontSize.smallest
                            font.weight: Font.DemiBold
                            color: Math.abs(band.gain) > 0.05 ? root.accent : root.secondaryText
                        }
                    }
                }
            }
        }

        Card {
            Item {
                width: parent.width
                height: 24

                StyledText {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Tempo & Pitch")
                    font.pixelSize: Appearance.fontSize.small
                    font.weight: Font.DemiBold
                    color: "white"
                }
                TextButton {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    variant: TextButton.Text
                    scheme: root.scheme
                    implicitHeight: 28
                    text: qsTr("Reset")
                    labelColor: root.secondaryText
                    opacity: root.tempoModified || root.pitchModified ? 1 : 0
                    enabled: opacity > 0.5
                    onClicked: {
                        Sound.tempo = 1
                        Sound.pitch = 0
                    }

                    Behavior on opacity {
                        NumberAnimation { duration: Appearance.duration(200); easing.type: Easing.InOutQuad }
                    }
                }
            }

            ControlRow {
                symbol: "speed"
                valueText: (Math.round(Sound.tempo * 100) / 100) + "×"
                modified: root.tempoModified
                from: 0.5
                to: 2
                stepSize: 0.05
                value: Sound.tempo
                onMoved: (value) => Sound.tempo = value
            }

            Item {
                width: parent.width
                height: 32

                StyledText {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Keep Original Pitch")
                    color: "white"
                }
                StyledSwitch {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    scheme: root.scheme
                    checked: Sound.preservesPitch
                    onToggled: Sound.preservesPitch = checked
                }
            }

            StyledText {
                width: parent.width
                text: Sound.preservesPitch
                    ? qsTr("speed changes the tempo only, not the key")
                    : qsTr("speed also affects the key, js like sped up or slowed down songs are")
                wrapMode: Text.Wrap
                font.pixelSize: Appearance.fontSize.smaller
                color: root.secondaryText
                animateChange: true
                changeDistanceY: 0
            }

            ControlRow {
                symbol: "piano"
                valueText: {
                    const semitones = Math.round(Sound.pitch)
                    return semitones > 0 ? "+" + semitones : String(semitones)
                }
                modified: root.pitchModified
                from: -12
                to: 12
                stepSize: 1
                value: Sound.pitch
                enabled: Sound.preservesPitch
                opacity: enabled ? 1 : 0.35
                onMoved: (value) => Sound.pitch = value
            }
        }
    }

    // what can be done with the sound as it is first, then the presets you
    // saved in a menu of their own beside it, then the built-in ones
    StyledMenu {
        id: presetMenu

        readonly property var savedPresets: Sound.presetNames.filter(name => Sound.isSavedPreset(name))
        readonly property var builtInPresets: Sound.presetNames.filter(name => !Sound.isSavedPreset(name))

        /// where an entry at `offset` among those after `marker` goes. by
        /// place in the whole menu, a list made again after the menu was
        /// built landed in among the entries above it
        function indexAfter(marker: Item, offset: int): int {
            for (let index = 0; index < count; ++index) {
                if (itemAt(index) === marker) {
                    return index + 1 + offset
                }
            }
            return count
        }

        StyledMenuItem {
            text: qsTr("Custom")
            checkmark: true
            checked: true
            enabled: false
            visible: Sound.activePresetName.length === 0
            height: visible ? implicitHeight : 0
        }
        StyledMenuItem {
            text: qsTr("Save Current…")
            symbol: "save"
            enabled: !Sound.flat
            onTriggered: saveDialog.ask()
        }
        StyledMenuItem {
            readonly property string preset: Sound.activePresetName
            text: qsTr("Delete “%1”").arg(preset)
            symbol: "delete"
            destructive: true
            visible: preset.length > 0 && Sound.isSavedPreset(preset)
            height: visible ? implicitHeight : 0
            onTriggered: Sound.deletePreset(preset)
        }
        StyledMenuItem {
            text: qsTr("Reset")
            symbol: "restart_alt"
            enabled: !Sound.flat || Math.abs(Sound.preampGain) > 0.01
            onTriggered: Sound.reset()
        }

        StyledMenu {
            id: savedMenu
            title: qsTr("Custom Presets")
            enabled: presetMenu.savedPresets.length > 0

            // nothing else is in it, so a place in the list is its place here
            Instantiator {
                model: presetMenu.savedPresets
                delegate: StyledMenuItem {
                    required property string modelData
                    text: modelData
                    checkmark: true
                    checked: modelData === Sound.activePresetName
                    onTriggered: Sound.applyPreset(modelData)
                }
                onObjectAdded: (index, object) => savedMenu.insertItem(index, object as StyledMenuItem)
                onObjectRemoved: (index, object) => savedMenu.removeItem(object as StyledMenuItem)
            }
        }

        StyledMenuSeparator {
            id: builtInDivider
        }

        Instantiator {
            model: presetMenu.builtInPresets
            delegate: StyledMenuItem {
                required property string modelData
                text: qsTranslate("Presets", modelData)
                checkmark: true
                checked: modelData === Sound.activePresetName
                onTriggered: Sound.applyPreset(modelData)
            }
            onObjectAdded: (index, object) => presetMenu.insertItem(presetMenu.indexAfter(builtInDivider, index),
                                                                    object as StyledMenuItem)
            onObjectRemoved: (index, object) => presetMenu.removeItem(object as StyledMenuItem)
        }
    }

    StyledDialog {
        id: saveDialog

        function ask() {
            name.text = Sound.activePresetName
            open()
            name.inputItem.forceActiveFocus()
        }
        function save() {
            const trimmed = name.text.trim()
            if (trimmed.length > 0) {
                Sound.savePreset(trimmed)
                accept()
            }
        }

        headline: qsTr("Save Preset")
        symbol: "tune"
        iconShape: "flower"

        OutlinedTextField {
            id: name
            width: parent.width
            placeholder: qsTr("Name")
            onAccepted: saveDialog.save()
        }

        buttons: [
            TextButton {
                text: qsTr("Save")
                enabled: name.text.trim().length > 0
                onClicked: saveDialog.save()
            },
            TextButton {
                variant: TextButton.Text
                text: qsTr("Cancel")
                onClicked: saveDialog.close()
            }
        ]
    }
}
