#include "stats/StatsStore.h"

#include "app/Paths.h"
#include "app/Settings.h"
#include "spotify/SpotifySession.h"
#include "stats/PlayHistory.h"
#include "stats/StatsAggregator.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

Q_DECLARE_LOGGING_CATEGORY(lcStats)

namespace swiftify {

namespace {

StatsStore *s_instance = nullptr;

const QString kStatsFm = QStringLiteral("statsfm");
const QString kLocal = QStringLiteral("local");

/// artists asked about at a time for their genres: the top ones are what the
/// genre list is made of
constexpr int kGenreArtists = 50;

QString genreCachePath()
{
    return paths::cacheDirectory() + QStringLiteral("/artist-genres.json");
}

QList<Song> songsOf(const QList<StatEntry> &entries)
{
    QList<Song> songs;
    songs.reserve(entries.size());
    for (const StatEntry &entry : entries) {
        songs.append(entry.song);
    }
    return songs;
}

} // namespace

StatsStore::StatsStore(Settings *settings, SpotifySession *session, QNetworkAccessManager *network,
                       PlayHistory *history, PlayTracker *tracker, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_session(session)
    , m_history(history)
    , m_tracker(tracker)
    , m_statsFm(network)
    , m_topTracks(new StatsListModel(this))
    , m_topArtists(new StatsListModel(this))
    , m_topAlbums(new StatsListModel(this))
    , m_topGenres(new StatsListModel(this))
    , m_recent(new StatsListModel(this))
    , m_topTrackSongs(new SongListModel(this))
    , m_recentSongs(new SongListModel(this))
{
    if (!s_instance) {
        s_instance = this;
    }
    loadGenreCache();

    m_localRefresh.setSingleShot(true);
    m_localRefresh.setInterval(800);
    connect(&m_localRefresh, &QTimer::timeout, this, [this] {
        if (!usesStatsFm()) {
            computeLocal();
        }
    });
    if (m_history) {
        connect(m_history, &PlayHistory::recorded, this, [this] {
            emit historyChanged();
            m_localRefresh.start();
        });
    }
    if (m_settings) {
        connect(m_settings, &Settings::languageChanged, this, [this] {
            m_calendarWeeks = calendarFor(m_snapshot.daily, QDate::currentDate(), &m_calendarMonths,
                                         &m_calendarMax, &m_calendarTotal);
            emit snapshotChanged();
        });
        connect(m_settings, &Settings::statsSourceChanged, this, [this] {
            emit sourceChanged();
            refresh();
        });
        connect(m_settings, &Settings::statsFmUsernameChanged, this, [this] {
            emit sourceChanged();
            if (usesStatsFm()) refresh();
        });
        connect(m_settings, &Settings::trackListeningChanged, this, [this] {
            if (m_tracker) m_tracker->setEnabled(m_settings->trackListening());
            emit trackingChanged();
        });
        if (m_tracker) {
            m_tracker->setEnabled(m_settings->trackListening());
        }
    }
    if (m_session) {
        // the genres of the local stats come through the library login
        connect(m_session, &SpotifySession::libraryConnectedChanged, this, [this] {
            if (!usesStatsFm() && m_session->isLibraryConnected()) {
                m_genresAsked.clear();
                computeLocal();
            }
        });
    }
    refresh();
}

StatsStore::~StatsStore()
{
    m_load.cancel();
    m_genreLoad.cancel();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

StatsStore *StatsStore::instance()
{
    return s_instance;
}

StatsStore *StatsStore::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString StatsStore::source() const
{
    return m_settings ? m_settings->statsSource() : kLocal;
}

void StatsStore::setSource(const QString &source)
{
    if (m_settings) m_settings->setStatsSource(source);
}

QString StatsStore::username() const
{
    return m_settings ? m_settings->statsFmUsername() : QString();
}

void StatsStore::setUsername(const QString &username)
{
    if (m_settings) m_settings->setStatsFmUsername(StatsFmClient::normalizeUsername(username));
}

bool StatsStore::tracking() const
{
    return m_settings ? m_settings->trackListening() : true;
}

void StatsStore::setTracking(bool tracking)
{
    if (m_settings) m_settings->setTrackListening(tracking);
}

void StatsStore::setPeriod(int period)
{
    const auto next = StatsPeriod(std::clamp(period, 0, 3));
    if (next == m_period) {
        return;
    }
    m_period = next;
    emit periodChanged();
    refresh();
}

void StatsStore::setRankByTime(bool byTime)
{
    if (m_rankByTime == byTime) {
        return;
    }
    m_rankByTime = byTime;
    emit rankByTimeChanged();
    reorder();
}

bool StatsStore::usesStatsFm() const
{
    return source() == kStatsFm;
}

int StatsStore::historyCount() const
{
    return m_history ? int(m_history->records().size()) : 0;
}

int StatsStore::peakHour() const
{
    const auto peak = std::max_element(m_snapshot.hourly.cbegin(), m_snapshot.hourly.cend());
    return *peak > 0 ? int(peak - m_snapshot.hourly.cbegin()) : -1;
}

QVariantList StatsStore::hourly() const
{
    QVariantList list;
    for (const int value : m_snapshot.hourly) list.append(value);
    return list;
}

QVariantList StatsStore::weekdays() const
{
    QVariantList list;
    for (const int value : m_snapshot.weekdays) list.append(value);
    return list;
}

void StatsStore::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        emit loadingChanged();
    }
}

void StatsStore::setError(const QString &message)
{
    if (m_errorMessage != message) {
        m_errorMessage = message;
        emit errorChanged();
    }
}

void StatsStore::refresh()
{
    m_load.cancel();
    if (usesStatsFm()) {
        if (username().isEmpty()) {
            setError({});
            setLoading(false);
            apply({});
            return;
        }
        detach(loadStatsFm(m_load.renew()));
    } else {
        setError({});
        setLoading(false);
        computeLocal();
    }
}

void StatsStore::computeLocal()
{
    const QList<PlayRecord> records = m_history ? m_history->records() : QList<PlayRecord> {};
    StatsSnapshot snapshot = StatsAggregator::aggregate(records, m_period, QDateTime::currentDateTime(), m_genres);
    // the artists' pictures, where spotify has given them
    for (StatEntry &entry : snapshot.topArtists) {
        if (!entry.artworkUrl.isValid()) {
            entry.artworkUrl = m_artistArtwork.value(entry.artist.id);
            entry.artist.artworkUrl = entry.artworkUrl;
        }
    }
    apply(snapshot);

    // the top artists spotify has not yet been asked about
    QStringList missing;
    for (const StatEntry &entry : std::as_const(snapshot.topArtists)) {
        const QString id = entry.artist.id;
        if (!id.isEmpty() && !m_genres.contains(id) && !m_genresAsked.contains(id) && missing.size() < kGenreArtists) {
            missing.append(id);
        }
    }
    if (!missing.isEmpty() && m_session && m_session->isLibraryConnected() && !m_genreLoad.isRunning()) {
        for (const QString &id : std::as_const(missing)) {
            m_genresAsked.insert(id);
        }
        detach(fillGenres(missing, m_genreLoad.renew()));
    }
}

QCoro::Task<> StatsStore::fillGenres(QStringList artistIds, CancelToken token)
{
    try {
        auto artwork = std::make_shared<QHash<QString, QUrl>>();
        const QHash<QString, QStringList> found = co_await m_session->withLibraryAccessToken<QHash<QString, QStringList>>(
            [this, artistIds, artwork](QString accessToken) {
                return m_session->webApi().artistGenres(artistIds, accessToken, artwork.get());
            });
        throwIfCancelled(token);
        for (auto it = found.cbegin(); it != found.cend(); ++it) {
            m_genres.insert(it.key(), it.value());
        }
        for (auto it = artwork->cbegin(); it != artwork->cend(); ++it) {
            m_artistArtwork.insert(it.key(), it.value());
        }
        saveGenreCache();
        m_genreLoad.finish(token);
        if (!usesStatsFm()) {
            computeLocal();
        }
    } catch (const Cancelled &) {
    } catch (const SpotifyError &error) {
        // the genres stay empty; the rest of the stats do not need them
        qCWarning(lcStats).noquote() << "artist genres:" << error.message();
        m_genreLoad.finish(token);
    }
}

QCoro::Task<> StatsStore::loadStatsFm(CancelToken token)
{
    const QString name = username();
    setLoading(true);
    setError({});
    try {
        if (m_profileFor != name) {
            const StatsFmProfile profile = co_await m_statsFm.profile(name);
            throwIfCancelled(token);
            m_profile = profile;
            m_profileFor = name;
            emit profileChanged();
        }
        const StatsSnapshot snapshot = co_await m_statsFm.snapshot(name, m_period, m_profile.isPlus);
        throwIfCancelled(token);
        apply(snapshot);
        setLoading(false);
        m_load.finish(token);
    } catch (const Cancelled &) {
    } catch (const StatsFmError &error) {
        if (isCancelled(token)) {
            co_return;
        }
        if (error.kind() == StatsFmError::Kind::NotFound) {
            m_profile = {};
            m_profileFor.clear();
            emit profileChanged();
        }
        apply({});
        setError(error.message());
        setLoading(false);
        m_load.finish(token);
    }
}

void StatsStore::apply(const StatsSnapshot &snapshot)
{
    m_snapshot = snapshot;
    m_calendarWeeks = calendarFor(snapshot.daily, QDate::currentDate(), &m_calendarMonths, &m_calendarMax, &m_calendarTotal);
    reorder();
}

void StatsStore::reorder()
{
    m_topTracks->setEntries(m_snapshot.topTracks, m_rankByTime);
    m_topArtists->setEntries(m_snapshot.topArtists, m_rankByTime);
    m_topAlbums->setEntries(m_snapshot.topAlbums, m_rankByTime);
    m_topGenres->setEntries(m_snapshot.topGenres, m_rankByTime);
    m_recent->setEntries(m_snapshot.recent, false);
    m_topTrackSongs->setSongs(songsOf(m_topTracks->entries()));
    m_recentSongs->setSongs(songsOf(m_snapshot.recent));
    emit snapshotChanged();
}

QVariantList StatsStore::calendarFor(const QMap<QDate, int> &daily, const QDate &today, QVariantList *months,
                                     int *maximum, int *total)
{
    constexpr int kWeeks = 53;
    // the column of the week holding today is the last one; each starts on a monday
    const QDate lastMonday = today.addDays(1 - today.dayOfWeek());
    const QDate first = lastMonday.addDays(-7 * (kWeeks - 1));
    QVariantList weeks;
    if (months) months->clear();
    int most = 0;
    int sum = 0;
    int shownMonth = -1;
    const QLocale locale;
    for (int week = 0; week < kWeeks; ++week) {
        QVariantList days;
        const QDate monday = first.addDays(7 * week);
        for (int day = 0; day < 7; ++day) {
            const QDate date = monday.addDays(day);
            if (date > today) {
                days.append(-1);
                continue;
            }
            const int count = daily.value(date);
            most = std::max(most, count);
            sum += count;
            days.append(count);
            // a month is named over the first week that starts in it
            if (day == 0 && months && date.month() != shownMonth) {
                shownMonth = date.month();
                if (week > 0 || date.day() <= 7) {
                    months->append(QVariantMap {{QStringLiteral("week"), week},
                                                {QStringLiteral("label"), locale.monthName(date.month(), QLocale::ShortFormat)}});
                }
            }
        }
        weeks.append(QVariant(days));
    }
    if (maximum) *maximum = most;
    if (total) *total = sum;
    return weeks;
}

QString StatsStore::exportHistory() const
{
    if (!m_history || m_history->records().isEmpty()) {
        return {};
    }
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    const QString path = folder + QStringLiteral("/swiftify-listening-history-%1.csv")
                                      .arg(QDate::currentDate().toString(Qt::ISODate));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(m_history->toCsv()) < 0 || !file.commit()) {
        qCWarning(lcStats).noquote() << "could not export the listening history to" << path << ':' << file.errorString();
        return {};
    }
    return QDir::toNativeSeparators(path);
}

void StatsStore::loadGenreCache()
{
    QFile file(genreCachePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject artists = root.value(QStringLiteral("artists")).toObject();
    for (auto it = artists.begin(); it != artists.end(); ++it) {
        const QJsonObject artist = it.value().toObject();
        QStringList genres;
        for (const QJsonValue &genre : artist.value(QStringLiteral("genres")).toArray()) {
            genres.append(genre.toString());
        }
        m_genres.insert(it.key(), genres);
        if (const QUrl image(artist.value(QStringLiteral("image")).toString()); image.isValid()) {
            m_artistArtwork.insert(it.key(), image);
        }
    }
}

void StatsStore::saveGenreCache() const
{
    QJsonObject artists;
    for (auto it = m_genres.cbegin(); it != m_genres.cend(); ++it) {
        QJsonObject artist {{QStringLiteral("genres"), QJsonArray::fromStringList(it.value())}};
        if (const QUrl image = m_artistArtwork.value(it.key()); image.isValid()) {
            artist.insert(QStringLiteral("image"), image.toString());
        }
        artists.insert(it.key(), artist);
    }
    QSaveFile file(genreCachePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(QJsonObject {{QStringLiteral("artists"), artists}}).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

} // namespace swiftify
