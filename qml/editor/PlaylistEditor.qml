pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs
import Swiftify

// a new playlist, or the open one's details (PlaylistEditorSheet): a cover
// from the carousel, either a photo framed in the cropper or one of ten
// designs drawn from the playlist's own colours, then the name, the
// description and whether it is public. nothing reaches spotify until Save.
StyledDialog {
    id: root

    property bool creating: true
    property playlist playlist
    /// the design picked in the carousel; -1 while the photo, or nothing, is
    property int selectedDesign: -1
    /// a photo already cropped and encoded, waiting for Save
    property var pickedCover: null
    property bool saving: false
    property string problem

    readonly property real tileEdge: 96
    readonly property bool nameIsEmpty: nameField.text.trim().length === 0

    function openForCreate() {
        creating = true
        reset("", "", false)
    }

    function openForEdit(target: playlist) {
        creating = false
        playlist = target
        reset(target.name, target.description, target.isPublic)
    }

    function reset(name: string, description: string, isPublic: bool) {
        nameField.text = name
        descriptionField.text = description
        publicSwitch.checked = isPublic
        selectedDesign = -1
        pickedCover = null
        saving = false
        problem = ""
        maker.photo = ""
        carousel.positionViewAtBeginning()
        open()
        nameField.inputItem.forceActiveFocus()
    }

    function save() {
        if (nameIsEmpty || saving) {
            return
        }
        let cover = new ArrayBuffer(0)
        if (selectedDesign >= 0) {
            cover = maker.designJpeg(selectedDesign, nameField.text)
        } else if (pickedCover) {
            cover = pickedCover
        }
        saving = true
        Library.savePlaylist(creating, nameField.text.trim(), descriptionField.text, publicSwitch.checked, cover)
    }

    headline: creating ? qsTr("New Playlist") : qsTr("Edit Playlist")
symbol: "queue_music"
iconShape: "bun"
    preferredWidth: 440

    Connections {
        target: Library
        enabled: root.saving
        function onPlaylistSaveFinished(ok: bool) {
            root.saving = false
            if (ok) {
                UiState.burst(saveButton, "playlist")
                root.accept()
            }
        }
    }

    // the designs follow the colours of the artwork the playlist has now
    ArtworkColors {
        id: ambience
        source: root.creating ? "" : root.playlist.artworkUrl
    }

    CoverMaker {
        id: maker
        colors: ambience.palette
    }

    FileDialog {
        id: picker
        title: qsTr("Choose a Cover Photo")
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.tif *.tiff *.avif *.heic)"]
        onAccepted: {
            maker.photo = selectedFile
            if (maker.photoValid) {
                root.problem = ""
                cropper.begin()
            } else {
                root.problem = qsTr("That image could not be used as a cover.")
            }
        }
    }

    CoverCropper {
        id: cropper
        maker: maker
        onUsed: (jpeg) => {
            root.pickedCover = jpeg
            root.selectedDesign = -1
        }
    }

    component Tile: RippleButton {
        id: tile

        property bool selected: false

        width: root.tileEdge
        height: root.tileEdge
        padding: 0
        buttonRadius: 10
        colBackground: "transparent"
        focusPolicy: Qt.NoFocus

        // drawn inside the tile, so nothing hangs past the carousel's clip
        Rectangle {
            anchors.fill: parent
            radius: 10
            color: "transparent"
            border.width: 3
            border.color: Qt.rgba(Appearance.colors.primary.r, Appearance.colors.primary.g, Appearance.colors.primary.b,
                                  tile.selected ? 1 : 0)
            z: 2

            Behavior on border.color {
                ColorAnimation { duration: Appearance.duration(200) }
            }
        }
    }

    ListView {
        id: carousel
        width: parent.width
        height: root.tileEdge
        orientation: ListView.Horizontal
        spacing: 12
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: 1 + maker.designCount

        delegate: Tile {
            id: tile

            required property int index

            selected: index === 0 ? root.pickedCover !== null && root.selectedDesign < 0 : root.selectedDesign === index - 1
            tooltip: index === 0 ? qsTr("Choose a photo for the cover") : ""
            onClicked: {
                if (index === 0) {
                    picker.open()
                } else {
                    root.selectedDesign = index - 1
                }
            }

            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    visible: tile.index === 0
                    radius: 10
                    color: Appearance.colors.surfaceContainerHighest

                    Column {
                        anchors.centerIn: parent
                        Reveal.shown: root.pickedCover === null
                        spacing: 5

                        MaterialSymbol {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "add_photo_alternate"
                            iconSize: 24
                            color: Appearance.colors.onSurfaceVariant
                        }
                        StyledText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Photo")
                            font.pixelSize: Appearance.fontSize.smaller
                            color: Appearance.colors.onSurfaceVariant
                        }
                    }

                    // the cropped photo, straight from its jpeg rather than
                    // through the artwork cache
                    Image {
                        id: picked
                        visible: false
                        source: root.pickedCover !== null ? maker.preview(root.pickedCover) : ""
                    }
                    ShaderEffect {
                        anchors.fill: parent
                        Reveal.shown: root.pickedCover !== null && picked.status === Image.Ready

                        property variant source: picked
                        property size itemSize: Qt.size(width, height)
                        property real radius: 10
                        property real borderWidth: 0
                        property color borderColor: "transparent"
                        property size uvScale: Qt.size(1, 1)
                        property point uvOffset: Qt.point(0, 0)

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
                }

                PlaylistCover {
                    anchors.fill: parent
                    visible: tile.index > 0
                    radius: 10
                    colors: maker.colors
                    design: Math.max(0, tile.index - 1)
                    title: nameField.text
                }
            }
        }
    }

    StyledText {
        width: parent.width
        Reveal.shown: root.problem.length > 0
        text: root.problem
        wrapMode: Text.Wrap
        font.pixelSize: Appearance.fontSize.smaller
        color: Appearance.colors.error
    }

    OutlinedTextField {
        id: nameField
        width: parent.width
        placeholder: qsTr("Name")
        onAccepted: root.save()
    }

    OutlinedTextField {
        id: descriptionField
        width: parent.width
        placeholder: qsTr("Description")
        minimumLines: 3
        maximumLines: 6
    }

    Item {
        width: parent.width
        height: 36

        StyledText {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Public playlist")
        }
        StyledSwitch {
            id: publicSwitch
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    buttons: [
        TextButton {
            id: saveButton
            text: qsTr("Save")
            busy: root.saving
            enabled: !root.nameIsEmpty && !root.saving
            onClicked: root.save()
        },
        TextButton {
            variant: TextButton.Text
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    ]
}
