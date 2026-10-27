#include "spotify/CredentialVault.h"
#include "spotify/SpotifySession.h"
#include "spotify/OAuthFlow.h"
#include "spotify/SpotifyError.h"
#include "app/ErrorCenter.h"
#include "FakeCore.h"
#include "core/Async.h"
#include <QFile>
#include <QDir>
#include <QLockFile>
#include <QTemporaryDir>
#include <QTest>
#include <QNetworkAccessManager>

using namespace swiftify;
namespace {
const QString password = QStringLiteral("fixture password only");
class MemoryUnlockStore final : public VaultUnlockStore {
public:
    QByteArray secret;
    bool writable = true;
    QByteArray load() override { return secret; }
    bool save(const QByteArray &value) override {
        if (!writable) return false;
        secret = value;
        return true;
    }
};
QByteArray bytes(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QCoro::Task<> waitingLoad(CredentialVault &vault, bool &done) {
    co_await vault.load(CredentialStore::Account::Playback);
    done = true;
}
}

class VaultTest : public QObject {
    Q_OBJECT
private slots:
    void newVaultUnlocksAutomaticallyAcrossLaunches()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("credentials/logins.vault"));
        auto store = std::make_shared<MemoryUnlockStore>();
        {
            CredentialVault vault(path, nullptr, store);
            vault.restore();
            QTRY_VERIFY_WITH_TIMEOUT(vault.unlocked(), 15000);
            QVERIFY(store->secret.size() >= 32);
            QCoro::waitFor(vault.save(CredentialStore::Account::Playback,
                {QStringLiteral("client"), QStringLiteral("fixture-token"), 2}));
            QVERIFY(!bytes(path).contains(store->secret));
            QVERIFY(!bytes(path).contains("fixture-token"));
        }
        {
            CredentialVault vault(path, nullptr, store);
            vault.restore();
            QTRY_VERIFY_WITH_TIMEOUT(vault.unlocked(), 15000);
            const auto credentials = QCoro::waitFor(vault.load(CredentialStore::Account::Playback));
            QVERIFY(credentials);
            QCOMPARE(credentials->refreshToken, QStringLiteral("fixture-token"));
        }
    }

    void oldVaultPasswordIsRememberedOnlyAfterSuccessfulUnlock()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("logins.vault"));
        {
            CredentialVault vault(path);
            vault.unlock(password);
            QTRY_VERIFY_WITH_TIMEOUT(vault.unlocked(), 15000);
        }
        auto store = std::make_shared<MemoryUnlockStore>();
        {
            CredentialVault vault(path, nullptr, store);
            vault.restore();
            QVERIFY(!vault.unlocked());
            vault.unlock(QStringLiteral("wrong password"));
            QTRY_VERIFY_WITH_TIMEOUT(!vault.busy(), 15000);
            QVERIFY(store->secret.isEmpty());
            vault.unlock(password);
            QTRY_VERIFY_WITH_TIMEOUT(vault.unlocked(), 15000);
            QCOMPARE(store->secret, password.toUtf8());
        }
        {
            CredentialVault vault(path, nullptr, store);
            vault.restore();
            QTRY_VERIFY_WITH_TIMEOUT(vault.unlocked(), 15000);
        }
    }

    void failedSecureStorageNeverCreatesAnUnrecoverableVault()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("logins.vault"));
        auto store = std::make_shared<MemoryUnlockStore>();
        store->writable = false;
        CredentialVault vault(path, nullptr, store);
        vault.restore();
        QVERIFY(!vault.unlocked());
        QVERIFY(!vault.exists());
        QVERIFY(!vault.error().isEmpty());
    }

    void automaticCreationDoesNotReplaceThePasswordWhileAnotherInstanceOwnsTheLock()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("logins.vault"));
        QLockFile lock(path + QStringLiteral(".lock"));
        QVERIFY(lock.tryLock(0));
        auto store = std::make_shared<MemoryUnlockStore>();
        CredentialVault vault(path, nullptr, store);
        vault.restore();
        QVERIFY(!vault.unlocked());
        QVERIFY(!vault.exists());
        QVERIFY(store->secret.isEmpty());
    }

#ifdef Q_OS_WIN
    void windowsPasswordIsProtectedByDpapi()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("logins.vault"));
        auto store = systemVaultUnlockStore(path);
        QVERIFY(store);
        QVERIFY(store->save(password.toUtf8()));
        QVERIFY(!bytes(path + QStringLiteral(".dpapi")).contains(password.toUtf8()));
        QCOMPARE(store->load(), password.toUtf8());
    }
#endif

    void quittingBeforeUnlockDoesNotResumeADestroyedSession()
    {
        QTemporaryDir directory;
        auto vault = std::make_unique<CredentialVault>(directory.filePath(QStringLiteral("logins.vault")));
        auto credentials = std::make_unique<CachedCredentialStore>(vault.get());
        ErrorCenter errors(nullptr);
        QNetworkAccessManager network;
        swiftify::testing::FakeCore core;
        auto session = std::make_unique<SpotifySession>(&core, credentials.get(), &network, &errors);
        session->restoreSavedSessions();
        session.reset();
        credentials.reset();
        vault.reset();
        QCoreApplication::processEvents();
        QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("logins.vault"))));
    }

    void sessionRestorationWaitsForThePassword()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("logins.vault"));
        {
            CredentialVault initial(path);
            initial.unlock(password);
            QTRY_VERIFY_WITH_TIMEOUT(initial.unlocked(), 15000);
            StoredCredentials playback {QString::fromLatin1(OAuthFlow::kPlaybackClientId), {},
                OAuthFlow::kPlaybackAuthorizationVersion, QStringLiteral("fixture-login")};
            StoredCredentials library {QStringLiteral("fixture-client"), QStringLiteral("fixture-refresh"),
                OAuthFlow::kLibraryAuthorizationVersion, {}, QStringLiteral("fixture-access"),
                QDateTime::currentDateTimeUtc().addSecs(3600)};
            QCoro::waitFor(initial.save(CredentialStore::Account::Playback, playback));
            QCoro::waitFor(initial.save(CredentialStore::Account::Library, library));
        }
        CredentialVault vault(path);
        CachedCredentialStore credentials(&vault);
        ErrorCenter errors(nullptr);
        QNetworkAccessManager network;
        swiftify::testing::FakeCore core;
        SpotifySession session(&core, &credentials, &network, &errors);
        session.setInitialLibraryLoader({});
        session.restoreSavedSessions();
        session.restoreSavedSessions();
        QVERIFY(!session.hasLibraryToken());
        QVERIFY(!core.take(QStringLiteral("connectStored")));
        vault.unlock(password);
        QTRY_VERIFY_WITH_TIMEOUT(vault.unlocked(), 15000);
        QTRY_VERIFY(session.hasLibraryToken());
        const auto login = core.take(QStringLiteral("connectStored"));
        QVERIFY(login); QCOMPARE(login->argument, QStringLiteral("fixture-login"));
        login->succeed(QStringLiteral("fixture-login"));
        QTRY_VERIFY(session.isConnected());
        QVERIFY(!core.take(QStringLiteral("connectStored")));
        core.reset();
    }

    void encryptedLoginsSurviveRestartAndLogout()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("private/logins.vault"));
        const StoredCredentials playback {QStringLiteral("client"), QStringLiteral("fixture-refresh-secret"), 2,
            QStringLiteral("fixture-reusable-login")};
        const StoredCredentials library {QStringLiteral("library-client"), QStringLiteral("fixture-library-secret"), 2};
        {
            CredentialVault vault(path);
            QVERIFY(!vault.unlocked()); QVERIFY(!vault.exists());
            bool restored = false;
            detach(waitingLoad(vault, restored));
            QVERIFY(!restored);
            vault.unlock(password);
            QTRY_VERIFY_WITH_TIMEOUT(vault.unlocked(), 15000);
            QTRY_VERIFY(restored);
            QCoro::waitFor(vault.save(CredentialStore::Account::Playback, playback));
            QCoro::waitFor(vault.save(CredentialStore::Account::Library, library));
            const auto encrypted = bytes(path);
            QVERIFY(!encrypted.isEmpty());
            QVERIFY(!encrypted.contains("fixture-refresh-secret"));
            QVERIFY(!encrypted.contains("fixture-reusable-login"));
            QVERIFY(!encrypted.contains(password.toUtf8()));
#ifdef Q_OS_UNIX
            QVERIFY(!(QFile::permissions(path) & (QFile::ReadGroup | QFile::ReadOther | QFile::WriteGroup | QFile::WriteOther)));
#endif
        }
        {
            CredentialVault vault(path);
            const auto before = bytes(path);
            vault.unlock(QStringLiteral("incorrect password"));
            QTRY_VERIFY_WITH_TIMEOUT(!vault.busy(), 15000);
            QVERIFY(!vault.unlocked()); QVERIFY(!vault.error().isEmpty());
            QCOMPARE(bytes(path), before);
            vault.unlock(password);
            QTRY_VERIFY_WITH_TIMEOUT(vault.unlocked(), 15000);
            for (int i = 0; i < 3; ++i) {
                const auto read = QCoro::waitFor(vault.load(CredentialStore::Account::Playback));
                QVERIFY(read); QCOMPARE(read->refreshToken, playback.refreshToken);
            }
            const auto readLibrary = QCoro::waitFor(vault.load(CredentialStore::Account::Library));
            QVERIFY(readLibrary); QCOMPARE(readLibrary->refreshToken, library.refreshToken);
            auto rotated = playback;
            rotated.refreshToken = QStringLiteral("fixture-rotated-secret");
            QCoro::waitFor(vault.save(CredentialStore::Account::Playback, rotated));
            QVERIFY(bytes(path) != before);
            QCoro::waitFor(vault.removeAll());
            QVERIFY(vault.unlocked()); // Logging out does not ask for the password again.
            QVERIFY(!QCoro::waitFor(vault.load(CredentialStore::Account::Playback)));
        }
        {
            CredentialVault vault(path);
            vault.unlock(password);
            QTRY_VERIFY_WITH_TIMEOUT(vault.unlocked(), 15000);
            QVERIFY(!QCoro::waitFor(vault.load(CredentialStore::Account::Playback)));
            QVERIFY(!QCoro::waitFor(vault.load(CredentialStore::Account::Library)));
        }
    }

    void emptyAndDamagedFilesAreNeverOverwritten()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("logins.vault"));
        for (const QByteArray value : {QByteArray(), QByteArray("truncated vault")}) {
            QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(value); file.close();
            CredentialVault vault(path);
            vault.unlock(password);
            QTRY_VERIFY_WITH_TIMEOUT(!vault.busy(), 15000);
            QVERIFY(!vault.unlocked()); QVERIFY(!vault.error().isEmpty());
            QCOMPARE(bytes(path), value);
        }
    }

    void failedWritesKeepThePreviousLogins()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("logins.vault"));
        CredentialVault vault(path);
        vault.unlock(password);
        QTRY_VERIFY_WITH_TIMEOUT(vault.unlocked(), 15000);
        StoredCredentials login {QStringLiteral("client"), QStringLiteral("original-secret"), 2};
        QCoro::waitFor(vault.save(CredentialStore::Account::Playback, login));
        QVERIFY(QFile::rename(path, path + QStringLiteral(".backup")));
        QVERIFY(QDir().mkdir(path)); // A directory cannot be replaced by QSaveFile.
        login.refreshToken = QStringLiteral("replacement-secret");
        bool failed = false;
        try { QCoro::waitFor(vault.save(CredentialStore::Account::Playback, login)); }
        catch (const SpotifyError &) { failed = true; }
        QVERIFY(failed);
        const auto stored = QCoro::waitFor(vault.load(CredentialStore::Account::Playback));
        QVERIFY(stored); QCOMPARE(stored->refreshToken, QStringLiteral("original-secret"));
        QVERIFY(QDir().rmdir(path));
        QVERIFY(QFile::rename(path + QStringLiteral(".backup"), path));
    }

    void aBusyVaultCannotBeOpenedByAnotherInstance()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("logins.vault"));
        QLockFile external(path + QStringLiteral(".lock"));
        QVERIFY(external.tryLock(0));
        CredentialVault vault(path);
        vault.unlock(password);
        QVERIFY(!vault.unlocked()); QVERIFY(!vault.error().isEmpty());
        QVERIFY(!QFile::exists(path));
    }
};

QTEST_GUILESS_MAIN(VaultTest)
#include "tst_vault.moc"
