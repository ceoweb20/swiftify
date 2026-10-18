#pragma once

#include "core/Async.h"
#include "spotify/SpotifyTypes.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace swiftify {
class LibraryStore;
class Settings;
class SpotifySession;

/// A local board of actual saved/recent/linked playlists, not Spotify's home feed.
/// Discovery reads only documented Web API endpoints; opening an approved entry
/// delegates to the application's existing playlist player.
class PlaylistCollections : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Collections)
    QML_SINGLETON
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY changed FINAL)
    Q_PROPERTY(QString status READ status NOTIFY changed FINAL)
public:
    PlaylistCollections(SpotifySession *session, Settings *settings, LibraryStore *library, QObject *parent = nullptr);
    ~PlaylistCollections() override;
    static PlaylistCollections *create(QQmlEngine *, QJSEngine *);
    QVariantList entries() const;
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void add(const QString &reference);
    Q_INVOKABLE void removeLink(const QString &id);
    Q_INVOKABLE void open(const QString &id);

signals:
    void changed();

private:
    struct Entry { Playlist playlist; QString origin; QString error; bool available = false; };
    struct Cached { Playlist playlist; QDateTime expires; };
    bool begin();
    void reset();
    void identify(const QString &userId);
    QString preferenceKey() const;
    void saveLinks();
    void rebuild();
    bool report(const SpotifyError &error);
    QCoro::Task<> run(bool adding, QString id, CancelToken token);
    QCoro::Task<Playlist> metadata(QString id, CancelToken token);
    void check(const CancelToken &token, quint64 generation) const;

    SpotifySession *m_session;
    Settings *m_settings;
    LibraryStore *m_library;
    TaskSlot m_request;
    QList<Entry> m_entries;
    QList<Playlist> m_saved;
    QStringList m_recent;
    QStringList m_links;
    QHash<QString, Cached> m_cache;
    QHash<QString, QString> m_errors;
    QString m_userId;
    QString m_status;
    QDateTime m_blockedUntil;
    bool m_busy = false;
};
} // namespace swiftify
