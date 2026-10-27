#pragma once

#include "core/Async.h"
#include "spotify/SpotifyTypes.h"

#include <QObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QSet>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

class LibraryStore;
class PlaybackController;
class SpotifySession;

namespace dj {
/// A bounded queue with no duplicate/unplayable tracks and, where possible,
/// different artists next to each other. This is not Spotify's AI DJ algorithm.
QList<Song> queue(QList<Song> recommendations, QList<Song> familiar, int limit,
                  const QSet<QString> &excluded, QRandomGenerator &random);
}

class DjMix final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DJMix)
    QML_SINGLETON
    Q_PROPERTY(bool busy READ busy NOTIFY changed FINAL)
    Q_PROPERTY(bool active READ active NOTIFY changed FINAL)
    Q_PROPERTY(QString status READ status NOTIFY changed FINAL)

public:
    DjMix(SpotifySession *session, LibraryStore *library, PlaybackController *player, QObject *parent = nullptr);
    ~DjMix() override;
    static DjMix *create(QQmlEngine *, QJSEngine *);
    bool busy() const { return m_busy; }
    bool active() const { return m_active; }
    QString status() const { return m_status; }
    Q_INVOKABLE void start();
    Q_INVOKABLE void cancel();

signals:
    void changed();

private:
    QCoro::Task<> collect(CancelToken token);
    QPointer<SpotifySession> m_session;
    QPointer<LibraryStore> m_library;
    QPointer<PlaybackController> m_player;
    TaskSlot m_request;
    QSet<quint64> m_instances;
    bool m_busy = false;
    bool m_active = false;
    QString m_status;
};

} // namespace swiftify
