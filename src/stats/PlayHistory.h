#pragma once

#include "stats/StatsTypes.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>

namespace swiftify {

class PlaybackController;

/// every song heard on this computer for long enough to count, kept one json
/// line each in a file of its own, so a play is a line appended and nothing
/// already kept is ever written again
class PlayHistory : public QObject {
    Q_OBJECT

public:
    /// a play counts once this much of it was heard...
    static constexpr qint64 kMinimumListenMs = 30'000;
    /// ...or this share of a song shorter than twice that
    static constexpr double kMinimumListenShare = 0.5;

    /// `path` is the file; empty keeps nothing on disk (tests)
    explicit PlayHistory(QString path, QObject *parent = nullptr);

    static QString defaultPath();
    /// whether `listenedMs` of `song` is a play
    static bool counts(const Song &song, qint64 listenedMs);

    const QList<PlayRecord> &records() const { return m_records; }
    /// keeps the play when it counts; reports whether it did
    bool record(const Song &song, const QDateTime &startedAt, qint64 listenedMs);
    /// the whole history as csv, a header and a play a row
    QByteArray toCsv() const;

signals:
    void recorded();

private:
    void load();
    void append(const PlayRecord &record);

    QString m_path;
    QList<PlayRecord> m_records;
};

/// follows the player and tells the history how long each song was heard:
/// time playing only, so a pause, a seek or loading leave it as it was
class PlayTracker : public QObject {
    Q_OBJECT

public:
    PlayTracker(PlaybackController *player, PlayHistory *history, QObject *parent = nullptr);
    ~PlayTracker() override;

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    /// the song now playing and whether it plays. what the player's signals
    /// carry, and what the tests drive it with
    void observe(const Song &song, bool playing);
    /// the song playing ends here (the app is quitting)
    void finish();

private:
    void pauseClock();

    PlaybackController *m_player;
    PlayHistory *m_history;
    bool m_enabled = true;
    Song m_song;
    QDateTime m_startedAt;
    qint64 m_heardMs = 0;
    QElapsedTimer m_clock;
};

} // namespace swiftify
