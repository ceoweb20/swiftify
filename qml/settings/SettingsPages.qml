pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// the settings themselves (SwiftifySettingsView), one page per rail
// destination: what the visualizers and the backdrop cost, the sleep fade and
// audio quality, the folders of music of the person's own, how the app fits
// the desktop, how much it moves and draws, the
// spotify logins, and about. every change applies at once.
Item {
    id: root

    property int page: 0
    readonly property list<string> fades: ["none", "finalSeconds", "entire"]
    readonly property list<int> qualities: [96, 160, 320]
    readonly property list<int> cacheSizes: [500, 1000, 2000]

    component Page: Column {
        width: parent ? parent.width : 0
        spacing: 24
    }

    /// a login's state: a label, and the state beside a dot that says
    /// whether all is well
    component StatusRow: Item {
        id: status

        property string label
        property string value
        property bool good: false

        width: parent ? parent.width : 0
        height: 40

        StyledText {
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            text: status.label
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 8
                height: 8
                radius: 4
                color: status.good ? Appearance.colors.primary : Appearance.colors.error
            }
            StyledText {
                anchors.verticalCenter: parent.verticalCenter
                text: status.value
                color: Appearance.colors.onSurfaceVariant
            }
        }
    }

    implicitHeight: pages.implicitHeight

    // only the chosen page is visible, and a column leaves the rest no room
    Column {
        id: pages
        width: parent.width

        Page {
            visible: root.page === 0

            ContentSection {
                title: "Visualizer"
                symbol: "graphic_eq"

                ConfigSwitch {
                    text: "Show audio visualizers"
                    checked: Settings.visualizersEnabled
                    onToggledTo: (checked) => Settings.visualizersEnabled = checked
                }
                ConfigSelection {
                    text: "Style"
                    enabled: Settings.visualizersEnabled
                    currentIndex: Settings.circularVisualizer ? 1 : 0
                    segments: [
                        Segment { text: "Normal" },
                        Segment { text: "Circular" }
                    ]
                    onActivated: (index) => Settings.circularVisualizer = index === 1
                }
                ConfigSelection {
                    text: "Shape"
                    enabled: Settings.visualizersEnabled
                    currentIndex: Settings.visualizerShape === "wave" ? 1 : 0
                    segments: [
                        Segment { text: "Bars" },
                        Segment { text: "Wave" }
                    ]
                    onActivated: (index) => Settings.visualizerShape = index === 1 ? "wave" : "bars"
                }
                ConfigSwitch {
                    text: "Round artwork"
                    // with visualizers on, the circular style rounds the
                    // cover inside its ring
                    enabled: !Settings.visualizersEnabled
                    checked: Settings.roundArtwork || (Settings.visualizersEnabled && Settings.circularVisualizer)
                    onToggledTo: (checked) => Settings.roundArtwork = checked
                }
                ConfigSlider {
                    text: "Sensitivity"
                    enabled: Settings.visualizersEnabled
                    from: 0.6
                    to: 1.8
                    stepSize: 0.05
                    value: Settings.visualizerSensitivity
                    valueText: Math.round(Settings.visualizerSensitivity * 100) + "%"
                    onMoved: (value) => Settings.visualizerSensitivity = value
                }
                NoteText {
                    text: "turn this off if u want lower cpu cost"
                }
            }
        }

        Page {
            visible: root.page === 1

            ContentSection {
                title: "Background"
                symbol: "wallpaper"

                ConfigSwitch {
                    text: "Live background"
                    checked: Settings.livingBackdrop
                    onToggledTo: (checked) => Settings.livingBackdrop = checked
                }
                ConfigSlider {
                    text: "Blur"
                    from: 0
                    to: 60
                    stepSize: 1
                    value: Settings.backdropBlur
                    onMoved: (value) => Settings.backdropBlur = value
                }
                ConfigSlider {
                    text: "Reactivity"
                    enabled: Settings.livingBackdrop
                    from: 0
                    to: 3
                    stepSize: 0.1
                    value: Settings.backdropReactivity
                    valueText: Math.round(Settings.backdropReactivity * 100) + "%"
                    onMoved: (value) => Settings.backdropReactivity = value
                }
                NoteText {
                    text: "turn this off if u want lower cpu cost"
                }
            }
        }

        Page {
            visible: root.page === 2

            ContentSection {
                title: "Crossfade"
                symbol: "play_circle"

                ConfigSwitch {
                    text: "Crossfade between songs"
                    checked: Settings.crossfadeEnabled
                    onToggledTo: (checked) => Settings.crossfadeEnabled = checked
                }
                ConfigSlider {
                    text: "Crossfade duration"
                    enabled: Settings.crossfadeEnabled
                    from: 1
                    to: 12
                    stepSize: 1
                    value: Settings.crossfadeDurationSeconds
                    valueText: Settings.crossfadeDurationSeconds + " s"
                    onMoved: (value) => Settings.crossfadeDurationSeconds = Math.round(value)
                }
            }

            ContentSection {
                title: "Sleep Timer"
                symbol: "bedtime"

                ConfigSelection {
                    text: "Fade Out"
                    currentIndex: Math.max(0, root.fades.indexOf(Settings.sleepTimerFade))
                    segments: [
                        Segment { text: "None" },
                        Segment { text: "Last 10 Seconds" },
                        Segment { text: "Whole Timer" }
                    ]
                    onActivated: (index) => Settings.sleepTimerFade = root.fades[index]
                }
                NoteText {
                    text: "fading lowers the volume as the timer runs down and restores it once playback stops."
                }
            }

            ContentSection {
                title: "Audio Quality"
                symbol: "high_quality"

                ConfigSelection {
                    text: "Streaming"
                    currentIndex: Math.max(0, root.qualities.indexOf(Settings.streamingQuality))
                    segments: [
                        Segment { text: "96 kbps" },
                        Segment { text: "160 kbps" },
                        Segment { text: "320 kbps" }
                    ]
                    onActivated: (index) => Settings.streamingQuality = root.qualities[index]
                }
                ConfigSelection {
                    text: "Song cache"
                    currentIndex: Math.max(0, root.cacheSizes.indexOf(Settings.songCacheMegabytes))
                    segments: [
                        Segment { text: "500 MB" },
                        Segment { text: "1 GB" },
                        Segment { text: "2 GB" }
                    ]
                    onActivated: (index) => Settings.songCacheMegabytes = root.cacheSizes[index]
                }
                NoteText {
                    text: "both cache and quality apply the next time playback connects"
                }
            }
        }

        Page {
            visible: root.page === 3

            ContentSection {
                title: "Local Files"
                symbol: "folder_open"

                MusicPicker {
                    id: musicPicker
                }

                Repeater {
                    model: LocalFiles.sources

                    Item {
                        id: source

                        required property string modelData

                        width: parent ? parent.width : 0
                        height: 48

                        MaterialSymbol {
                            id: sourceSymbol
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: source.modelData.match(/\.[A-Za-z0-9]{2,4}$/) ? "audio_file" : "folder"
                            iconSize: 20
                            color: Appearance.colors.onSurfaceVariant
                        }
                        StyledText {
                            anchors.left: sourceSymbol.right
                            anchors.leftMargin: 12
                            anchors.right: removeSource.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: source.modelData
                            elide: Text.ElideMiddle
                        }
                        IconButton {
                            id: removeSource
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            symbol: "close"
                            size: 32
                            iconSize: 18
                            tooltip: "Remove"
                            onClicked: LocalFiles.removeSource(source.modelData)
                        }
                    }
                }

                Flow {
                    x: 8
                    width: parent.width - 16
                    spacing: 8
                    topPadding: LocalFiles.sources.length > 0 ? 4 : 0
                    bottomPadding: 8

                    TextButton {
                        variant: TextButton.Tonal
                        symbol: "create_new_folder"
                        text: "Add Folder"
                        onClicked: musicPicker.pickFolder()
                    }
                    TextButton {
                        variant: TextButton.Text
                        symbol: "audio_file"
                        text: "Add Files"
                        onClicked: musicPicker.pickFiles()
                    }
                    TextButton {
                        variant: TextButton.Text
                        symbol: "refresh"
                        text: LocalFiles.scanning ? "Looking…" : "Look Again"
                        enabled: LocalFiles.sources.length > 0 && !LocalFiles.scanning
                        onClicked: LocalFiles.rescan()
                    }
                }

                NoteText {
                    text: "each songs title, artists, album and cover come from its own tags, and a .lrc file "
                          + "beside a song gives its lyrics. you can also drop files or folders onto the window"
                }
            }
        }

        Page {
            visible: root.page === 4

            ContentSection {
                title: "Colours"
                symbol: "palette"

                ConfigSelection {
                    text: "Light or dark"
                    // Matugen exports can contain both light and dark variants.
                    currentIndex: Math.max(0, ["system", "light", "dark"].indexOf(Settings.themeMode))
                    segments: [
                        Segment { text: "System"; symbol: "brightness_auto" },
                        Segment { text: "Light"; symbol: "light_mode" },
                        Segment { text: "Dark"; symbol: "dark_mode" }
                    ]
                    onActivated: (index) => Settings.themeMode = ["system", "light", "dark"][index]
                }

                Flow {
                    x: 8
                    width: parent.width - 16
                    spacing: 10
                    topPadding: 6
                    bottomPadding: 10

                    Repeater {
                        model: Theme.themeOptions(Theme.dark)

                        delegate: ThemeSwatch {
                            required property themeOption modelData
                            option: modelData
                            selected: modelData.id === Settings.themeSource
                            onClicked: Settings.themeSource = modelData.id
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: path.implicitHeight + automatic.implicitHeight + 24
                    visible: Settings.themeSource === "system"

                    OutlinedTextField {
                        id: path
                        x: 8
                        y: 8
                        width: parent.width - 16
                        placeholder: "Automatic (optional JSON file path)"
                        text: Settings.matugenColorsPath
                        onAccepted: Settings.matugenColorsPath = text
                    }
                    TextButton {
                        id: automatic
                        x: 8
                        y: path.y + path.height + 8
                        variant: TextButton.Tonal
                        symbol: "refresh"
                        text: "Auto-detect"
                        onClicked: {
                            path.text = ""
                            Settings.matugenColorsPath = ""
                            Theme.refreshSystemScheme()
                        }
                    }
                    Connections {
                        target: path.inputItem
                        function onActiveFocusChanged() {
                            if (!path.inputItem.activeFocus) {
                                Settings.matugenColorsPath = path.text
                            }
                        }
                    }
                }
                NoteText {
                    text: Settings.themeSource !== "system"
                        ? "these are hardcoded themes"
                        : (Theme.usingSystemScheme
                            ? "following " + Theme.systemSchemePath + "; updates when the file changes."
                            : Settings.matugenColorsPath.length > 0
                              ? "no valid Matugen scheme found at " + Theme.systemSchemePath
                              : "automatically looks for Matugen JSON in common config and cache folders. an optional path overrides discovery.")
                }
            }

            ContentSection {
                // a mac window keeps its traffic lights, and has no tray
                visible: !App.macOS
                title: "Window"
                symbol: "web_asset"

                ConfigSelection {
                    objectName: "windowControlsMode"
                    visible: App.windows
                    text: "Window controls"
                    currentIndex: Settings.nativeTitleBar ? 0 : 1
                    segments: [
                        Segment { text: "Native title bar" },
                        Segment { text: "In-app" }
                    ]
                    onActivated: (index) => Settings.nativeTitleBar = index === 0
                }
                ConfigSwitch {
                    text: "Window buttons"
                    visible: !App.windows
                    checked: Settings.showWindowControls
                    onToggledTo: (checked) => Settings.showWindowControls = checked
                }
                ConfigSwitch {
                    objectName: "windowControlsOnRightSwitch"
                    text: App.windows ? "Window controls on right" : "Window buttons on right"
                    checked: Settings.windowControlsOnRight
                    enabled: App.windows ? !Settings.nativeTitleBar : Settings.showWindowControls
                    onToggledTo: (checked) => Settings.windowControlsOnRight = checked
                }
                ConfigSwitch {
                    text: App.windows ? "Always show tray icon" : "Tray icon"
                    checked: Settings.trayIcon
                    onToggledTo: (checked) => Settings.trayIcon = checked
                }
                NoteText {
                    text: App.windows
                        ? "closing the window keeps Swiftify running in the tray. choose Quit Swiftify from the tray menu to exit."
                        : "window buttons are for desktops that draw none of their own, Hyprland closes and moves windows itself"
                }
            }
        }

        Page {
            visible: root.page === 5

            ContentSection {
                title: "Motion"
                symbol: "animation"

                ConfigSwitch {
                    text: "Reduce motion"
                    checked: Settings.reduceMotion || Settings.performanceMode
                    enabled: !Settings.performanceMode
                    onToggledTo: (checked) => Settings.reduceMotion = checked
                }
                ConfigSwitch {
                    text: "Performance mode"
                    checked: Settings.performanceMode
                    onToggledTo: (checked) => Settings.performanceMode = checked
                }
                NoteText {
                    text: "reduced motion stops anything from constantly animating on its own, interactions still animate tho. "
                        + "performance mode goes way further, NOTHING animates! drastically improves performance but looks ugly imo"
                }
            }

            ContentSection {
                title: "Shapes"
                symbol: "category"

                ConfigSwitch {
                    text: "No shapes"
                    checked: Settings.noShapes
                    onToggledTo: (checked) => Settings.noShapes = checked
                }
                NoteText {
                    text: "less shapes if you prefer minimalistic look"
                }
            }
        }

        // the logins: who is signed in, the library's and playback's state,
        // connecting them again, and logging out
        Page {
            visible: root.page === 6

            ContentSection {
                title: "Discord Rich Presence"
                symbol: "share"

                ConfigSwitch {
                    text: qsTr("Share listening activity")
                    checked: Settings.discordEnabled
                    onToggledTo: (checked) => Settings.discordEnabled = checked
                }
                OutlinedTextField {
                    id: discordId
                    x: 8
                    width: Math.min(460, parent.width - 16)
                    placeholder: qsTr("Discord Application ID")
                    text: Settings.discordApplicationId
                    onAccepted: Settings.discordApplicationId = text
                    Connections {
                        target: discordId.inputItem
                        function onActiveFocusChanged() {
                            if (!discordId.inputItem.activeFocus) Settings.discordApplicationId = discordId.text
                        }
                    }
                }
                NoteText {
                    text: discordId.text.length > 0 && !/^[0-9]{17,20}$/.test(discordId.text.trim())
                        ? qsTr("Enter a valid numeric Application ID (17–20 digits).")
                        : !Settings.discordEnabled ? qsTr("Listening activity is private. Enable to share track, artist and progress with Discord.")
                        : Settings.discordApplicationId.length === 0 ? qsTr("Create an application in the Discord Developer Portal and paste its Application ID above. No token or secret is needed.")
                        : Discord.status
                }
                TextButton {
                    x: 8
                    text: qsTr("Open Discord Developer Portal")
                    variant: TextButton.Text
                    onClicked: Qt.openUrlExternally("https://discord.com/developers/applications")
                }
            }

            ContentSection {
                title: "Account"
                symbol: "account_circle"

                Item {
                    width: parent.width
                    height: 72

                    Item {
                        id: avatar
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: 48
                        height: 48

                        ArtworkImage {
                            anchors.fill: parent
                            Reveal.shown: Library.profileArtworkUrl.toString().length > 0
                            circular: true
                            source: Library.profileArtworkUrl
                            placeholderSymbol: "person"
                        }
                        MaterialSymbol {
                            anchors.centerIn: parent
                            Reveal.shown: Library.profileArtworkUrl.toString().length === 0
                            text: "account_circle"
                            iconSize: 48
                            fill: 1
                            color: Appearance.colors.onSurfaceVariant
                        }
                    }

                    Column {
                        anchors.left: avatar.right
                        anchors.leftMargin: 18
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        StyledText {
                            width: parent.width
                            text: Library.profileName.length > 0 ? Library.profileName : "Spotify"
                            elide: Text.ElideRight
                            font.pixelSize: Appearance.fontSize.larger
                            font.weight: Font.DemiBold
                        }
                        StyledText {
                            width: parent.width
                            text: Session.libraryConnected && Session.connected ? "Library and playback connected"
                                : Session.libraryConnected ? "Library connected"
                                : Session.connected ? "Playback connected" : "Not signed in"
                            elide: Text.ElideRight
                            color: Appearance.colors.onSurfaceVariant
                        }
                    }
                }

                Flow {
                    x: 8
                    width: parent.width - 16
                    spacing: 8

                    TextButton {
                        symbol: "logout"
                        text: "Log Out"
                        colBackground: Appearance.colors.error
                        colBackgroundHover: Appearance.colors.errorHover
                        labelColor: Appearance.colors.onError
                        enabled: Session.libraryConnected || Session.connected
                        onClicked: logOutDialog.open()
                    }
                }
                NoteText {
                    text: "forgets both logins on this computer, local files stay of course"
                }

                StyledDialog {
                    id: logOutDialog
                    headline: "Log Out of Spotify?"
                    symbol: "logout"
                    iconShape: "ghostish"
                    body: "swiftify forgets your library and playback logins on this computer. you can connect again any time."

                    buttons: [
                        TextButton {
                            text: "Log Out"
                            colBackground: Appearance.colors.error
                            colBackgroundHover: Appearance.colors.errorHover
                            labelColor: Appearance.colors.onError
                            onClicked: {
                                Session.logOut()
                                logOutDialog.accept()
                            }
                        },
                        TextButton {
                            variant: TextButton.Text
                            text: "Cancel"
                            onClicked: logOutDialog.close()
                        }
                    ]
                }
            }

            ContentSection {
                title: "Library"
                symbol: "library_music"

                StatusRow {
                    label: "Status"
                    value: Session.libraryStatus
                    good: Session.libraryConnected && !Session.libraryNeedsAuthorizationUpgrade
                }
                OutlinedTextField {
                    x: 8
                    width: Math.min(460, parent.width - 16)
                    placeholder: "Spotify Client ID"
                    text: Session.libraryClientId
                    onTextEdited: Session.libraryClientId = text
                }
                Flow {
                    x: 8
                    width: parent.width - 16
                    spacing: 8
                    topPadding: 4

                    TextButton {
                        variant: TextButton.Tonal
                        symbol: "library_music"
                        text: Session.libraryConnected ? "Reconnect Library" : "Connect Library"
                        enabled: Session.libraryClientId.trim().length > 0 && !Session.loginInProgress
                        onClicked: Session.beginLibraryLogin()
                    }
                }
                NoteText {
                    text: Session.libraryNeedsAuthorizationUpgrade
                          ? "reconnect once to let Swiftify edit playlists and save songs"
                          : "your playlists, saved songs and search come through the Spotify app whose Client ID is above."
                }
            }

            ContentSection {
                title: "Playback"
                symbol: "play_circle"

                StatusRow {
                    label: "Status"
                    value: Session.connected ? "Connected" : "Not connected"
                    good: Session.connected
                }
                Flow {
                    x: 8
                    width: parent.width - 16
                    spacing: 8
                    topPadding: 4

                    TextButton {
                        variant: TextButton.Tonal
                        symbol: "play_circle"
                        text: Session.connected ? "Reconnect Playback" : "Connect Playback"
                        enabled: !Session.loginInProgress
                        onClicked: Session.beginPlaybackLogin()
                    }
                }
                NoteText {
                    text: "playing music needs a Spotify Premium login of its own, separate from the librarys"
                }
            }
        }

        Page {
            visible: root.page === 7

            ContentSection {
                title: "Debugging"
                symbol: "bug_report"

                ConfigSwitch {
                    text: "Debugging shortcuts"
                    checked: Settings.debuggingEnabled
                    onToggledTo: (checked) => Settings.debuggingEnabled = checked
                }
                NoteText {
                    text: "Adds a seconds dial to the sleep timer, so a fade can be watched without waiting out a long ass timer"
                }

                Flow {
                    x: 8
                    width: parent.width - 16
                    spacing: 8
                    topPadding: 4

                    TextButton {
                        variant: TextButton.Tonal
                        symbol: "hourglass_empty"
                        text: "Play Loading Test"
                        onClicked: Player.playLoadingTest()
                    }
                }
                NoteText {
                    text: "plays a song that never finishes loading, so the visualizers loading animation can be watched"
                }
            }

            ContentSection {
                id: about

                property bool revealed: false

                title: "About"
                symbol: "info"

                Row {
                    x: 12
                    width: parent.width - 24
                    topPadding: 8
                    bottomPadding: 8
                    spacing: 16

                    // the app's icon
                    Item {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 72
                        height: 72

                        Image {
                            anchors.centerIn: parent
                            width: 46
                            height: 46
                            source: "qrc:/qt/qml/Swiftify/resources/swiftify-icon.png"
                            sourceSize: Qt.size(96, 96)
                            smooth: true
                            mipmap: true
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        StyledText {
                            text: "Swiftify " + App.version
                            font.weight: Font.Medium

                            TapHandler {
                                onTapped: about.revealed = true
                            }
                            HoverHandler {
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                        StyledText {
                            text: "developed by addenator, the best programmer on earth"
                            font.pixelSize: Appearance.fontSize.smaller
                            color: Appearance.colors.onSurfaceVariant
                        }
                        StyledText {
                            topPadding: 8
                            visible: opacity > 0
                            opacity: about.revealed ? 1 : 0
                            scale: about.revealed ? 1 : 0.92
                            transformOrigin: Item.Left
                            text: "no secret dev options here..."
                            font.weight: Font.DemiBold
                            color: Appearance.colors.primary

                            Behavior on opacity {
                                NumberAnimation { duration: Appearance.duration(250); easing.type: Easing.OutCubic }
                            }
                            Behavior on scale {
                                ElementMoveSmall {}
                            }
                        }
                    }
                }
            }
        }
    }
}
