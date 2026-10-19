pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// the settings themselves (SwiftifySettingsView), one page per rail
// destination: what the visualizers and the backdrop cost, the sleep fade and
// audio quality, how the app fits the desktop, how much it moves and draws,
// the spotify logins, and about. every change applies at once.
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
                title: qsTr("Visualizer")
                symbol: "graphic_eq"

                ConfigSwitch {
                    text: qsTr("Show audio visualizers")
                    checked: Settings.visualizersEnabled
                    onToggledTo: (checked) => Settings.visualizersEnabled = checked
                }
                ConfigSwitch {
                    text: qsTr("Player bar visualizer")
                    enabled: Settings.visualizersEnabled
                    checked: Settings.playerBarVisualizer
                    onToggledTo: (checked) => Settings.playerBarVisualizer = checked
                }
                ConfigSwitch {
                    text: qsTr("Track list visualizer")
                    enabled: Settings.visualizersEnabled
                    checked: Settings.trackListVisualizer
                    onToggledTo: (checked) => Settings.trackListVisualizer = checked
                }
                ConfigSwitch {
                    objectName: "immersiveVisualizerSwitch"
                    text: qsTr("Full-screen player visualizer")
                    enabled: Settings.visualizersEnabled
                    checked: Settings.immersiveVisualizer
                    onToggledTo: (checked) => Settings.immersiveVisualizer = checked
                }
                ConfigSelection {
                    text: qsTr("Style")
                    enabled: Settings.visualizersEnabled && Settings.immersiveVisualizer
                    currentIndex: Settings.circularVisualizer ? 1 : 0
                    segments: [
                        Segment { text: qsTr("Normal") },
                        Segment { text: qsTr("Circular") }
                    ]
                    onActivated: (index) => Settings.circularVisualizer = index === 1
                }
                ConfigSelection {
                    text: qsTr("Shape")
                    enabled: Settings.visualizersEnabled && Settings.immersiveVisualizer
                    currentIndex: Settings.visualizerShape === "wave" ? 1 : 0
                    segments: [
                        Segment { text: qsTr("Bars") },
                        Segment { text: qsTr("Wave") }
                    ]
                    onActivated: (index) => Settings.visualizerShape = index === 1 ? "wave" : "bars"
                }
                ConfigSwitch {
                    text: qsTr("Round artwork")
                    // with visualizers on, the circular style rounds the
                    // cover inside its ring
                    enabled: !Settings.visualizersEnabled || !Settings.immersiveVisualizer
                    checked: Settings.roundArtwork || (Settings.visualizersEnabled && Settings.immersiveVisualizer && Settings.circularVisualizer)
                    onToggledTo: (checked) => Settings.roundArtwork = checked
                }
                ConfigSlider {
                    text: qsTr("Sensitivity")
                    enabled: Settings.visualizersEnabled && (Settings.playerBarVisualizer || Settings.trackListVisualizer || Settings.immersiveVisualizer)
                    from: 0.6
                    to: 1.8
                    stepSize: 0.05
                    value: Settings.visualizerSensitivity
                    valueText: Math.round(Settings.visualizerSensitivity * 100) + "%"
                    onMoved: (value) => Settings.visualizerSensitivity = value
                }
                NoteText {
                    text: qsTr("turn this off if u want lower cpu cost")
                }
            }
        }

        Page {
            visible: root.page === 1

            ContentSection {
                title: qsTr("Background")
                symbol: "wallpaper"

                ConfigSwitch {
                    text: qsTr("Live background")
                    checked: Settings.livingBackdrop
                    onToggledTo: (checked) => Settings.livingBackdrop = checked
                }
                ConfigSlider {
                    text: qsTr("Blur")
                    from: 0
                    to: 60
                    stepSize: 1
                    value: Settings.backdropBlur
                    onMoved: (value) => Settings.backdropBlur = value
                }
                ConfigSlider {
                    text: qsTr("Reactivity")
                    enabled: Settings.livingBackdrop
                    from: 0
                    to: 3
                    stepSize: 0.1
                    value: Settings.backdropReactivity
                    valueText: Math.round(Settings.backdropReactivity * 100) + "%"
                    onMoved: (value) => Settings.backdropReactivity = value
                }
                NoteText {
                    text: qsTr("turn this off if u want lower cpu cost")
                }
            }
        }

        Page {
            visible: root.page === 2

            ContentSection {
                title: qsTr("Spotify Connect")
                symbol: "cast"

                ConfigSwitch {
                    objectName: "spotifyConnectSwitch"
                    text: qsTr("Show Swiftify as a Spotify Connect device")
                    checked: Settings.spotifyConnect
                    onToggledTo: (checked) => Settings.spotifyConnect = checked
                }
                OutlinedTextField {
                    x: 8
                    width: Math.min(460, parent.width - 16)
                    placeholder: qsTr("Device name")
                    enabled: Settings.spotifyConnect
                    text: Settings.connectDeviceName
                    onAccepted: Settings.connectDeviceName = text
                    onTextEdited: if (text.trim().length > 0) Settings.connectDeviceName = text
                }
                NoteText {
                    text: qsTr("Spotify, and anything watching your account such as a scrobbling bot, sees what Swiftify plays. Other Spotify apps cannot control Swiftify.")
                }
            }

            ContentSection {
                title: qsTr("Crossfade")
                symbol: "play_circle"

                ConfigSwitch {
                    text: qsTr("Crossfade between songs")
                    checked: Settings.crossfadeEnabled
                    onToggledTo: (checked) => Settings.crossfadeEnabled = checked
                }
                ConfigSlider {
                    text: qsTr("Crossfade duration")
                    enabled: Settings.crossfadeEnabled
                    from: 1
                    to: 12
                    stepSize: 1
                    value: Settings.crossfadeDurationSeconds
                    valueText: Settings.crossfadeDurationSeconds + qsTr(" s")
                    onMoved: (value) => Settings.crossfadeDurationSeconds = Math.round(value)
                }
            }

            ContentSection {
                title: qsTr("Sleep Timer")
                symbol: "bedtime"

                ConfigSelection {
                    text: qsTr("Fade Out")
                    currentIndex: Math.max(0, root.fades.indexOf(Settings.sleepTimerFade))
                    segments: [
                        Segment { text: qsTr("None") },
                        Segment { text: qsTr("Last 10 Seconds") },
                        Segment { text: qsTr("Whole Timer") }
                    ]
                    onActivated: (index) => Settings.sleepTimerFade = root.fades[index]
                }
                NoteText {
                    text: qsTr("fading lowers the volume as the timer runs down and restores it once playback stops.")
                }
            }

            ContentSection {
                title: qsTr("Audio Quality")
                symbol: "high_quality"

                ConfigSelection {
                    text: qsTr("Streaming")
                    currentIndex: Math.max(0, root.qualities.indexOf(Settings.streamingQuality))
                    segments: [
                        Segment { text: qsTr("96 kbps") },
                        Segment { text: qsTr("160 kbps") },
                        Segment { text: qsTr("320 kbps") }
                    ]
                    onActivated: (index) => Settings.streamingQuality = root.qualities[index]
                }
                ConfigSelection {
                    text: qsTr("Song cache")
                    currentIndex: Math.max(0, root.cacheSizes.indexOf(Settings.songCacheMegabytes))
                    segments: [
                        Segment { text: qsTr("500 MB") },
                        Segment { text: qsTr("1 GB") },
                        Segment { text: qsTr("2 GB") }
                    ]
                    onActivated: (index) => Settings.songCacheMegabytes = root.cacheSizes[index]
                }
                NoteText {
                    text: qsTr("both cache and quality apply the next time playback connects")
                }
            }
        }

        Page {
            visible: root.page === 3

            ContentSection {
                title: qsTr("Language")
                symbol: "language"

                ConfigSelection {
                    text: qsTr("Interface language")
                    objectName: "languageSelection"
                    currentIndex: ["system", "en", "ru"].indexOf(Settings.language)
                    segments: [
                        Segment { text: qsTr("System") },
                        Segment { text: "English" },
                        Segment { text: "Русский" }
                    ]
                    onActivated: (index) => Settings.language = ["system", "en", "ru"][index]
                }
            }

            ContentSection {
                title: qsTr("Colours")
                symbol: "palette"

                ConfigSelection {
                    text: qsTr("Light or dark")
                    // Matugen exports can contain both light and dark variants.
                    currentIndex: Math.max(0, ["system", "light", "dark"].indexOf(Settings.themeMode))
                    segments: [
                        Segment { text: qsTr("System"); symbol: "brightness_auto" },
                        Segment { text: qsTr("Light"); symbol: "light_mode" },
                        Segment { text: qsTr("Dark"); symbol: "dark_mode" }
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
                        placeholder: qsTr("Automatic (optional JSON file path)")
                        text: Settings.matugenColorsPath
                        onAccepted: Settings.matugenColorsPath = text
                    }
                    TextButton {
                        id: automatic
                        x: 8
                        y: path.y + path.height + 8
                        variant: TextButton.Tonal
                        symbol: "refresh"
                        text: qsTr("Auto-detect")
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
                        ? qsTr("these are hardcoded themes")
                        : (Theme.usingSystemScheme
                            ? qsTr("following ") + Theme.systemSchemePath + qsTr("; updates when the file changes.")
                            : Settings.matugenColorsPath.length > 0
                              ? qsTr("no valid Matugen scheme found at ") + Theme.systemSchemePath
                              : qsTr("automatically looks for Matugen JSON in common config and cache folders. an optional path overrides discovery."))
                }
            }

            ContentSection {
                // a mac window keeps its traffic lights, and has no tray
                visible: !App.macOS
                title: qsTr("Window")
                symbol: "web_asset"

                ConfigSelection {
                    objectName: "windowControlsMode"
                    visible: App.windows
                    text: qsTr("Window controls")
                    currentIndex: Settings.nativeTitleBar ? 0 : 1
                    segments: [
                        Segment { text: qsTr("Native title bar") },
                        Segment { text: qsTr("In-app") }
                    ]
                    onActivated: (index) => Settings.nativeTitleBar = index === 0
                }
                ConfigSwitch {
                    text: qsTr("Window buttons")
                    visible: !App.windows
                    checked: Settings.showWindowControls
                    onToggledTo: (checked) => Settings.showWindowControls = checked
                }
                ConfigSwitch {
                    objectName: "windowControlsOnRightSwitch"
                    text: App.windows ? qsTr("Window controls on right") : qsTr("Window buttons on right")
                    checked: Settings.windowControlsOnRight
                    enabled: App.windows ? !Settings.nativeTitleBar : Settings.showWindowControls
                    onToggledTo: (checked) => Settings.windowControlsOnRight = checked
                }
                ConfigSwitch {
                    text: App.windows ? qsTr("Always show tray icon") : qsTr("Tray icon")
                    checked: Settings.trayIcon
                    onToggledTo: (checked) => Settings.trayIcon = checked
                }
                NoteText {
                    text: App.windows
                        ? qsTr("closing the window keeps Swiftify running in the tray. choose Quit Swiftify from the tray menu to exit.")
                        : qsTr("window buttons are for desktops that draw none of their own, Hyprland closes and moves windows itself")
                }
            }
        }

        Page {
            visible: root.page === 4

            ContentSection {
                title: qsTr("Motion")
                symbol: "animation"

                ConfigSwitch {
                    text: qsTr("Reduce motion")
                    checked: Settings.reduceMotion || Settings.performanceMode
                    enabled: !Settings.performanceMode
                    onToggledTo: (checked) => Settings.reduceMotion = checked
                }
                ConfigSwitch {
                    text: qsTr("Performance mode")
                    checked: Settings.performanceMode
                    onToggledTo: (checked) => Settings.performanceMode = checked
                }
                NoteText {
                    text: qsTr("reduced motion stops anything from constantly animating on its own, interactions still animate tho. ")
                        + qsTr("performance mode goes way further, NOTHING animates! drastically improves performance but looks ugly imo")
                }
            }

            ContentSection {
                title: qsTr("Shapes")
                symbol: "category"

                ConfigSwitch {
                    text: qsTr("No shapes")
                    checked: Settings.noShapes
                    onToggledTo: (checked) => Settings.noShapes = checked
                }
                NoteText {
                    text: qsTr("less shapes if you prefer minimalistic look")
                }
            }
        }

        // the logins: who is signed in, the library's and playback's state,
        // connecting them again, and logging out
        Page {
            visible: root.page === 5


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
                title: qsTr("Account")
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
                            text: Session.libraryConnected && Session.connected ? qsTr("Library and playback connected")
                                : Session.libraryConnected ? qsTr("Library connected")
                                : Session.connected ? qsTr("Playback connected") : qsTr("Not signed in")
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
                        text: qsTr("Log Out")
                        colBackground: Appearance.colors.error
                        colBackgroundHover: Appearance.colors.errorHover
                        labelColor: Appearance.colors.onError
                        enabled: Session.libraryConnected || Session.connected
                        onClicked: logOutDialog.open()
                    }
                }
                NoteText {
                    text: qsTr("forgets both logins on this computer")
                }

                StyledDialog {
                    id: logOutDialog
                    headline: qsTr("Log Out of Spotify?")
                    symbol: "logout"
                    iconShape: "ghostish"
                    body: qsTr("swiftify forgets your library and playback logins on this computer. you can connect again any time.")

                    buttons: [
                        TextButton {
                            text: qsTr("Log Out")
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
                            text: qsTr("Cancel")
                            onClicked: logOutDialog.close()
                        }
                    ]
                }
            }

            ContentSection {
                title: qsTr("Library")
                symbol: "library_music"

                StatusRow {
                    label: qsTr("Status")
                    value: qsTranslate("SpotifySession", Session.libraryStatus)
                    good: Session.libraryConnected && !Session.libraryNeedsAuthorizationUpgrade
                }
                OutlinedTextField {
                    x: 8
                    width: Math.min(460, parent.width - 16)
                    placeholder: qsTr("Spotify Client ID")
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
                        text: Session.libraryConnected ? qsTr("Reconnect Library") : qsTr("Connect Library")
                        enabled: Session.libraryClientId.trim().length > 0 && !Session.loginInProgress
                        onClicked: Session.beginLibraryLogin()
                    }
                }
                NoteText {
                    text: Session.libraryNeedsAuthorizationUpgrade
                          ? qsTr("reconnect once to let Swiftify edit playlists and save songs")
                          : qsTr("your playlists, saved songs and search come through the Spotify app whose Client ID is above.")
                }
            }

            ContentSection {
                title: qsTr("Playback")
                symbol: "play_circle"

                StatusRow {
                    label: qsTr("Status")
                    value: Session.connected ? qsTr("Connected") : qsTr("Not connected")
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
                        text: Session.connected ? qsTr("Reconnect Playback") : qsTr("Connect Playback")
                        enabled: !Session.loginInProgress
                        onClicked: Session.beginPlaybackLogin()
                    }
                }
                NoteText {
                    text: qsTr("playing music needs a Spotify Premium login of its own, separate from the librarys")
                }
            }
        }

        Page {
            visible: root.page === 6

            ContentSection {
                title: qsTr("Updates")
                symbol: "system_update"

                ConfigSwitch {
                    text: qsTr("Check for updates automatically")
                    enabled: Updates.supported
                    checked: Updates.automatic
                    onToggledTo: (checked) => Updates.automatic = checked
                }
                ConfigSwitch {
                    objectName: "installUpdatesAutomatically"
                    text: qsTr("Install updates automatically")
                    enabled: Updates.supported && Updates.automatic
                    checked: Updates.installAutomatically
                    onToggledTo: (checked) => Updates.installAutomatically = checked
                }
                NoteText {
                    text: qsTr("Looks for new Swiftify releases on GitHub once a day. A verified update installs by itself, and Swiftify restarts, once nothing has played for a little while. Turn the second switch off to confirm each update yourself.")
                }
                NoteText {
                    objectName: "updateStatus"
                    text: qsTranslate("Updates", Updates.status)
                }
                NoteText {
                    visible: Updates.busy && Updates.progress > 0
                    text: qsTr("Downloaded %1%").arg(Math.round(Updates.progress * 100))
                }
                Flow {
                    x: 8
                    width: parent.width - 16
                    spacing: 8

                    TextButton {
                        objectName: "checkForUpdates"
                        text: qsTr("Check for updates")
                        enabled: Updates.supported && !Updates.busy
                        onClicked: Updates.check()
                    }
                    TextButton {
                        objectName: "installUpdate"
                        text: qsTr("Download and install")
                        visible: Updates.available
                        enabled: !Updates.busy
                        onClicked: updateDialog.open()
                    }
                }
                StyledDialog {
                    id: updateDialog
                    headline: qsTr("Install Swiftify %1?").arg(Updates.latestVersion)
                    symbol: "system_update"
                    body: qsTr("Swiftify will download and verify the update, stop playback, replace this portable installation and restart. Your library and settings are kept. Do not move the application during installation.")
                    buttons: [
                        TextButton {
                            text: qsTr("Install and restart")
                            onClicked: {
                                updateDialog.accept()
                                Updates.install()
                            }
                        },
                        TextButton {
                            variant: TextButton.Text
                            text: qsTr("Cancel")
                            onClicked: updateDialog.close()
                        }
                    ]
                }
            }

            ContentSection {
                title: qsTr("Debugging")
                symbol: "bug_report"

                ConfigSwitch {
                    text: qsTr("Debugging shortcuts")
                    checked: Settings.debuggingEnabled
                    onToggledTo: (checked) => Settings.debuggingEnabled = checked
                }
                NoteText {
                    text: qsTr("Adds a seconds dial to the sleep timer, so a fade can be watched without waiting out a long ass timer")
                }

                Flow {
                    x: 8
                    width: parent.width - 16
                    spacing: 8
                    topPadding: 4

                    TextButton {
                        variant: TextButton.Tonal
                        symbol: "hourglass_empty"
                        text: qsTr("Play Loading Test")
                        onClicked: Player.playLoadingTest()
                    }
                }
                NoteText {
                    text: qsTr("plays a song that never finishes loading, so the visualizers loading animation can be watched")
                }
            }

            ContentSection {
                id: about

                property bool revealed: false

                title: qsTr("About")
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
                            text: qsTr("developed by addenator, the best programmer on earth")
                            font.pixelSize: Appearance.fontSize.smaller
                            color: Appearance.colors.onSurfaceVariant
                        }
                        StyledText {
                            topPadding: 8
                            visible: opacity > 0
                            opacity: about.revealed ? 1 : 0
                            scale: about.revealed ? 1 : 0.92
                            transformOrigin: Item.Left
                            text: qsTr("no secret dev options here...")
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
