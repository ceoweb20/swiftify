#pragma once

#include "app/Paths.h"
#include "core/Async.h"
#include "library/CollectionModels.h"
#include "library/SongListCache.h"
#include "library/SongListModel.h"
#include "spotify/SpotifyTypes.h"

#include <QHash>
#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QtQml/qqmlregistration.h>

#include <optional>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

class ErrorCenter;
class LocalLibrary;
class PlaybackController;
class Settings;
class SpotifySession;

/// the last few things of a kind that were opened, keyed by id. bounded on
/// purpose: a playlist can run to thousands of songs. eviction is by insertion
/// order, since the point is to make going back cheap.
template <typename Value>
class RecentlyOpened {
public:
    explicit RecentlyOpened(int limit)
        : m_limit(std::max(1, limit))
    {
    }

    const Value *find(const QString &id) const
    {
        const auto found = m_values.constFind(id);
        return found == m_values.cend() ? nullptr : &found.value();
    }

    void insert(const QString &id, const Value &value)
    {
        m_order.removeAll(id);
        m_values.insert(id, value);
        m_order.append(id);
        while (m_order.size() > m_limit) {
            m_values.remove(m_order.takeFirst());
        }
    }

    void remove(const QString &id)
    {
        m_order.removeAll(id);
        m_values.remove(id);
    }

    void clear()
    {
        m_order.clear();
        m_values.clear();
    }

private:
    int m_limit;
    QStringList m_order;
    QHash<QString, Value> m_values;
};

/// what the user has in spotify and where they are in it: playlists, saved
/// songs, recent plays, search, and the page on screen. the library half of
/// PlayerModel.swift, plus the back history ContentView.swift keeps.
class LibraryStore : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Library)
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    QML_SINGLETON

    Q_PROPERTY(Page page READ page NOTIFY destinationChanged FINAL)
    Q_PROPERTY(QString selectedPlaylistId READ selectedPlaylistId NOTIFY destinationChanged FINAL)
    Q_PROPERTY(swiftify::Playlist selectedPlaylist READ selectedPlaylist NOTIFY selectedPlaylistChanged FINAL)
    Q_PROPERTY(bool selectedPlaylistEditable READ isSelectedPlaylistEditable NOTIFY selectedPlaylistChanged FINAL)
    Q_PROPERTY(swiftify::Album currentAlbum READ currentAlbum NOTIFY destinationChanged FINAL)
    Q_PROPERTY(swiftify::Artist currentArtist READ currentArtist NOTIFY destinationChanged FINAL)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY destinationChanged FINAL)
    /// how many destinations back goes through; the page host keeps one
    /// page for each, so going back shows the page as it was left
    Q_PROPERTY(int historyDepth READ historyDepth NOTIFY destinationChanged FINAL)

    Q_PROPERTY(QString profileId READ profileId NOTIFY profileChanged FINAL)
    Q_PROPERTY(QString profileName READ profileName NOTIFY profileChanged FINAL)
    Q_PROPERTY(QUrl profileArtworkUrl READ profileArtworkUrl NOTIFY profileChanged FINAL)

    Q_PROPERTY(swiftify::PlaylistListModel *playlists READ playlists CONSTANT FINAL)
    Q_PROPERTY(swiftify::PlaylistListModel *pinnedPlaylists READ pinnedPlaylists CONSTANT FINAL)
    Q_PROPERTY(swiftify::PlaylistListModel *personalizedPlaylists READ personalizedPlaylists CONSTANT FINAL)
    Q_PROPERTY(QVariantList homeSections READ homeSections NOTIFY discoveryChanged FINAL)
    Q_PROPERTY(bool loadingDiscovery READ isLoadingDiscovery NOTIFY discoveryChanged FINAL)
    Q_PROPERTY(QString discoveryStatus READ discoveryStatus NOTIFY discoveryChanged FINAL)
    Q_PROPERTY(int pinsRevision READ pinsRevision NOTIFY pinsChanged FINAL)

    /// the open playlist as stored, and as shown (filtered and sorted)
    Q_PROPERTY(swiftify::SongListModel *playlistSongs READ playlistSongs CONSTANT FINAL)
    Q_PROPERTY(swiftify::SongListModel *displayedPlaylistSongs READ displayedPlaylistSongs CONSTANT FINAL)
    Q_PROPERTY(QString playlistFilterQuery READ playlistFilterQuery WRITE setPlaylistFilterQuery NOTIFY playlistViewChanged FINAL)
    Q_PROPERTY(SortOrder playlistSortOrder READ playlistSortOrder WRITE setPlaylistSortOrder NOTIFY playlistViewChanged FINAL)
    Q_PROPERTY(bool canReorderPlaylist READ canReorderPlaylist NOTIFY playlistViewChanged FINAL)

    Q_PROPERTY(swiftify::SongListModel *savedSongs READ savedSongs CONSTANT FINAL)
    Q_PROPERTY(swiftify::SongListModel *recentlyPlayed READ recentlyPlayed CONSTANT FINAL)
    Q_PROPERTY(swiftify::ArtistListModel *recommendedArtists READ recommendedArtists CONSTANT FINAL)
    Q_PROPERTY(swiftify::AlbumListModel *recommendedAlbums READ recommendedAlbums CONSTANT FINAL)
    Q_PROPERTY(swiftify::SongListModel *albumSongs READ albumSongs CONSTANT FINAL)
    Q_PROPERTY(swiftify::SongListModel *artistTopSongs READ artistTopSongs CONSTANT FINAL)
    Q_PROPERTY(swiftify::AlbumListModel *artistAlbums READ artistAlbums CONSTANT FINAL)

    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchChanged FINAL)
    Q_PROPERTY(bool searchIsCatchphrase READ searchIsCatchphrase NOTIFY searchChanged FINAL)
    Q_PROPERTY(swiftify::SongListModel *searchSongs READ searchSongs CONSTANT FINAL)
    Q_PROPERTY(swiftify::AlbumListModel *searchAlbums READ searchAlbums CONSTANT FINAL)
    Q_PROPERTY(swiftify::ArtistListModel *searchArtists READ searchArtists CONSTANT FINAL)
    Q_PROPERTY(swiftify::PlaylistListModel *searchPlaylists READ searchPlaylists CONSTANT FINAL)
    Q_PROPERTY(bool hasSearchResults READ hasSearchResults NOTIFY searchChanged FINAL)
    Q_PROPERTY(swiftify::SongListModel *searchHistory READ searchHistory CONSTANT FINAL)
    /// the first eight, which is what the search page shows
    Q_PROPERTY(swiftify::SongListModel *searchHistoryPreview READ searchHistoryPreview CONSTANT FINAL)

    Q_PROPERTY(int savedRevision READ savedRevision NOTIFY savedChanged FINAL)
    /// how many songs are saved, before their list has loaded too; -1 unknown
    Q_PROPERTY(int savedSongTotal READ savedSongTotal NOTIFY savedSongTotalChanged FINAL)

    Q_PROPERTY(bool loadingPlaylists READ isLoadingPlaylists NOTIFY loadingChanged FINAL)
    Q_PROPERTY(bool loadingSongs READ isLoadingSongs NOTIFY loadingChanged FINAL)
    Q_PROPERTY(bool loadingHome READ isLoadingHome NOTIFY loadingChanged FINAL)
    Q_PROPERTY(bool loadingSavedSongs READ isLoadingSavedSongs NOTIFY loadingChanged FINAL)
    Q_PROPERTY(bool searching READ isSearching NOTIFY loadingChanged FINAL)
    Q_PROPERTY(bool loadingArtist READ isLoadingArtist NOTIFY loadingChanged FINAL)
    Q_PROPERTY(bool savingPlaylist READ isSavingPlaylist NOTIFY loadingChanged FINAL)

public:
    /// LocalFiles is the grid of playlists made on this computer, opening on
    /// LocalTracks, every music file of the person's own as a list
    enum class Page { Home, Search, Songs, Playlists, PinnedPlaylists, Playlist, Album, Artist, LocalFiles, LocalTracks, Personalized };
    Q_ENUM(Page)
    enum class SortOrder { Playlist, Title, Artist, Album, Duration };
    Q_ENUM(SortOrder)

    LibraryStore(SpotifySession *session, Settings *settings, ErrorCenter *errors, QObject *parent = nullptr);
    ~LibraryStore() override;

    static LibraryStore *instance();
    static LibraryStore *create(QQmlEngine *, QJSEngine *);

    /// the player records search plays here
    void attachPlayer(PlaybackController *player);
    /// playlists made on this computer open, and are edited, like the
    /// person's own spotify playlists
    void attachLocalFiles(LocalLibrary *localFiles);

    Page page() const { return m_destination.page; }
    QString selectedPlaylistId() const { return m_destination.page == Page::Playlist ? m_destination.playlistId : QString(); }
    Playlist selectedPlaylist() const;
    bool isSelectedPlaylistEditable() const;
    Album currentAlbum() const { return m_destination.album; }
    Artist currentArtist() const { return m_destination.artist; }
    bool canGoBack() const { return !m_history.isEmpty(); }
    int historyDepth() const { return int(m_history.size()); }

    QString profileId() const { return m_profile.id; }
    QString profileName() const { return m_profile.displayName; }
    QUrl profileArtworkUrl() const { return m_profile.artworkUrl; }

    PlaylistListModel *playlists() const { return m_playlists; }
    PlaylistListModel *pinnedPlaylists() const { return m_pinnedPlaylists; }
    PlaylistListModel *personalizedPlaylists() const { return m_personalizedPlaylists; }
    QVariantList homeSections() const { return m_homeSections; }
    bool isLoadingDiscovery() const { return m_loadingDiscovery; }
    QString discoveryStatus() const { return m_discoveryStatus; }
    int pinsRevision() const { return m_pinsRevision; }
    SongListModel *playlistSongs() const { return m_playlistSongs; }
    SongListModel *displayedPlaylistSongs() const { return m_displayedPlaylistSongs; }
    QString playlistFilterQuery() const { return m_playlistFilterQuery; }
    void setPlaylistFilterQuery(const QString &query);
    SortOrder playlistSortOrder() const { return m_sortOrder; }
    void setPlaylistSortOrder(SortOrder order);
    bool canReorderPlaylist() const;

    SongListModel *savedSongs() const { return m_savedSongs; }
    SongListModel *recentlyPlayed() const { return m_recentlyPlayed; }
    ArtistListModel *recommendedArtists() const { return m_recommendedArtists; }
    AlbumListModel *recommendedAlbums() const { return m_recommendedAlbums; }
    SongListModel *albumSongs() const { return m_albumSongs; }
    SongListModel *artistTopSongs() const { return m_artistTopSongs; }
    AlbumListModel *artistAlbums() const { return m_artistAlbums; }

    QString searchQuery() const { return m_searchQuery; }
    void setSearchQuery(const QString &query);
    bool searchIsCatchphrase() const;
    SongListModel *searchSongs() const { return m_searchSongs; }
    AlbumListModel *searchAlbums() const { return m_searchAlbums; }
    ArtistListModel *searchArtists() const { return m_searchArtists; }
    PlaylistListModel *searchPlaylists() const { return m_searchPlaylists; }
    bool hasSearchResults() const;
    SongListModel *searchHistory() const { return m_searchHistory; }
    SongListModel *searchHistoryPreview() const { return m_searchHistoryPreview; }

    int savedRevision() const { return m_savedRevision; }
    int savedSongTotal() const { return m_hasLoadedSavedSongs ? m_savedSongs->count() : m_savedSongTotal; }

    bool isLoadingPlaylists() const { return m_loadingPlaylists; }
    bool isLoadingSongs() const { return m_loadingSongs; }
    bool isLoadingHome() const { return m_loadingHome; }
    bool isLoadingSavedSongs() const { return m_loadingSavedSongs; }
    bool isSearching() const { return m_searching; }
    bool isLoadingArtist() const { return m_loadingArtist; }
    bool isSavingPlaylist() const { return m_savingPlaylist; }

    QList<Playlist> editablePlaylistList() const;

    // --- navigation ---------------------------------------------------------------

    Q_INVOKABLE void showHome();
    Q_INVOKABLE void showSearch();
    Q_INVOKABLE void showSavedSongs();
    Q_INVOKABLE void showPlaylists();
    Q_INVOKABLE void showPinnedPlaylists();
    Q_INVOKABLE void showPersonalized();
    Q_INVOKABLE void refreshDiscovery();
    /// music files of the person's own (LocalLibrary): the playlists made of
    /// them, and a way to all of them
    Q_INVOKABLE void showLocalFiles();
    /// every music file of the person's own, as a list
    Q_INVOKABLE void showLocalTracks();
    Q_INVOKABLE void selectPlaylist(const swiftify::Playlist &playlist);
    Q_INVOKABLE void selectPlaylistById(const QString &id);
    Q_INVOKABLE void selectAlbum(const swiftify::Album &album);
    Q_INVOKABLE void selectArtist(const swiftify::Artist &artist);
    Q_INVOKABLE void goToAlbum(const swiftify::Song &song);
    Q_INVOKABLE void goToArtist(const swiftify::Song &song);
    Q_INVOKABLE void goBack();

    // --- library ---------------------------------------------------------------------

    Q_INVOKABLE bool isPinned(const QString &playlistId) const { return m_pinnedIds.contains(playlistId); }
    Q_INVOKABLE void togglePinned(const swiftify::Playlist &playlist);
    Q_INVOKABLE bool isSaved(const QString &uri) const { return m_savedUris.contains(uri); }
    Q_INVOKABLE void toggleSaved(const swiftify::Song &song);
    Q_INVOKABLE QVariantList editablePlaylists() const;
    Q_INVOKABLE void clearSearchHistory();
    Q_INVOKABLE void retryLoadingPlaylists();
    Q_INVOKABLE void addToPlaylist(const swiftify::Song &song, const swiftify::Playlist &playlist);
    Q_INVOKABLE void removeFromPlaylist(const swiftify::Song &song, const swiftify::Playlist &playlist);
    Q_INVOKABLE void removeFromSelectedPlaylist(const swiftify::Song &song);
    /// moves `count` rows starting at `from` to before row `to`, in the open
    /// playlist's own order. contiguous ranges only.
    Q_INVOKABLE void moveSongs(int from, int count, int to);
    Q_INVOKABLE void deletePlaylist(const swiftify::Playlist &playlist);
    /// creates a playlist or updates the open one, then uploads `coverJpeg`
    /// when one is given. reports through playlistSaveFinished.
    Q_INVOKABLE void savePlaylist(bool isNew, const QString &name, const QString &description, bool isPublic,
                                  const QByteArray &coverJpeg);
    /// creates a playlist of music files on this computer, holding
    /// `firstSong` when that is one (a song, or null), or updates the open
    /// one. reports through playlistSaveFinished.
    Q_INVOKABLE void saveLocalPlaylist(bool isNew, const QString &name, const QString &description,
                                       const QByteArray &coverJpeg, const QVariant &firstSong);

    /// what a library login runs before it counts as done
    QCoro::Task<> loadInitialLibrary();
    void recordSearchPlayback(const Song &song);

signals:
    void destinationChanged();
    void selectedPlaylistChanged();
    void profileChanged();
    void pinsChanged();
    void playlistViewChanged();
    void searchChanged();
    void savedChanged();
    void savedSongTotalChanged();
    void loadingChanged();
    void playlistSaveFinished(bool ok);
    void playlistDeleted(const QString &id);
    void discoveryChanged();

private:
    struct Destination {
        Page page = Page::Home;
        QString playlistId;
        Album album;
        Artist artist;
        /// as opened, for a playlist from search that is not in the library
        Playlist playlist;

        bool operator==(const Destination &other) const;
    };
    struct ArtistDetail {
        QList<Song> topSongs;
        QList<Album> albums;
    };

    void navigate(const Destination &destination);
    void cancelDetailLoads();
    void setLoading(bool &flag, bool value);
    void setProfile(const Profile &profile);
    void setPlaylistList(const QList<Playlist> &playlists);
    void refreshPinnedPlaylists();
    void persistPins();
    void setSongs(const QList<Song> &songs);
    void refreshDisplayedSongs();
    void refreshRecommendations();
    void persistSearchHistory();
    void setSearchHistory(const QList<Song> &songs);
    void adjustPlaylistSongCount(const QString &id, int delta);
    void reportError(const QString &context, const QString &message);
    void reset();
    QString reorderFailureMessage(const QString &message, bool forbidden, const Playlist &playlist) const;
    /// the open playlist's songs again, once a playlist made here or the
    /// files it holds changed
    void refreshLocalPlaylist();
    void removeFromLocalPlaylist(const Song &song, const Playlist &playlist);
    void deleteLocalPlaylist(const Playlist &playlist);

    QCoro::Task<> loadPlaylists();
    QCoro::Task<> loadHome();
    QCoro::Task<> loadDiscovery(CancelToken token);
    QCoro::Task<> loadSavedSongs();
    QCoro::Task<> loadSavedSongTotal();
    /// a save or removal before the list has loaded
    void adjustSavedSongTotal(int delta);
    QCoro::Task<> loadSavedStatuses(QList<Song> songs);
    QCoro::Task<> runPlaylistLoad(Playlist playlist, CancelToken token);
    QCoro::Task<> runAlbumLoad(Album album, CancelToken token);
    QCoro::Task<> runArtistLoad(Artist artist, CancelToken token);
    QCoro::Task<> runSearch(QString query, CancelToken token);
    QCoro::Task<> runToggleSaved(Song song, bool shouldSave, quint64 generation);
    QCoro::Task<> runSongNavigation(Song song, bool toAlbum);
    QCoro::Task<> runAdd(Song song, Playlist playlist);
    QCoro::Task<> runRemove(Song song, Playlist playlist);
    QCoro::Task<> runMove(Playlist playlist, int from, int count, int to, QList<Song> previous, QList<Song> reordered);
    QCoro::Task<> runDelete(Playlist playlist);
    QCoro::Task<> runSave(bool isNew, QString name, QString description, bool isPublic, QByteArray coverJpeg);
    QCoro::Task<bool> uploadCover(Playlist playlist, QByteArray jpeg);
    QCoro::Task<> refreshCover(Playlist playlist);

    SpotifySession *m_session;
    Settings *m_settings;
    ErrorCenter *m_errors;
    LocalLibrary *m_localFiles = nullptr;

    Destination m_destination;
    QList<Destination> m_history;
    std::optional<Destination> m_restoreTarget;

    Profile m_profile;
    QList<Playlist> m_playlistList;
    QSet<QString> m_pinnedIds;
    int m_pinsRevision = 0;
    PlaylistListModel *m_playlists;
    PlaylistListModel *m_pinnedPlaylists;
    PlaylistListModel *m_personalizedPlaylists;
    QList<Playlist> m_rootPlaylists;
    QSet<QString> m_removedRootIds;
    QVariantList m_homeSections;
    QString m_discoveryUserId;
    QString m_discoveryStatus;
    QDateTime m_discoveryLoadedAt;
    bool m_loadingDiscovery = false;

    QList<Song> m_songs;
    SongListModel *m_playlistSongs;
    SongListModel *m_displayedPlaylistSongs;
    QString m_playlistFilterQuery;
    SortOrder m_sortOrder = SortOrder::Playlist;

    SongListModel *m_savedSongs;
    SongListModel *m_recentlyPlayed;
    ArtistListModel *m_recommendedArtists;
    AlbumListModel *m_recommendedAlbums;
    SongListModel *m_albumSongs;
    SongListModel *m_artistTopSongs;
    AlbumListModel *m_artistAlbums;

    QString m_searchQuery;
    SongListModel *m_searchSongs;
    AlbumListModel *m_searchAlbums;
    ArtistListModel *m_searchArtists;
    PlaylistListModel *m_searchPlaylists;
    SongListModel *m_searchHistory;
    SongListModel *m_searchHistoryPreview;

    QSet<QString> m_savedUris;
    QSet<QString> m_pendingSavedUris;
    int m_savedRevision = 0;
    int m_savedStatusRevision = 0;
    /// spotify's count, kept up with saves and removals until the list loads
    int m_savedSongTotal = -1;

    bool m_hasLoadedHome = false;
    bool m_hasLoadedSavedSongs = false;
    bool m_loadingPlaylists = false;
    bool m_loadingSongs = false;
    bool m_loadingHome = false;
    bool m_loadingSavedSongs = false;
    bool m_searching = false;
    bool m_loadingArtist = false;
    bool m_savingPlaylist = false;

    RecentlyOpened<QList<Song>> m_openedPlaylists {8};
    /// the playlists' and saved songs' last loaded lists, kept across restarts
    SongListCache m_songListCache {paths::cacheDirectory() + QStringLiteral("/song-lists")};
    RecentlyOpened<QList<Song>> m_openedAlbums {8};
    RecentlyOpened<ArtistDetail> m_openedArtists {8};

    TaskSlot m_songLoad;
    TaskSlot m_artistLoad;
    TaskSlot m_search;
    TaskSlot m_discoveryLoad;
};

} // namespace swiftify
