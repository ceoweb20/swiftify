#include "stats/PlayHistory.h"

#include "app/Paths.h"
#include "playback/PlaybackController.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimeZone>

#include <algorithm>

Q_LOGGING_CATEGORY(lcStats, "swiftify.stats")

namespace swiftify {

namespace {

QByteArray csvField(const QString &value)
{
    QString escaped = value;
    escaped.replace(u'"', QStringLiteral("\"\""));
    return '"' + escaped.toUtf8() + '"';
}

} // namespace

PlayHistory::PlayHistory(QString path, QObject *parent)
    : QObject(parent)
    , m_path(std::move(path))
{
    load();
}

QString PlayHistory::defaultPath()
{
    return paths::dataDirectory() + QStringLiteral("/listening-history.jsonl");
}

bool PlayHistory::counts(const Song &song, qint64 listenedMs)
{
    if (!song.isValid() || listenedMs <= 0) {
        return false;
    }
    if (listenedMs >= kMinimumListenMs) {
        return true;
    }
    return song.durationMs > 0 && listenedMs >= qint64(song.durationMs * kMinimumListenShare);
}

bool PlayHistory::record(const Song &song, const QDateTime &startedAt, qint64 listenedMs)
{
    if (!counts(song, listenedMs)) {
        return false;
    }
    PlayRecord record {startedAt.toUTC(), listenedMs, song};
    m_records.append(record);
    append(record);
    emit recorded();
    return true;
}

void PlayHistory::load()
{
    if (m_path.isEmpty()) {
        return;
    }
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    int skipped = 0;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QJsonObject object = QJsonDocument::fromJson(line).object();
        PlayRecord record;
        record.startedAt = QDateTime::fromMSecsSinceEpoch(object.value(QStringLiteral("t")).toInteger(), QTimeZone::UTC);
        record.listenedMs = object.value(QStringLiteral("ms")).toInteger();
        record.song = Song::fromJson(object.value(QStringLiteral("song")).toObject());
        // a line cut short when the app stopped mid-write is passed over,
        // never thrown away with the file around it
        if (!record.song.isValid() || object.value(QStringLiteral("t")).toInteger() <= 0) {
            ++skipped;
            continue;
        }
        m_records.append(record);
    }
    if (skipped > 0) {
        qCWarning(lcStats) << "passed over" << skipped << "unreadable lines in" << m_path;
    }
    // appended in order as a rule; a clock set back is the exception
    std::stable_sort(m_records.begin(), m_records.end(),
                     [](const PlayRecord &a, const PlayRecord &b) { return a.startedAt < b.startedAt; });
}

void PlayHistory::append(const PlayRecord &record)
{
    if (m_path.isEmpty()) {
        return;
    }
    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qCWarning(lcStats).noquote() << "could not keep a play in" << m_path << ':' << file.errorString();
        return;
    }
    const QJsonObject object {
        {QStringLiteral("t"), record.startedAt.toMSecsSinceEpoch()},
        {QStringLiteral("ms"), record.listenedMs},
        {QStringLiteral("song"), record.song.toJson()},
    };
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
}

QByteArray PlayHistory::toCsv() const
{
    QByteArray csv = "played_at,listened_ms,track,artists,album,uri\n";
    for (const PlayRecord &record : m_records) {
        csv += record.startedAt.toString(Qt::ISODate).toUtf8() + ',' + QByteArray::number(record.listenedMs) + ','
            + csvField(record.song.name) + ',' + csvField(record.song.artists) + ',' + csvField(record.song.albumName)
            + ',' + csvField(record.song.uri) + '\n';
    }
    return csv;
}

PlayTracker::PlayTracker(PlaybackController *player, PlayHistory *history, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_history(history)
{
    if (m_player) {
        const auto follow = [this] {
            observe(m_player->hasSong() ? m_player->currentSong() : Song {}, m_player->isPlaying());
        };
        connect(m_player, &PlaybackController::currentSongChanged, this, follow);
        connect(m_player, &PlaybackController::playingChanged, this, follow);
    }
}

PlayTracker::~PlayTracker()
{
    finish();
}

void PlayTracker::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    if (!enabled) {
        // what has been heard so far still counts; nothing after it does
        finish();
    }
    m_enabled = enabled;
    if (enabled && m_player && m_player->hasSong()) {
        observe(m_player->currentSong(), m_player->isPlaying());
    }
}

void PlayTracker::pauseClock()
{
    if (m_clock.isValid()) {
        m_heardMs += m_clock.elapsed();
        m_clock.invalidate();
    }
}

void PlayTracker::observe(const Song &song, bool playing)
{
    if (!m_enabled) {
        return;
    }
    // the same occurrence keeps counting; a new one, even of the same
    // track played again, is a play of its own
    if (!song.isSameInstance(m_song) || song.uri != m_song.uri) {
        finish();
        m_song = song;
        m_startedAt = QDateTime::currentDateTimeUtc();
        m_heardMs = 0;
    }
    if (!m_song.isValid()) {
        return;
    }
    if (playing && !m_clock.isValid()) {
        m_clock.start();
    } else if (!playing) {
        pauseClock();
    }
}

void PlayTracker::finish()
{
    pauseClock();
    if (m_song.isValid() && m_history) {
        m_history->record(m_song, m_startedAt, m_heardMs);
    }
    m_song = {};
    m_heardMs = 0;
}

} // namespace swiftify
