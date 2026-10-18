#include "FakeCore.h"
#include "app/ErrorCenter.h"
#include "app/Settings.h"
#include "playback/PlaybackController.h"
#include "spotify/CredentialStore.h"
#include "spotify/SpotifySession.h"
#include "system/DiscordPresence.h"

#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QNetworkAccessManager>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <QtEndian>

using namespace swiftify;
using swiftify::testing::FakeCore;

class DiscordTest : public QObject {
    Q_OBJECT
private:
    static QString temporaryTemplate()
    {
#ifdef Q_OS_WIN
        return QDir::tempPath() + QStringLiteral("/swiftify-rpc-XXXXXX");
#else
        // Unix socket addresses have a much shorter limit than normal files.
        return QStringLiteral("/tmp/swiftify-rpc-XXXXXX");
#endif
    }

    static QString serverName(const QString &directory, int slot)
    {
#ifdef Q_OS_WIN
        Q_UNUSED(directory)
        // QLocalServer/QLocalSocket use Windows named pipes for these names.
        return QStringLiteral("swiftify-discord-test-%1-%2").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)).arg(slot);
#else
        return QDir(directory).filePath(QStringLiteral("discord-ipc-%1").arg(slot));
#endif
    }

    struct Rig {
        // Nested sandbox layouts exceed macOS's Unix socket path limit under
        // the default long per-user temporary directory.
        QTemporaryDir directory {temporaryTemplate()};
        Settings settings {directory.filePath(QStringLiteral("settings.ini"))};
        ErrorCenter errors {nullptr};
        MemoryCredentialStore credentials;
        QNetworkAccessManager network;
        FakeCore core;
        SpotifySession session {&core, &credentials, &network, &errors};
        PlaybackController player {&core, &session, &settings, &errors};
        QStringList candidates;
        DiscordPresence presence {&player, &settings, nullptr, [this] { return candidates; }};

        void enable()
        {
            settings.setDiscordApplicationId(QStringLiteral("123456789012345678"));
            settings.setDiscordEnabled(true);
        }
    };

    static bool readMessage(QLocalSocket *peer, QByteArray &buffer, QJsonObject &message)
    {
        if (!message.isEmpty()) return true;
        buffer += peer->readAll();
        quint32 opcode;
        QByteArray payload;
        if (discord::takeFrame(buffer, opcode, payload) != discord::FrameResult::Complete) return false;
        message = QJsonDocument::fromJson(payload).object();
        return !message.isEmpty();
    }

private slots:
    void discoversSandboxSockets_data()
    {
        QTest::addColumn<QString>("suffix");
        for (const char *suffix : {"", "app/com.discordapp.Discord", "app/com.discordapp.DiscordCanary",
             "app/com.discordapp.DiscordPTB", "app/dev.vencord.Vesktop", ".flatpak/com.discordapp.Discord/xdg-run",
             ".flatpak/com.discordapp.DiscordCanary/xdg-run", ".flatpak/dev.vencord.Vesktop/xdg-run",
             "snap.discord", "snap.discord-canary"}) {
            QTest::newRow(*suffix ? suffix : "native") << QString::fromLatin1(suffix);
        }
    }

    void discoversSandboxSockets()
    {
#ifdef Q_OS_WIN
        QSKIP("Unix filesystem socket layouts");
#else
        QFETCH(QString, suffix);
        QTemporaryDir directory(QStringLiteral("/tmp/swiftify-rpc-XXXXXX"));
        const QString folder = QDir(directory.path()).filePath(suffix);
        QVERIFY(QDir().mkpath(folder));
        QLocalServer server;
        QVERIFY2(server.listen(QDir(folder).filePath(QStringLiteral("discord-ipc-7"))), qPrintable(server.errorString()));
        const auto found = discord::socketCandidates({QString(), QStringLiteral("relative"), directory.path(), directory.path()});
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first(), server.fullServerName());
#endif
    }

    void skipsUnreachableCandidateAndExplainsProtocolRejections()
    {
        Rig rig;
        const QString unreachable = serverName(rig.directory.path(), 0);
#ifndef Q_OS_WIN
        QFile stale(unreachable);
        QVERIFY(stale.open(QIODevice::WriteOnly));
        stale.close();
#endif
        QLocalServer server;
        QVERIFY(server.listen(serverName(rig.directory.path(), 1)));
        rig.candidates = {unreachable, server.fullServerName()};
        QCOMPARE(rig.candidates.size(), 2);
        rig.enable();
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 2000);
        std::unique_ptr<QLocalSocket> peer(server.nextPendingConnection());
        QByteArray buffer;
        QJsonObject message;
        QTRY_VERIFY(readMessage(peer.get(), buffer, message));
        peer->write(discord::frame(1, QByteArrayLiteral("{\"evt\":\"READY\"}")));
        peer->flush();
        QTRY_VERIFY(rig.presence.connected());
        QCOMPARE(rig.presence.status(), QStringLiteral("Connected to Discord"));
        peer->write(discord::frame(1, QByteArrayLiteral("{\"evt\":\"ERROR\",\"data\":{\"code\":4002,\"message\":\"sensitive payload\"}}")));
        peer->flush();
        QTRY_VERIFY(!rig.presence.connected());
        QVERIFY(rig.presence.status().contains(QStringLiteral("activity")));
        QVERIFY(rig.presence.status().contains(QStringLiteral("4002")));
        QVERIFY(!rig.presence.status().contains(QStringLiteral("sensitive")));
    }

    void reportsHandshakeCloseCodeAndMissingSockets()
    {
        Rig rig;
        rig.enable();
        QTRY_VERIFY(rig.presence.status().contains(QStringLiteral("No reachable Discord IPC")));
        rig.settings.setDiscordEnabled(false);
        QLocalServer server;
        QVERIFY(server.listen(serverName(rig.directory.path(), 0)));
        rig.candidates = {server.fullServerName()};
        rig.settings.setDiscordEnabled(true);
        QTRY_VERIFY(server.hasPendingConnections());
        std::unique_ptr<QLocalSocket> peer(server.nextPendingConnection());
        QByteArray buffer;
        QJsonObject message;
        QTRY_VERIFY(readMessage(peer.get(), buffer, message));
        peer->write(discord::frame(2, QByteArrayLiteral("{\"code\":4000,\"message\":\"sensitive payload\"}")));
        peer->flush();
        QTRY_VERIFY(rig.presence.status().contains(QStringLiteral("4000")));
        QVERIFY(!rig.presence.connected());
        QVERIFY(rig.presence.status().contains(QStringLiteral("connection")));
        QVERIFY(!rig.presence.status().contains(QStringLiteral("sensitive")));
    }

    void fragmentedAndCoalescedFrames()
    {
        const QByteArray first = discord::frame(1, QByteArrayLiteral("{\"evt\":\"READY\"}"));
        const QByteArray second = discord::frame(3, QByteArrayLiteral("ping"));
        QByteArray buffer = first.left(6);
        quint32 opcode = 99;
        QByteArray payload;
        QCOMPARE(discord::takeFrame(buffer, opcode, payload), discord::FrameResult::Incomplete);
        buffer += first.mid(6, 4);
        QCOMPARE(discord::takeFrame(buffer, opcode, payload), discord::FrameResult::Incomplete);
        buffer += first.mid(10) + second;
        QCOMPARE(discord::takeFrame(buffer, opcode, payload), discord::FrameResult::Complete);
        QCOMPARE(opcode, 1u);
        QCOMPARE(payload, QByteArrayLiteral("{\"evt\":\"READY\"}"));
        QCOMPARE(discord::takeFrame(buffer, opcode, payload), discord::FrameResult::Complete);
        QCOMPARE(opcode, 3u);
        QCOMPARE(payload, QByteArrayLiteral("ping"));
        QVERIFY(buffer.isEmpty());
    }

    void rejectsOversizedFrames()
    {
        QByteArray buffer = discord::frame(1, {});
        qToLittleEndian(discord::maxFrameSize + 1, buffer.data() + 4);
        quint32 opcode;
        QByteArray payload;
        QCOMPARE(discord::takeFrame(buffer, opcode, payload), discord::FrameResult::Invalid);
    }

    void handshakePlaybackPauseDisableAndReconnect()
    {
        // An isolated fake Discord; no Discord account or real Spotify request.
        QTemporaryDir directory;
        const bool hadRuntime = qEnvironmentVariableIsSet("XDG_RUNTIME_DIR");
        const QByteArray oldRuntime = qgetenv("XDG_RUNTIME_DIR");
        qputenv("XDG_RUNTIME_DIR", directory.path().toUtf8());
        const auto restore = qScopeGuard([&] {
            if (hadRuntime) qputenv("XDG_RUNTIME_DIR", oldRuntime);
            else qunsetenv("XDG_RUNTIME_DIR");
        });
        QLocalServer server;
        QVERIFY2(server.listen(serverName(directory.path(), 0)), qPrintable(server.errorString()));
        Settings settings(directory.filePath(QStringLiteral("settings.ini")));
        ErrorCenter errors(nullptr);
        MemoryCredentialStore credentials;
        QNetworkAccessManager network;
        FakeCore core;
        SpotifySession session(&core, &credentials, &network, &errors);
        PlaybackController player(&core, &session, &settings, &errors);
        DiscordPresence presence(&player, &settings, nullptr, [&] { return QStringList {server.fullServerName()}; });
        QVERIFY(!presence.connected());
        QCoreApplication::processEvents();
        QVERIFY(!server.hasPendingConnections());

        settings.setDiscordApplicationId(QStringLiteral("123456789012345678"));
        settings.setDiscordEnabled(true);
        QTRY_VERIFY(server.hasPendingConnections());
        std::unique_ptr<QLocalSocket> peer(server.nextPendingConnection());
        QByteArray buffer;
        QJsonObject message;
        auto read = [&]() -> bool {
            if (!message.isEmpty()) return true;
            buffer += peer->readAll();
            quint32 opcode;
            QByteArray payload;
            if (discord::takeFrame(buffer, opcode, payload) != discord::FrameResult::Complete) return false;
            message = QJsonDocument::fromJson(payload).object();
            return !message.isEmpty();
        };
        QTRY_VERIFY(read());
        QCOMPARE(message.value(QStringLiteral("v")).toInt(), 1);
        QCOMPARE(message.value(QStringLiteral("client_id")).toString(), settings.discordApplicationId());
        peer->write(discord::frame(1, QByteArrayLiteral("{\"evt\":\"READY\"}")));
        peer->flush();
        QTRY_VERIFY(presence.connected());
        message = {};

        Song song;
        song.name = QStringLiteral("Я"); // one UTF-16 unit must be padded for Discord
        song.artists = QStringLiteral("Исполнитель");
        song.albumName = QStringLiteral("Альбом");
        song.uri = QUrl::fromLocalFile(directory.filePath(QStringLiteral("private.flac"))).toString();
        song.artworkUrl = QUrl(QStringLiteral("https://private.example/cover?token=secret"));
        song.durationMs = 200000;
        core.duration = song.durationMs;
        player.play(song, {song});
        QTRY_VERIFY(!core.pending.isEmpty());
        const auto call = core.take(QStringLiteral("playFile"));
        QVERIFY(call);
        call->succeed();
        QTRY_VERIFY(player.isPlaying());
        QTRY_VERIFY(read());
        QCOMPARE(message.value(QStringLiteral("cmd")).toString(), QStringLiteral("SET_ACTIVITY"));
        auto activity = message.value(QStringLiteral("args")).toObject().value(QStringLiteral("activity")).toObject();
        QCOMPARE(activity.value(QStringLiteral("type")).toInt(), 2);
        QCOMPARE(activity.value(QStringLiteral("name")).toString(), QStringLiteral("Swiftify"));
        QCOMPARE(activity.value(QStringLiteral("details")).toString(), QStringLiteral("Я\u00a0"));
        QCOMPARE(activity.value(QStringLiteral("state")).toString(), song.artists);
        QVERIFY(activity.contains(QStringLiteral("timestamps")));
        QVERIFY(!activity.contains(QStringLiteral("assets")));
        QVERIFY(!QJsonDocument(message).toJson().contains("private"));

        player.pause();
        message = {};
        QTRY_VERIFY(read());
        activity = message.value(QStringLiteral("args")).toObject().value(QStringLiteral("activity")).toObject();
        QCOMPARE(activity.value(QStringLiteral("name")).toString(), QStringLiteral("Swiftify"));
        QCOMPARE(activity.value(QStringLiteral("details")).toString(), QStringLiteral("Я\u00a0"));
        QCOMPARE(activity.value(QStringLiteral("state")).toString(), QStringLiteral("Paused · Исполнитель"));
        QVERIFY(!activity.contains(QStringLiteral("timestamps")));

        // Pause doesn't keep sending an activity as wall-clock time advances.
        message = {};
        QTest::qWait(2200);
        player.adjustVolume(-0.05);
        QTest::qWait(350);
        buffer += peer->readAll();
        QVERIFY(buffer.isEmpty());

        player.resume();
        QTRY_VERIFY(player.isPlaying());
        QTRY_VERIFY(read());
        activity = message.value(QStringLiteral("args")).toObject().value(QStringLiteral("activity")).toObject();
        QCOMPARE(activity.value(QStringLiteral("state")).toString(), song.artists);
        QVERIFY(activity.contains(QStringLiteral("timestamps")));

        // Disabling still explicitly clears the activity, even after a pause.
        player.pause();
        message = {};
        QTRY_VERIFY(read());
        message = {};
        settings.setDiscordEnabled(false);
        QTRY_VERIFY(read());
        QVERIFY(message.value(QStringLiteral("args")).toObject().value(QStringLiteral("activity")).isNull());
        QTRY_VERIFY(!presence.connected());
        QTRY_COMPARE(peer->state(), QLocalSocket::UnconnectedState);

        settings.setDiscordEnabled(true);
        QTRY_VERIFY(server.hasPendingConnections());
        peer.reset(server.nextPendingConnection());
        buffer.clear();
        message = {};
        QTRY_VERIFY(read());
        peer->write(discord::frame(1, QByteArrayLiteral("{\"evt\":\"READY\"}")));
        peer->flush();
        QTRY_VERIFY(presence.connected());
        message = {};
        QTRY_VERIFY(read());
        activity = message.value(QStringLiteral("args")).toObject().value(QStringLiteral("activity")).toObject();
        QCOMPARE(activity.value(QStringLiteral("state")).toString(), QStringLiteral("Paused · Исполнитель"));
        QVERIFY(!activity.contains(QStringLiteral("timestamps")));
        // Ping payload is opaque, not necessarily JSON.
        peer->write(discord::frame(3, QByteArrayLiteral("ping")));
        peer->flush();
        QTRY_VERIFY(peer->bytesAvailable() >= 12);
        buffer = peer->readAll();
        quint32 opcode;
        QByteArray payload;
        QCOMPARE(discord::takeFrame(buffer, opcode, payload), discord::FrameResult::Complete);
        QCOMPARE(opcode, 4u);
        QCOMPARE(payload, QByteArrayLiteral("ping"));
        settings.setDiscordEnabled(false);
    }
};

QTEST_GUILESS_MAIN(DiscordTest)
#include "tst_discord.moc"
