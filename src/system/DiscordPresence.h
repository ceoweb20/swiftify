#pragma once

#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

class PlaybackController;
class Settings;

namespace discord {
constexpr quint32 maxFrameSize = 1024 * 1024;
QByteArray frame(quint32 opcode, const QByteArray &payload);
// A complete frame is removed from buffer. Invalid lengths abort the connection.
enum class FrameResult { Incomplete, Complete, Invalid };
FrameResult takeFrame(QByteArray &buffer, quint32 &opcode, QByteArray &payload);
}

/// Local Discord IPC only. No token, SDK, external service, or background thread.
/// Presence is opt-in; pauses keep a labeled track without a ticking timer.
/// Disabling clears it and closes the connection.
class DiscordPresence : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Discord)
    QML_SINGLETON
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged FINAL)
public:
    DiscordPresence(PlaybackController *player, Settings *settings, QObject *parent = nullptr);
    ~DiscordPresence() override;
    static DiscordPresence *create(QQmlEngine *, QJSEngine *);
    bool connected() const { return m_ready; }

signals:
    void connectedChanged();

private:
    void configure();
    void connectNext();
    void disconnected();
    void readFrames();
    void send(quint32 opcode, const QByteArray &payload);
    void scheduleUpdate();
    void update();
    void clear();
    void setReady(bool ready);

    PlaybackController *m_player;
    Settings *m_settings;
    QLocalSocket m_socket;
    QTimer m_retry;
    QTimer m_deadline;
    QTimer m_update;
    QStringList m_endpoints;
    qsizetype m_endpoint = 0;
    QByteArray m_buffer;
    bool m_ready = false;
    bool m_published = false;
    QJsonObject m_lastMetadata;
    qint64 m_lastPosition = 0;
    qint64 m_lastUpdate = 0;
};

} // namespace swiftify
