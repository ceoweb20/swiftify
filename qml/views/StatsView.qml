pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Swiftify

// what the person has been listening to (listening-stats for spicetify):
// the figures of a period, the top songs, artists, albums and genres, when
// in the day and the week the listening happens, and a year of it on a
// calendar. from stats.fm, or from the plays kept on this computer
Flickable {
    id: root

    topMargin: Appearance.sizes.topBarHeight

    readonly property real margin: Appearance.sizes.pageMargin
    readonly property real innerWidth: width - 2 * margin
    readonly property bool usesStatsFm: Stats.source === "statsfm"
    readonly property bool needsUsername: usesStatsFm && Stats.username.length === 0
    readonly property bool wide: innerWidth >= 860
    property bool allTracks: false
    readonly property list<string> periodNames: [qsTr("today"), qsTr("the last 4 weeks"), qsTr("the last 6 months"), qsTr("all time")]
    readonly property list<string> weekdayNames: {
        const names = []
        for (let day = 1; day <= 7; ++day) {
            names.push(Format.locale.dayName(day % 7, Locale.ShortFormat))
        }
        return names
    }
    readonly property list<string> hourNames: {
        const names = []
        for (let hour = 0; hour < 24; ++hour) {
            names.push(root.hourName(hour))
        }
        return names
    }

    contentHeight: column.implicitHeight + 24 + Appearance.sizes.playerBarClearance
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: StyledScrollBar { topPadding: 3 + Appearance.sizes.topBarHeight }

    function reaches(top: real, height: real, slack: real): bool {
        return top + height > root.contentY - slack && top < root.contentY + root.height + slack
    }
    function hourName(hour: int): string {
        return new Date(2000, 0, 1, hour).toLocaleTimeString(Format.locale, "HH:mm")
    }
    /// "1,284 plays", "3 hr 5 min" when ranking by time, or "" when the
    /// source left both out
    function amount(plays: int, listenedMs: real): string {
        if (Stats.rankByTime && listenedMs > 0) {
            return Format.totalDuration(listenedMs)
        }
        if (plays >= 0) {
            return (plays === 1 ? qsTr("%n play", "", plays) : qsTr("%n plays", "", plays))
        }
        return listenedMs > 0 ? Format.totalDuration(listenedMs) : ""
    }
    /// "1,284"
    function figure(value: real): string {
        return Math.round(value).toLocaleString(Format.locale, "f", 0)
    }
    /// listening time as the overview says it: hours past a few, else minutes
    function listeningTime(milliseconds: real): string {
        const minutes = Math.round(milliseconds / 60000)
        if (minutes >= 600) {
            return figure(minutes / 60) + qsTr(" hr")
        }
        return minutes >= 60 ? Format.totalDuration(milliseconds) : figure(minutes) + qsTr(" min")
    }
    /// "5m", "3h", "2d" ago
    function ago(when: date): string {
        const seconds = Math.max(0, (Date.now() - when.getTime()) / 1000)
        if (seconds < 60) return qsTr("now")
        if (seconds < 3600) return qsTr("%1m").arg(Math.floor(seconds / 60))
        if (seconds < 86400) return qsTr("%1h").arg(Math.floor(seconds / 3600))
        return qsTr("%1d").arg(Math.floor(seconds / 86400))
    }
    function openGenre(name: string) {
        Library.searchQuery = name
        Library.showSearch()
    }

    // a part of the page in a card of its own, a heading over what it holds
    component Card: Rectangle {
        id: card

        property string title
        property string symbol
        property string iconShape: "cookie9"
        property string note
        default property alias content: body.data

        width: parent ? parent.width : 0
        implicitHeight: body.y + body.implicitHeight + 20
        radius: 24
        color: Appearance.colors.surfaceContainerLow
        border.width: 0.75
        border.color: Qt.rgba(1, 1, 1, 0.07)

        Row {
            id: heading
            x: 20
            y: 18
            spacing: 12

            ShapeIcon {
                anchors.verticalCenter: parent.verticalCenter
                size: 32
                iconSize: 18
                shape: card.iconShape
                symbol: card.symbol
            }
            StyledText {
                anchors.verticalCenter: parent.verticalCenter
                text: card.title
                font.pixelSize: Appearance.fontSize.larger
                font.weight: Font.Bold
            }
        }
        StyledText {
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: heading.verticalCenter
            width: Math.min(implicitWidth, card.width - heading.width - 60)
            text: card.note
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignRight
            font.pixelSize: Appearance.fontSize.smaller
            color: Appearance.colors.onSurfaceVariant
        }
        Column {
            id: body
            x: 20
            y: heading.y + heading.height + 16
            width: card.width - 40
            spacing: 4
        }
    }

    Stickers {
        width: root.width
        height: 260
        key: "stats"
        count: 5
    }

    Column {
        id: column
        y: 24
        width: root.width
        spacing: 24

        // --- the heading and what it shows ------------------------------------------
        Column {
            x: root.margin
            width: root.innerWidth
            spacing: 6

            TitleText {
                text: qsTr("Stats")
                font.pixelSize: Appearance.fontSize.display
                font.weight: Font.Bold
                font.letterSpacing: -0.8
            }
            Row {
                spacing: 10

                ShapeIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    size: 30
                    iconSize: 17
                    shape: root.usesStatsFm ? "softBurst" : "clamShell"
                    symbol: root.usesStatsFm ? "query_stats" : "computer"
                    color: Appearance.colors.tertiaryContainer
                    iconColor: Appearance.colors.onTertiaryContainer
                }
                StyledText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.usesStatsFm
                          ? (Stats.profileName.length > 0 ? qsTr("%1 on stats.fm").arg(Stats.profileName) : qsTr("From stats.fm"))
                            + (Stats.isPlus ? " · Plus" : "")
                          : qsTr("Played in Swiftify") + " · " + (Stats.historyCount === 1 ? qsTr("%n play", "", Stats.historyCount) : qsTr("%n plays", "", Stats.historyCount))
                            + (Stats.tracking ? "" : " · " + qsTr("tracking paused"))
                    color: Appearance.colors.onSurfaceVariant
                }
            }
        }

        Flow {
            x: root.margin
            width: root.innerWidth
            spacing: 12

            SegmentedControl {
                currentIndex: root.usesStatsFm ? 0 : 1
                segments: [
                    Segment { text: "stats.fm"; symbol: "query_stats" },
                    Segment { text: qsTr("This computer"); symbol: "computer" }
                ]
                onActivated: (index) => Stats.source = index === 0 ? "statsfm" : "local"
            }
            SegmentedControl {
                currentIndex: Stats.period
                segments: [
                    Segment { text: qsTr("Today") },
                    Segment { text: qsTr("4 Weeks") },
                    Segment { text: qsTr("6 Months") },
                    Segment { text: qsTr("All Time") }
                ]
                onActivated: (index) => Stats.period = index
            }
            SegmentedControl {
                currentIndex: Stats.rankByTime ? 1 : 0
                segments: [
                    Segment { symbol: "leaderboard"; tooltip: qsTr("Rank by plays") },
                    Segment { symbol: "schedule"; tooltip: qsTr("Rank by listening time") }
                ]
                onActivated: (index) => Stats.rankByTime = index === 1
            }
            IconButton {
                symbol: "refresh"
                tooltip: qsTr("Refresh")
                enabled: !Stats.loading && !root.needsUsername
                onClicked: Stats.refresh()
            }
            IconButton {
                visible: !root.usesStatsFm && Stats.historyCount > 0
                symbol: "download"
                tooltip: qsTr("Export listening history as CSV")
                onClicked: {
                    const path = Stats.exportHistory()
                    UiState.toastRequested(path.length > 0 ? qsTr("Saved to %1").arg(path) : qsTr("Could not export the history"))
                }
            }
        }

        // --- connecting stats.fm ----------------------------------------------------------
        Card {
            x: root.margin
            width: Math.min(root.innerWidth, 640)
            visible: root.usesStatsFm && (root.needsUsername || Stats.errorMessage.length > 0)
            title: root.needsUsername ? qsTr("Connect stats.fm") : "stats.fm"
            symbol: "link"
            iconShape: "clover4"

            StyledText {
                width: parent.width
                visible: Stats.errorMessage.length > 0
                text: qsTranslate("Stats", Stats.errorMessage)
                wrapMode: Text.Wrap
                color: Appearance.colors.error
            }
            StyledText {
                width: parent.width
                text: qsTr("Type your stats.fm username, or paste your profile link. Your profile's stats have to be public.")
                wrapMode: Text.Wrap
                color: Appearance.colors.onSurfaceVariant
            }
            Item { width: 1; height: 6 }
            Row {
                width: parent.width
                spacing: 10

                OutlinedTextField {
                    id: usernameField
                    width: parent.width - connectButton.width - 10
                    placeholder: qsTr("stats.fm username")
                    text: Stats.username
                    onAccepted: Stats.username = text
                }
                TextButton {
                    id: connectButton
                    anchors.verticalCenter: parent.verticalCenter
                    variant: TextButton.Tonal
                    symbol: "query_stats"
                    text: qsTr("Show Stats")
                    enabled: usernameField.text.trim().length > 0
                    onClicked: {
                        Stats.username = usernameField.text
                        if (Stats.username === usernameField.text.trim()) {
                            Stats.refresh()
                        }
                    }
                }
            }
            StyledText {
                width: parent.width
                topPadding: 4
                text: qsTr("stats.fm sees what Spotify records as played. If songs played in Swiftify do not show up there, the stats from this computer still count them.")
                wrapMode: Text.Wrap
                font.pixelSize: Appearance.fontSize.smaller
                color: Appearance.colors.outline
            }
        }

        Item {
            width: root.width
            height: 200
            visible: Stats.loading && !Stats.hasData

            Column {
                anchors.centerIn: parent
                spacing: 12
                LoadingSpinner { anchors.horizontalCenter: parent.horizontalCenter }
                StyledText {
                    text: qsTr("Counting your plays…")
                    color: Appearance.colors.onSurfaceVariant
                }
            }
        }

        EmptyState {
            x: root.margin
            width: root.innerWidth
            visible: !Stats.loading && !Stats.hasData && !root.needsUsername && Stats.errorMessage.length === 0
            symbol: "insights"
            title: root.usesStatsFm ? qsTr("Nothing for %1").arg(root.periodNames[Stats.period]) : qsTr("No plays yet")
            description: root.usesStatsFm
                ? qsTr("stats.fm has no plays for this period. Try a longer one.")
                : Stats.tracking
                  ? (Stats.historyCount > 0
                     ? qsTr("Nothing played %1. Try a longer period.").arg(root.periodNames[Stats.period])
                     : qsTr("Songs you listen to for 30 seconds or more show up here."))
                  : qsTr("Listening tracking is off. Turn it on in Settings to collect stats.")
        }

        // --- the overview -------------------------------------------------------------------
        Flow {
            id: overview
            x: root.margin
            width: root.innerWidth
            visible: Stats.hasData
            spacing: 12

            readonly property int columns: width >= 900 ? 6 : width >= 560 ? 3 : 2
            readonly property real tileWidth: (width - spacing * (columns - 1)) / columns

            StatTile {
                width: overview.tileWidth
                accent: true
                value: root.listeningTime(Stats.listenedMs)
                label: qsTr("Listening time")
                symbol: "headphones"
                iconShape: "sunny"
            }
            StatTile {
                width: overview.tileWidth
                value: root.figure(Stats.plays)
                label: qsTr("Plays")
                symbol: "play_arrow"
                iconShape: "cookie9"
            }
            StatTile {
                width: overview.tileWidth
                value: root.figure(Stats.uniqueTracks)
                label: qsTr("Songs")
                symbol: "music_note"
                iconShape: "heart"
            }
            StatTile {
                width: overview.tileWidth
                value: root.figure(Stats.uniqueArtists)
                label: qsTr("Artists")
                symbol: "person"
                iconShape: "cookie12"
            }
            StatTile {
                width: overview.tileWidth
                value: Stats.uniqueAlbums > 0 ? root.figure(Stats.uniqueAlbums) : root.figure(Stats.activeDays)
                label: Stats.uniqueAlbums > 0 ? qsTr("Albums") : qsTr("Days listened")
                symbol: Stats.uniqueAlbums > 0 ? "album" : "today"
                iconShape: "flower"
            }
            StatTile {
                width: overview.tileWidth
                value: Stats.streakDays === 1 ? qsTr("%n day", "", Stats.streakDays) : qsTr("%n days", "", Stats.streakDays)
                label: qsTr("Streak")
                symbol: "local_fire_department"
                iconShape: "burst"
            }
        }

        // --- top songs ------------------------------------------------------------------------
        Card {
            x: root.margin
            width: root.innerWidth
            visible: Stats.topTracks.count > 0
            title: qsTr("Top Songs")
            symbol: "music_note"
            iconShape: "heart"
            note: qsTr("Top %1 · %2").arg(Math.min(Stats.topTracks.count, 50)).arg(root.periodNames[Stats.period])

            Repeater {
                model: Stats.topTracks

                RankRow {
                    required property int index
                    required property var model

                    width: parent.width
                    visible: root.allTracks || index < 10
                    height: visible ? implicitHeight : 0
                    rank: model.rank
                    name: model.name
                    subtitle: model.subtitle
                    artworkUrl: model.artworkUrl
                    plays: model.plays
                    listenedMs: model.listenedMs
                    share: model.share
                    current: Player.hasSong && Player.currentSong.uri === model.song.uri
                    timeFirst: Stats.rankByTime
                    onClicked: Player.playFromModel(Stats.topTrackSongs, index)
                    onSecondaryClicked: SongMenus.open(model.song, false)
                }
            }
            TextButton {
                visible: Stats.topTracks.count > 10
                variant: TextButton.Text
                symbol: root.allTracks ? "expand_less" : "expand_more"
                text: root.allTracks ? qsTr("Show fewer") : qsTr("Show all %1").arg(Stats.topTracks.count)
                onClicked: root.allTracks = !root.allTracks
            }
        }

        // --- top artists and albums -----------------------------------------------------------
        Shelf {
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            visible: Stats.topArtists.count > 0
            title: qsTr("Top Artists")
            symbol: "person"
            iconShape: "cookie12"
            model: Stats.topArtists
            delegate: MediaCard {
                required property var model

                artworkUrl: model.artworkUrl
                placeholderSymbol: "person"
                title: model.rank + ". " + model.name
                subtitle: root.amount(model.plays, model.listenedMs)
                circular: true
                titleLines: 1
                badgeSymbol: (model.artist?.id ?? "").length > 0 ? "chevron_right" : "person"
                onClicked: {
                    if ((model.artist?.id ?? "").length > 0) {
                        Library.selectArtist(model.artist)
                    }
                }
            }
        }

        Shelf {
            width: root.width
            onScreen: root.reaches(column.y + y, height, 0)
            nearScreen: root.reaches(column.y + y, height, root.height / 2)
            visible: Stats.topAlbums.count > 0
            title: qsTr("Top Albums")
            symbol: "album"
            iconShape: "sunny"
            model: Stats.topAlbums
            delegate: MediaCard {
                required property var model

                artworkUrl: model.artworkUrl
                placeholderSymbol: "album"
                title: model.rank + ". " + model.name
                subtitle: [root.amount(model.plays, model.listenedMs), model.subtitle ?? ""]
                          .filter(part => part.length > 0).join(" · ")
                // a reused card can be asked about its row while the list resets
                badgeSymbol: (model.album?.id ?? "").length > 0 ? "chevron_right" : "album"
                onClicked: {
                    if ((model.album?.id ?? "").length > 0) {
                        Library.selectAlbum(model.album)
                    }
                }
            }
        }

        // --- genres ----------------------------------------------------------------------------
        Card {
            x: root.margin
            width: root.innerWidth
            visible: Stats.topGenres.count > 0 || (!root.usesStatsFm && Stats.hasData)
            title: qsTr("Top Genres")
            symbol: "sell"
            iconShape: "clover8"
            note: !root.usesStatsFm && Stats.topGenres.count === 0
                  ? (Session.libraryConnected ? qsTr("Looking up your artists…") : qsTr("Connect your library to see genres")) : ""

            Flow {
                width: parent.width
                spacing: 8

                Repeater {
                    model: Stats.topGenres

                    Rectangle {
                        id: chip

                        required property int index
                        required property string name
                        required property int plays
                        required property real share

                        visible: index < 24
                        width: chipText.implicitWidth + 28
                        height: 36 + Math.round(8 * share)
                        radius: height / 2
                        color: Qt.alpha(Appearance.colors.primary, 0.08 + 0.32 * share)
                        border.width: index < 3 ? 1 : 0
                        border.color: Qt.alpha(Appearance.colors.primary, 0.5)

                        StyledText {
                            id: chipText
                            anchors.centerIn: parent
                            text: chip.name
                            font.pixelSize: chip.index < 3 ? Appearance.fontSize.normal : Appearance.fontSize.smallie
                            font.weight: chip.index < 3 ? Font.Bold : Font.Medium
                            color: Appearance.colors.onSurface
                        }
                        HoverHandler {
                            id: chipHover
                            cursorShape: Qt.PointingHandCursor
                        }
                        Loader {
                            active: chipHover.hovered
                            sourceComponent: StyledToolTip {
                                visible: true
                                text: (chip.plays > 0 ? (chip.plays === 1 ? qsTr("%n play", "", chip.plays) : qsTr("%n plays", "", chip.plays)) + " · " : "") + qsTr("search for it")
                            }
                        }
                        TapHandler {
                            onTapped: root.openGenre(chip.name)
                        }
                    }
                }
            }
        }

        // --- when the listening happens ---------------------------------------------------
        Flow {
            id: activity
            x: root.margin
            width: root.innerWidth
            visible: Stats.hasData
            spacing: 16

            readonly property real cardWidth: root.wide ? (width - spacing) / 2 : width

            Card {
                width: activity.cardWidth
                title: qsTr("By Hour")
                symbol: "schedule"
                iconShape: "cookie6"
                note: Stats.peakHour >= 0 ? qsTr("Most active around %1").arg(root.hourName(Stats.peakHour)) : ""

                BarChart {
                    width: parent.width
                    values: Stats.hourly
                    labels: root.hourNames
                    labelEvery: 6
                    unit: qsTr("plays")
                }
            }
            Card {
                width: activity.cardWidth
                title: qsTr("By Weekday")
                symbol: "calendar_month"
                iconShape: "pentagon"
                note: {
                    let best = -1
                    let most = 0
                    for (let day = 0; day < Stats.weekdays.length; ++day) {
                        if (Stats.weekdays[day] > most) {
                            most = Stats.weekdays[day]
                            best = day
                        }
                    }
                    return best >= 0 ? qsTr("Busiest day: %1").arg(Format.locale.dayName((best + 1) % 7, Locale.LongFormat)) : ""
                }

                BarChart {
                    width: parent.width
                    values: Stats.weekdays
                    labels: root.weekdayNames
                    unit: qsTr("plays")
                }
            }
        }

        // --- the year ------------------------------------------------------------------------
        Card {
            x: root.margin
            width: root.innerWidth
            visible: Stats.calendarTotal > 0
            title: qsTr("Listening Calendar")
            symbol: "calendar_month"
            iconShape: "squircle"
            note: qsTr("%n plays in the last year", "", Stats.calendarTotal)

            CalendarHeatmap {
                width: parent.width
                weeks: Stats.calendarWeeks
                months: Stats.calendarMonths
                maximum: Stats.calendarMax
            }
        }

        // --- recently --------------------------------------------------------------------------
        Card {
            x: root.margin
            width: root.innerWidth
            visible: Stats.recent.count > 0
            title: qsTr("Recently Played")
            symbol: "history"
            iconShape: "clamShell"

            Repeater {
                model: Stats.recent

                RankRow {
                    required property int index
                    required property var model

                    width: parent.width
                    rank: index + 1
                    rankLabel: root.ago(model.playedAt)
                    name: model.name
                    subtitle: model.subtitle
                    artworkUrl: model.artworkUrl
                    detail: Format.playbackTime(model.listenedMs)
                    current: Player.hasSong && Player.currentSong.uri === model.song.uri
                    onClicked: Player.playFromModel(Stats.recentSongs, index)
                    onSecondaryClicked: SongMenus.open(model.song, false)
                }
            }
        }
    }
}
