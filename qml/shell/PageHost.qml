pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// the pages, one for each step of the library's back history
// (Library.historyDepth). going on keeps the page left behind, hidden and
// just as it was, so back shows it again where it was scrolled to, and a
// cover that flew from one of its cards to the next page's hero flies home
// to that card. only the last few are kept built; an older one is built
// afresh when back reaches it. each page fades in as it rises a little
// (material's fade-through, without holding the old one).
Item {
    id: root

    /// pages kept built behind the one showing. each holds its covers
    /// decoded and its rows built, so a few is what back usually reaches
    /// without keeping a whole browsing session in memory
    readonly property int keptPages: 1
    /// a PageSlot for each step, the one showing last
    property list<PageSlot> slots
    readonly property Item currentPage: slots.length > 0 ? slots[slots.length - 1].view : null

    function componentFor(page: int): Component {
        if (UiState.needsSetup) {
            return setupPage
        }
        switch (page) {
        case Library.Page.Search: return searchPage
        case Library.Page.Songs: return savedPage
        case Library.Page.Playlists: return playlistsPage
        case Library.Page.Personalized: return personalizedPage
        case Library.Page.PinnedPlaylists: return pinnedPage
        case Library.Page.Playlist: return playlistPage
        case Library.Page.Album: return albumPage
        case Library.Page.Artist: return artistPage
        case Library.Page.LocalFiles: return localFilesPage
        case Library.Page.LocalTracks: return localTracksPage
        case Library.Page.Stats: return statsPage
        default: return homePage
        }
    }

    function showing(): PageSlot {
        return slots.length > 0 ? slots[slots.length - 1] : null
    }

    function withoutLast(): list<PageSlot> {
        const kept = []
        for (let index = 0; index < slots.length - 1; ++index) {
            kept.push(slots[index])
        }
        return kept
    }

    function slotFor(page: Component, built: bool): PageSlot {
        const slot = slotComponent.createObject(root, { page: page, built: built }) as PageSlot
        slot.visible = built
        return slot
    }

    function enter(slot: PageSlot) {
        enterMotion.stop()
        fade.target = slot
        rise.target = slot
        enterMotion.start()
    }

    /// matches the stack to the history after the destination changes
    function sync() {
        const steps = Library.historyDepth + 1
        if (slots.length > 0 && steps === slots.length + 1) {
            push()
        } else if (slots.length > 1 && steps === slots.length - 1) {
            pop()
        } else {
            rebuild(steps)
        }
    }

    function push() {
        const behind = showing()
        // the cover clicked to come here, to fly back to on the way back
        if (UiState.heroArmed && UiState.heroCover !== null) {
            behind.returnCover = UiState.heroCover
            behind.returnSource = UiState.heroSource
            behind.returnCircular = UiState.heroCircular
        } else {
            behind.returnCover = null
        }
        behind.visible = false
        const slot = slotFor(componentFor(Library.page), true)
        const grown = withoutLast()
        grown.push(behind)
        grown.push(slot)
        slots = grown
        if (slots.length > keptPages + 1) {
            slots[slots.length - keptPages - 2].built = false
            App.trimMemory()
        }
        enter(slot)
    }

    function pop() {
        const leaving = showing()
        slots = withoutLast()
        const back = showing()

        // the leaving page's hero cover flies home to the card it came from,
        // if the hero is still in sight and the card still there showing
        // the same cover
        const box = leaving.view ? findHero(leaving.view, 0) : null
        const shown = box !== null ? box.mapToItem(leaving, 0, 0, box.width, box.height) : Qt.rect(0, 0, 0, 0)
        const cover = back.returnCover
        const flies = box !== null && shown.y + shown.height > 0 && shown.y < leaving.height
                      && cover !== null && back.built
                      && cover.source.toString() === back.returnSource.toString()
        const from = flies ? box.mapToItem(null, 0, 0, box.width, box.height) : Qt.rect(0, 0, 0, 0)

        leaving.visible = false
        leaving.destroy()
        App.trimMemory()
        if (!back.built) {
            back.page = componentFor(Library.page)
            back.built = true
        }
        back.visible = true
        if (flies && cover.visible) {
            UiState.launchReturn(from, cover, back.returnSource, back.returnCircular)
        }
        back.returnCover = null
        enter(back)
    }

    function rebuild(steps: int) {
        for (let index = 0; index < slots.length; ++index) {
            slots[index].destroy()
        }
        const fresh = []
        // the steps behind are built only if back reaches them
        for (let step = 0; step < steps - 1; ++step) {
            fresh.push(slotFor(null, false))
        }
        const slot = slotFor(componentFor(Library.page), true)
        fresh.push(slot)
        slots = fresh
        enter(slot)
    }

    // the hero sits near the top of its page, so the search stays shallow
    function findHero(item: Item, depth: int): Item {
        if (item.objectName === "heroArtwork") {
            return item
        }
        if (depth >= 7) {
            return null
        }
        for (let index = 0; index < item.children.length; ++index) {
            const found = findHero(item.children[index], depth + 1)
            if (found !== null) {
                return found
            }
        }
        return null
    }

    Component.onCompleted: sync()

    Connections {
        target: Library
        function onDestinationChanged() { root.sync() }
    }
    Connections {
        target: UiState
        // every page changes between setup and the library
        function onNeedsSetupChanged() { root.rebuild(Library.historyDepth + 1) }
    }

    Component {
        id: slotComponent
        PageSlot {
            width: root.width
            height: root.height
        }
    }

    ParallelAnimation {
        id: enterMotion
        NumberAnimation { id: fade; property: "opacity"; from: 0; to: 1; duration: Appearance.duration(210); easing.type: Easing.OutCubic }
        NumberAnimation {
            id: rise
            property: "y"
            from: 14
            to: 0
            duration: Appearance.motion(400)
            easing.type: Easing.BezierSpline
            easing.bezierCurve: Appearance.curves.emphasizedDecel
        }
    }

    Component { id: setupPage; SetupView {} }
    Component { id: homePage; HomeView {} }
    Component { id: searchPage; SearchView {} }
    Component { id: savedPage; SavedSongsView {} }
    Component { id: playlistsPage; PlaylistsView {} }
    Component { id: personalizedPage; PlaylistsView { personalized: true } }
    Component { id: pinnedPage; PlaylistsView { pinnedOnly: true } }
    Component { id: playlistPage; PlaylistView {} }
    Component { id: albumPage; AlbumView {} }
    Component { id: artistPage; ArtistView {} }
    Component { id: localFilesPage; LocalFilesView {} }
    Component { id: localTracksPage; LocalTracksView {} }
    Component { id: statsPage; StatsView {} }
}
