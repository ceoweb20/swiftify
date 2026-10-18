pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import Swiftify

// the macos unified toolbar as a material top app bar: back, the open
// playlist's actions, a search field, and the playback menu. it has no
// background: the pages scroll up under it, and each button sits on a shape
// of its own. it is also the handle the window is moved by.
Item {
    id: root

    required property Window window
    property Item backdropSource
    property bool windowControlsOnRight: false
    property bool inspectorShown: false
    readonly property bool onPlaylist: Library.page === Library.Page.Playlist && Library.selectedPlaylist.valid
    readonly property bool searching: Session.libraryConnected && Library.page === Library.Page.Search
    readonly property bool compact: width < 600
    readonly property real buttonSize: width < 420 ? 26 : (compact ? 34 : 46)
    readonly property real searchLeftEdge: back.x + back.width + 8
    readonly property real searchRightEdge: rightControls.x - 8
    // Reserve the collapsed field's space even while its overlay is focused.
    readonly property real reservedSearchWidth: Math.max(40, Math.min(340, searchRightEdge - searchLeftEdge - (onPlaylist ? playlistActions.implicitWidth + 8 : 0)))
    readonly property string playlistLink: "https://open.spotify.com/playlist/" + Library.selectedPlaylist.id

    function focusSearch() {
        if (search.visible) {
            search.focusField();
        }
    }

    implicitHeight: Appearance.sizes.topBarHeight

    DragHandler {
        target: null
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        onActiveChanged: if (active)
            root.window.startSystemMove()
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: root.window.visibility === Window.Maximized ? root.window.showNormal() : root.window.showMaximized()
    }

    Item {
        id: toolbarButtons
        anchors.fill: parent

        ToolbarButton {
            id: back
            effect: ToolbarButton.NudgeLeft
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            size: root.buttonSize
            iconSize: root.compact ? 20 : 26
            symbol: "arrow_back"
            // it sits on an arrow, pointing the way it goes
            shape: "arrow"
            shapeAngle: -90
            turnsOnHover: false
            enabled: Library.canGoBack
            onClicked: Library.goBack()
            tooltip: qsTr("Back")
        }

        Row {
            id: rightControls
            anchors.right: parent.right
            anchors.rightMargin: 12 + (root.windowControlsOnRight && !root.inspectorShown ? 96 : 0)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            ToolbarButton {
                id: overflow
                effect: ToolbarButton.Spin
                size: root.buttonSize
                iconSize: root.compact ? 20 : 26
                shape: "cookie9"
                anchors.verticalCenter: parent.verticalCenter
                symbol: "more_vert"
                onClicked: playbackMenu.popupAt(overflow, 0, overflow.height)
                tooltip: qsTr("Playback")
            }

        }

        Row {
            id: playlistActions
            objectName: "playlistActions"
            x: root.searchRightEdge - root.reservedSearchWidth - 8 - width
            anchors.verticalCenter: parent.verticalCenter
            spacing: root.compact ? 0 : 2
            Reveal.shown: root.onPlaylist

            ToolbarButton {
                effect: ToolbarButton.Spin
                size: root.buttonSize
                iconSize: root.compact ? 19 : 26
                shape: "clover4"
                symbol: "add"
                onClicked: UiState.beginCreatePlaylist()
                tooltip: qsTr("New Playlist")
            }
            ToolbarButton {
                effect: ToolbarButton.Pop
                Reveal.shown: Library.selectedPlaylistEditable
                size: root.buttonSize
                iconSize: root.compact ? 19 : 26
                shape: "puffy"
                symbol: "edit"
                onClicked: UiState.editPlaylistRequested()
                tooltip: qsTr("Edit Playlist")
            }
            ToolbarButton {
                effect: ToolbarButton.Pop
                size: root.buttonSize
                iconSize: root.compact ? 19 : 26
                shape: "cookie6"
                symbol: "share"
                onClicked: {
                    App.copyToClipboard(root.playlistLink);
                    UiState.toastRequested(qsTr("Playlist link copied"));
                }
                tooltip: qsTr("Copy Link")
            }
            ToolbarButton {
                id: more
                effect: ToolbarButton.Pop
                size: root.buttonSize
                iconSize: root.compact ? 19 : 26
                shape: "sunny"
                symbol: "more_horiz"
                onClicked: moreMenu.popupAt(more, 0, more.height)
                tooltip: qsTr("More")
            }
            ToolbarButton {
                id: sort
                objectName: "sortButton"
                effect: ToolbarButton.Pop
                size: root.buttonSize
                iconSize: root.compact ? 19 : 26
                shape: "flower"
                symbol: "sort"
                onClicked: sortMenu.popupAt(sort, 0, sort.height)
                tooltip: qsTr("Sort Options")
            }
        }
    }

    Rectangle {
        id: searchMask
        width: search.width
        height: search.height
        radius: height / 2
        layer.enabled: true
        visible: false
    }

    Item {
        id: searchBlur
        anchors.left: search.left
        anchors.right: search.right
        anchors.top: search.top
        anchors.bottom: search.bottom
        z: 8
        visible: search.editing && !!root.backdropSource

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: Appearance.colors.layer1
        }

        ShaderEffectSource {
            id: searchBackdrop
            anchors.fill: parent
            sourceItem: root.backdropSource
            live: searchBlur.visible
            hideSource: false
            sourceRect: {
                const origin = root.mapToItem(root.backdropSource, search.x, search.y);
                return Qt.rect(origin.x, origin.y, search.width, search.height);
            }
            textureSize: Qt.size(Math.max(1, Math.ceil(search.width)), Math.max(1, Math.ceil(search.height)))
            visible: false
        }
        MultiEffect {
            anchors.fill: parent
            source: searchBackdrop
            blurEnabled: true
            blur: 0.8
            blurMax: 48
            maskEnabled: true
            maskSource: searchMask
        }
        // Capture only the buttons, so the field never samples itself.
        ShaderEffectSource {
            id: buttonBackdrop
            anchors.fill: parent
            sourceItem: toolbarButtons
            sourceRect: Qt.rect(search.x, search.y, search.width, search.height)
            live: searchBlur.visible
            hideSource: false
            visible: false
        }
        MultiEffect {
            anchors.fill: parent
            source: buttonBackdrop
            blurEnabled: true
            blur: 0.8
            blurMax: 48
            maskEnabled: true
            maskSource: searchMask
        }
        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: Qt.alpha(Appearance.colors.surfaceContainerHighest, 0.18)
        }
    }

    SearchField {
        id: search
        objectName: "toolbarSearch"
        x: root.searchRightEdge - width
        anchors.verticalCenter: parent.verticalCenter
        z: editing ? 9 : 0
        Reveal.shown: root.searching || root.onPlaylist
        width: {
            if (!visible)
                return 0;
            return editing ? Math.max(40, Math.min(340, root.searchRightEdge - root.searchLeftEdge)) : root.reservedSearchWidth;
        }
        placeholder: root.searching ? qsTr("Search Spotify") : qsTr("Find in Playlist")
        text: root.searching ? Library.searchQuery : Library.playlistFilterQuery
        onTextChanged: {
            if (root.searching) {
                Library.searchQuery = text;
            } else if (root.onPlaylist) {
                Library.playlistFilterQuery = text;
            }
        }

        Behavior on width {
            ElementMoveSmall {}
        }
    }

    MenuLoader {
        id: playbackMenu
        sourceComponent: PlaybackMenu {}
    }

    MenuLoader {
        id: moreMenu
        sourceComponent: StyledMenu {
            StyledMenuItem {
                text: qsTr("New Playlist")
                symbol: "add"
                onTriggered: UiState.beginCreatePlaylist()
            }
            StyledMenuItem {
                visible: Library.selectedPlaylistEditable
                height: visible ? implicitHeight : 0
                text: qsTr("Edit Playlist")
                symbol: "edit"
                onTriggered: UiState.editPlaylistRequested()
            }
            StyledMenuItem {
                text: qsTr("Open in Spotify")
                symbol: "open_in_new"
                onTriggered: App.openUrl(root.playlistLink)
            }
            StyledMenuSeparator {
                visible: Library.selectedPlaylistEditable
                height: visible ? implicitHeight : 0
            }
            StyledMenuItem {
                visible: Library.selectedPlaylistEditable
                height: visible ? implicitHeight : 0
                text: qsTr("Delete Playlist")
                symbol: "delete"
                destructive: true
                onTriggered: UiState.deletePlaylistRequested(Library.selectedPlaylist)
            }
        }
    }

    MenuLoader {
        id: sortMenu
        objectName: "sortMenu"
        sourceComponent: StyledMenu {
            Repeater {
                model: [
                    {
                        label: qsTr("Playlist Order"),
                        order: Library.SortOrder.Playlist
                    },
                    {
                        label: qsTr("Title"),
                        order: Library.SortOrder.Title
                    },
                    {
                        label: qsTr("Artist"),
                        order: Library.SortOrder.Artist
                    },
                    {
                        label: qsTr("Album"),
                        order: Library.SortOrder.Album
                    },
                    {
                        label: qsTr("Time"),
                        order: Library.SortOrder.Duration
                    }
                ]

                StyledMenuItem {
                    required property var modelData
                    text: modelData.label
                    checkmark: true
                    checked: Library.playlistSortOrder === modelData.order
                    trailingSymbol: checked ? (Library.playlistSortReversed ? "arrow_downward" : "arrow_upward") : ""
                    onTriggered: Library.togglePlaylistSortOrder(modelData.order)
                }
            }
        }
    }
}
