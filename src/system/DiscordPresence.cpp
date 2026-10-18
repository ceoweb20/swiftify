#include "system/DiscordPresence.h"
#include "app/Settings.h"
#include "playback/PlaybackController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJSEngine>
#include <QSignalBlocker>
#include <QUuid>
#include <QtEndian>

namespace swiftify {
namespace {
DiscordPresence *s_instance = nullptr;

QStringList endpoints()
{
    QStringList result;
#ifdef Q_OS_WIN
    for (int i = 0; i < 10; ++i) result << QStringLiteral("discord-ipc-%1").arg(i);
#else
    QStringList directories {qEnvironmentVariable("XDG_RUNTIME_DIR"), qEnvironmentVariable("TMPDIR"),
                             qEnvironmentVariable("TMP"), qEnvironmentVariable("TEMP"),
                             QDir::tempPath(), QStringLiteral("/tmp")};
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty()) directories << runtime + QStringLiteral("/app/com.discordapp.Discord")
                                          << runtime + QStringLiteral("/snap.discord");
    directories.removeAll(QString());
    directories.removeDuplicates();
    for (int i = 0; i < 10; ++i) {
        for (const auto &directory : directories) {
            const QString path = QDir(directory).filePath(QStringLiteral("discord-ipc-%1").arg(i));
            if (QFileInfo::exists(path)) result << path;
        }
    }
#endif
    return result;
}

QJsonObject command(const QJsonValue &activity)
{
    return {{QStringLiteral("cmd"), QStringLiteral("SET_ACTIVITY")},
            {QStringLiteral("args"), QJsonObject {{QStringLiteral("pid"), QCoreApplication::applicationPid()},
                                                {QStringLiteral("activity"), activity}}},
            {QStringLiteral("nonce"), QUuid::createUuid().toString(QUuid::WithoutBraces)}};
}

QString fitText(const QString &value)
{
    QString result = value.trimmed().left(128);
    if (!result.isEmpty() && result.at(result.size() - 1).isHighSurrogate()) result.chop(1);
    result = result.trimmed();
    if (result.size() == 1) result += QChar(0x00a0);
    return result;
}
}

QByteArray discord::frame(quint32 opcode, const QByteArray &payload)
{
    QByteArray result(8, '\0');
    qToLittleEndian(opcode, result.data());
    qToLittleEndian(quint32(payload.size()), result.data() + 4);
    result += payload;
    return result;
}

discord::FrameResult discord::takeFrame(QByteArray &buffer, quint32 &opcode, QByteArray &payload)
{
    if (buffer.size() < 8) return FrameResult::Incomplete;
    const quint32 size = qFromLittleEndian<quint32>(buffer.constData() + 4);
    if (size > maxFrameSize) return FrameResult::Invalid;
    if (buffer.size() < qsizetype(size) + 8) return FrameResult::Incomplete;
    opcode = qFromLittleEndian<quint32>(buffer.constData());
    payload = buffer.mid(8, size);
    buffer.remove(0, size + 8);
    return FrameResult::Complete;
}

DiscordPresence::DiscordPresence(PlaybackController *player, Settings *settings, QObject *parent)
    : QObject(parent), m_player(player), m_settings(settings)
{
    s_instance = this;
    m_retry.setInterval(15000);
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(5000);
    m_update.setSingleShot(true);
    m_update.setInterval(250);
    m_socket.setReadBufferSize(discord::maxFrameSize + 8);
    connect(&m_retry, &QTimer::timeout, this, [this] {
        m_endpoints = endpoints();
        m_endpoint = 0;
        connectNext();
    });
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_socket.abort();
        disconnected();
    });
    connect(&m_update, &QTimer::timeout, this, &DiscordPresence::update);
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
        send(0, QJsonDocument(QJsonObject {{QStringLiteral("v"), 1},
            {QStringLiteral("client_id"), m_settings->discordApplicationId()}}).toJson(QJsonDocument::Compact));
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, &DiscordPresence::readFrames);
    connect(&m_socket, &QLocalSocket::disconnected, this, &DiscordPresence::disconnected);
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this] { disconnected(); });
    connect(settings, &Settings::discordEnabledChanged, this, &DiscordPresence::configure);
    connect(settings, &Settings::discordApplicationIdChanged, this, &DiscordPresence::configure);
    connect(player, &PlaybackController::mediaStateChanged, this, &DiscordPresence::scheduleUpdate);
    connect(player, &PlaybackController::currentSongChanged, this, &DiscordPresence::scheduleUpdate);
    connect(player, &PlaybackController::playingChanged, this, &DiscordPresence::scheduleUpdate);
    connect(player, &PlaybackController::loadingChanged, this, &DiscordPresence::scheduleUpdate);
    connect(player, &PlaybackController::positionChanged, this, &DiscordPresence::scheduleUpdate);
    connect(player, &PlaybackController::durationChanged, this, &DiscordPresence::scheduleUpdate);
    configure();
}

DiscordPresence::~DiscordPresence()
{
    clear();
    if (m_socket.bytesToWrite() > 0) m_socket.waitForBytesWritten(100);
    m_socket.abort();
    s_instance = nullptr;
}

DiscordPresence *DiscordPresence::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void DiscordPresence::setReady(bool ready)
{
    if (m_ready == ready) return;
    m_ready = ready;
    emit connectedChanged();
}

void DiscordPresence::configure()
{
    m_retry.stop();
    m_deadline.stop();
    m_update.stop();
    clear();
    // Disconnect our socket callbacks while replacing an old connection.
    m_socket.flush();
    const QSignalBlocker blocker(&m_socket);
    m_socket.abort();
    setReady(false);
    m_published = false;
    m_buffer.clear();
    m_endpoints.clear();
    if (!m_settings->discordEnabled() || m_settings->discordApplicationId().isEmpty()) return;
    m_endpoints = endpoints();
    m_endpoint = 0;
    m_retry.start();
    QTimer::singleShot(0, this, &DiscordPresence::connectNext);
}

void DiscordPresence::connectNext()
{
    if (!m_settings->discordEnabled() || m_settings->discordApplicationId().isEmpty()
        || m_socket.state() != QLocalSocket::UnconnectedState || m_endpoint >= m_endpoints.size()) return;
    m_buffer.clear();
    m_deadline.start();
    m_socket.connectToServer(m_endpoints.at(m_endpoint++));
}

void DiscordPresence::disconnected()
{
    m_deadline.stop();
    setReady(false);
    m_published = false;
    m_buffer.clear();
    QTimer::singleShot(0, this, &DiscordPresence::connectNext);
}

void DiscordPresence::send(quint32 opcode, const QByteArray &payload)
{
    if (m_socket.state() != QLocalSocket::ConnectedState) return;
    if (m_socket.bytesToWrite() > discord::maxFrameSize) {
        m_socket.abort();
        return;
    }
    m_socket.write(discord::frame(opcode, payload));
}

void DiscordPresence::readFrames()
{
    m_buffer += m_socket.readAll();
    quint32 opcode;
    QByteArray payload;
    while (true) {
        const auto result = discord::takeFrame(m_buffer, opcode, payload);
        if (result == discord::FrameResult::Incomplete) return;
        if (result == discord::FrameResult::Invalid) { m_socket.abort(); return; }
        if (opcode == 3) { send(4, payload); continue; } // ping / pong
        if (opcode == 4) continue;
        if (opcode != 1) { m_socket.abort(); return; } // close or unknown
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(payload, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) { m_socket.abort(); return; }
        const QString event = document.object().value(QStringLiteral("evt")).toString();
        if (event == QLatin1StringView("ERROR")) { m_socket.abort(); return; }
        if (event == QLatin1StringView("READY") && !m_ready) {
            m_deadline.stop();
            setReady(true);
            update();
        }
    }
}

void DiscordPresence::scheduleUpdate()
{
    if (m_ready && !m_update.isActive()) m_update.start();
}

void DiscordPresence::clear()
{
    if (!m_ready || !m_published) return;
    send(1, QJsonDocument(command(QJsonValue::Null)).toJson(QJsonDocument::Compact));
    m_published = false;
}

void DiscordPresence::update()
{
    if (!m_ready) return;
    if (!m_player->hasSong() || m_player->isLoading()) { clear(); return; }
    const Song song = m_player->currentSong();
    if (song.uri == QLatin1StringView(PlaybackController::kLoadingTestUri)) { clear(); return; }
    const QString title = fitText(song.name);
    const bool playing = m_player->isPlaying();
    QJsonObject activity {{QStringLiteral("type"), 2},
                          {QStringLiteral("name"), QStringLiteral("Spotify")},
                          {QStringLiteral("details"), title.isEmpty() ? QStringLiteral("Listening to music") : title},
                          {QStringLiteral("instance"), false}};
    const QString state = fitText(playing ? song.artists
        : song.artists.trimmed().isEmpty() ? tr("Paused") : tr("Paused") + QStringLiteral(" · ") + song.artists);
    if (!state.isEmpty()) activity.insert(QStringLiteral("state"), state);
    // Never expose local file paths, filenames, or private artwork URLs.
    if (!song.isLocalFile() && song.artworkUrl.scheme() == QLatin1StringView("https")
        && song.artworkUrl.host() == QLatin1StringView("i.scdn.co")) {
        QJsonObject assets {{QStringLiteral("large_image"), song.artworkUrl.toString()}};
        const QString album = fitText(song.albumName);
        if (!album.isEmpty()) assets.insert(QStringLiteral("large_text"), album);
        activity.insert(QStringLiteral("assets"), assets);
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 position = m_player->livePositionMs();
    // The output's first duration arrives on the next player poll. Metadata
    // already has it, so the initial activity needn't lose its progress bar.
    const qint64 duration = m_player->durationMs() > 0 ? m_player->durationMs() : song.durationMs;
    // Publish on metadata/track changes and seeks, not every progress tick.
    QJsonObject metadata = activity;
    metadata.insert(QStringLiteral("track"), QString::number(song.instanceId));
    metadata.insert(QStringLiteral("duration"), duration);
    metadata.insert(QStringLiteral("playing"), playing);
    if (m_published && metadata == m_lastMetadata
        && (!playing || qAbs(position - (m_lastPosition + now - m_lastUpdate)) < 2000)) return;
    if (playing && duration > position) {
        activity.insert(QStringLiteral("timestamps"), QJsonObject {
            {QStringLiteral("start"), (now - position) / 1000},
            {QStringLiteral("end"), (now + duration - position) / 1000}});
    }
    send(1, QJsonDocument(command(activity)).toJson(QJsonDocument::Compact));
    m_lastMetadata = metadata;
    m_lastPosition = position;
    m_lastUpdate = now;
    m_published = true;
}

} // namespace swiftify
