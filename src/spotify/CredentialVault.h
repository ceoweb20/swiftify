#pragma once

#include "spotify/CredentialStore.h"
#include "spotify/VaultUnlockStore.h"
#include <QObject>
#include <QJsonObject>
#include <QLockFile>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
struct SwiftifyVault;

namespace swiftify {

/// Encrypted Spotify logins; an optional OS-protected password store permits
/// unattended unlock without changing the existing encrypted vault format.
class CredentialVault final : public QObject, public CredentialStore {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool inUse READ inUse CONSTANT FINAL)
    Q_PROPERTY(bool exists READ exists NOTIFY changed FINAL)
    Q_PROPERTY(bool unlocked READ unlocked NOTIFY changed FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY changed FINAL)
    Q_PROPERTY(QString error READ error NOTIFY changed FINAL)
    Q_PROPERTY(bool automaticUnlock READ automaticUnlock CONSTANT FINAL)

public:
    explicit CredentialVault(QString path, QObject *parent = nullptr,
                             std::shared_ptr<VaultUnlockStore> unlockStore = {});
    ~CredentialVault() override;
    static CredentialVault *create(QQmlEngine *, QJSEngine *);
    bool inUse() const { return !m_path.isEmpty(); }
    bool exists() const;
    bool unlocked() const { return bool(m_key); }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    bool automaticUnlock() const { return bool(m_unlockStore); }
    void restore();
    Q_INVOKABLE void unlock(const QString &password);

    QCoro::Task<std::optional<StoredCredentials>> load(Account account) override;
    QCoro::Task<> save(Account account, StoredCredentials credentials) override;
    QCoro::Task<> removeAll() override;

signals:
    void changed();
    void ready();

private:
    void persist(const QHash<Account, StoredCredentials> &items, const std::shared_ptr<SwiftifyVault> &key);
    void fail(const QString &message);
    QString m_path;
    QString m_error;
    bool m_busy = false;
    std::shared_ptr<SwiftifyVault> m_key;
    QHash<Account, StoredCredentials> m_items;
    std::unique_ptr<QLockFile> m_lock;
    std::shared_ptr<VaultUnlockStore> m_unlockStore;
    bool m_restoring = false;
};
} // namespace swiftify
