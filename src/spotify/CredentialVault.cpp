#include "spotify/CredentialVault.h"
#include "spotify/SpotifyError.h"
#include "app/ErrorCenter.h"
#include <swiftify_core.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPointer>
#include <QQmlEngine>
#include <QSaveFile>
#include <QRandomGenerator>
#include <QtConcurrent>
#include <QCoroSignal>

namespace swiftify {
namespace {
CredentialVault *s_instance = nullptr;
constexpr qint64 maximumBytes = 2 * 1024 * 1024;

struct SecretBytes {
    QByteArray bytes;
    ~SecretBytes() { bytes.fill('\0'); }
};
struct OpenResult {
    std::shared_ptr<SwiftifyVault> key;
    std::shared_ptr<SecretBytes> plaintext;
};

SpotifyError vaultError()
{
    return {SpotifyError::Kind::Credentials, QStringLiteral("Could not write the encrypted Spotify credential vault.")};
}
}

CredentialVault::CredentialVault(QString path, QObject *parent, std::shared_ptr<VaultUnlockStore> unlockStore)
    : QObject(parent), m_path(std::move(path)), m_unlockStore(std::move(unlockStore))
{
    if (inUse()) {
        Q_ASSERT(!s_instance);
        s_instance = this;
    }
}

CredentialVault::~CredentialVault()
{
    if (s_instance == this) s_instance = nullptr;
    m_items.clear();
    m_key.reset();
}

CredentialVault *CredentialVault::create(QQmlEngine *engine, QJSEngine *)
{
    // Test/preview engines using MemoryCredentialStore have no password gate.
    // This inactive provider cannot be used as a credential backend.
    if (!s_instance) return new CredentialVault(QString(), engine);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool CredentialVault::exists() const { return inUse() && QFileInfo::exists(m_path); }

void CredentialVault::restore()
{
    if (!inUse() || !m_unlockStore || m_busy || unlocked()) return;
    QByteArray password = m_unlockStore->load();
    if (password.isEmpty() && !exists()) {
        const QString directory = QFileInfo(m_path).absolutePath();
        if (!QDir().mkpath(directory)) {
            fail(tr("Could not open the credential vault directory."));
            return;
        }
        QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        m_lock = std::make_unique<QLockFile>(m_path + QStringLiteral(".lock"));
        if (!m_lock->tryLock(0)) {
            fail(tr("Another Swiftify instance is using this vault."));
            return;
        }
        // Another process may have created the vault before we acquired the lock.
        // Never replace its remembered password with a freshly generated one.
        if (exists()) {
            m_lock.reset();
            return;
        }
        QByteArray random(32, Qt::Uninitialized);
        for (auto &byte : random) byte = char(QRandomGenerator::system()->generate() & 0xff);
        password = random.toBase64();
        random.fill('\0');
        // Never create a randomly keyed vault unless its password is recoverable.
        if (!m_unlockStore->save(password)) {
            password.fill('\0');
            fail(tr("Secure automatic unlock is unavailable. Create a vault password to continue."));
            return;
        }
    }
    if (password.isEmpty()) return; // An old vault needs its password once for migration.
    m_restoring = true;
    unlock(QString::fromUtf8(password));
    password.fill('\0');
}

void CredentialVault::fail(const QString &message)
{
    m_busy = false;
    m_restoring = false;
    m_error = message;
    if (!unlocked()) m_lock.reset();
    emit changed();
}

void CredentialVault::unlock(const QString &password)
{
    if (!inUse() || m_busy || unlocked()) return;
    if (password.isEmpty() || password.toUtf8().size() > 1024 || (!exists() && password.size() < 12)) {
        fail(tr("Use a password of at least 12 characters for a new vault.")); return;
    }
    const QString directory = QFileInfo(m_path).absolutePath();
    if (!QDir().mkpath(directory)) { fail(tr("Could not open the credential vault directory.")); return; }
    QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    if (!m_lock) {
        m_lock = std::make_unique<QLockFile>(m_path + QStringLiteral(".lock"));
        if (!m_lock->tryLock(0)) { fail(tr("Another Swiftify instance is using this vault.")); return; }
    }
    QByteArray encoded;
    const bool opening = exists();
    if (opening) {
        QFile file(m_path);
        const QFileInfo info(m_path);
        if (!info.isFile() || info.isSymLink() || info.size() > maximumBytes
            || !file.open(QIODevice::ReadOnly) || file.size() > maximumBytes) {
            fail(tr("Could not read the credential vault. It has not been changed.")); return;
        }
        encoded = file.readAll();
        // An existing empty/truncated file is never treated as a new vault.
        if (encoded.isEmpty()) { fail(tr("Could not read the credential vault. It has not been changed.")); return; }
    }
    m_busy = true; m_error.clear(); emit changed();
    auto *watcher = new QFutureWatcher<OpenResult>(this);
    const bool remember = m_unlockStore && !m_restoring;
    auto rememberedPassword = std::make_shared<SecretBytes>();
    if (remember) rememberedPassword->bytes = password.toUtf8();
    connect(watcher, &QFutureWatcher<OpenResult>::finished, this, [this, watcher, opening, remember, rememberedPassword] {
        const OpenResult result = watcher->result();
        watcher->deleteLater();
        if (!result.key || !result.plaintext) {
            fail(tr("Wrong password or damaged vault. The saved file has not been changed.")); return;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(result.plaintext->bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()
            || (opening && (document.object().value(QStringLiteral("version")).toInt() != 1
                || !document.object().value(QStringLiteral("accounts")).isObject()))) {
            fail(tr("Invalid credential vault contents. The saved file has not been changed.")); return;
        }
        QHash<Account, StoredCredentials> items;
        const auto accounts = document.object().value(QStringLiteral("accounts")).toObject();
        for (const Account account : {Account::Playback, Account::Library}) {
            const auto name = accountName(account);
            if (!accounts.contains(name)) continue;
            const auto value = StoredCredentials::fromJson(QJsonDocument(accounts.value(name).toObject()).toJson());
            if (!value) {
                fail(tr("Invalid credential vault contents. The saved file has not been changed.")); return;
            }
            items.insert(account, *value);
        }
        if (!opening) {
            try { persist(items, result.key); }
            catch (const SpotifyError &) {
                fail(tr("Could not save the encrypted vault. Check directory permissions.")); return;
            }
        }
        m_items = items;
        m_key = result.key;
        m_restoring = false;
        if (remember && !m_unlockStore->save(rememberedPassword->bytes)) {
            if (auto *errors = ErrorCenter::instance()) {
                errors->show(tr("The vault is unlocked, but its password could not be remembered securely. You may be asked again next launch."));
            }
        }
        rememberedPassword->bytes.fill('\0');
        m_busy = false; m_error.clear(); emit changed(); emit ready();
    });
    watcher->setFuture(QtConcurrent::run([passwordBytes = password.toUtf8(), encoded]() mutable {
        char *plaintext = nullptr;
        char *error = nullptr;
        auto *key = swiftify_vault_open(passwordBytes.constData(), encoded.constData(), &plaintext, &error);
        passwordBytes.fill('\0');
        OpenResult result;
        if (key) result.key = std::shared_ptr<SwiftifyVault>(key, swiftify_vault_free);
        if (plaintext) {
            result.plaintext = std::make_shared<SecretBytes>();
            result.plaintext->bytes = QByteArray(plaintext);
            swiftify_secret_free(plaintext);
        }
        swiftify_string_free(error);
        return result;
    }));
}

void CredentialVault::persist(const QHash<Account, StoredCredentials> &items, const std::shared_ptr<SwiftifyVault> &key)
{
    QJsonObject accounts;
    for (auto it = items.cbegin(); it != items.cend(); ++it)
        accounts.insert(accountName(it.key()), QJsonDocument::fromJson(it->toJson()).object());
    QByteArray plaintext = QJsonDocument(QJsonObject {{QStringLiteral("version"), 1},
        {QStringLiteral("accounts"), accounts}}).toJson(QJsonDocument::Compact);
    char *error = nullptr;
    char *encrypted = swiftify_vault_seal(key.get(), plaintext.constData(), &error);
    plaintext.fill('\0');
    swiftify_string_free(error);
    if (!encrypted) throw vaultError();
    const QByteArray ciphertext(encrypted);
    swiftify_string_free(encrypted);
    if (QFileInfo(m_path).isSymLink()) throw vaultError();
    QSaveFile file(m_path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
        || file.write(ciphertext) != ciphertext.size() || !file.commit()) throw vaultError();
}

QCoro::Task<std::optional<StoredCredentials>> CredentialVault::load(Account account)
{
    if (!inUse()) throw vaultError();
    const QPointer<CredentialVault> self(this);
    while (self && !self->unlocked()) co_await qCoro(self.data(), &CredentialVault::ready);
    if (!self) throw Cancelled();
    const auto it = m_items.constFind(account);
    co_return it == m_items.cend() ? std::nullopt : std::optional<StoredCredentials>(*it);
}

QCoro::Task<> CredentialVault::save(Account account, StoredCredentials credentials)
{
    if (!unlocked()) throw vaultError();
    auto items = m_items;
    items.insert(account, credentials);
    persist(items, m_key);
    m_items = items;
    co_return;
}

QCoro::Task<> CredentialVault::removeAll()
{
    if (!unlocked()) throw vaultError();
    persist({}, m_key);
    m_items.clear();
    co_return;
}
} // namespace swiftify
