#pragma once

#include <QByteArray>
#include <QString>
#include <memory>

namespace swiftify {

/// Stores the vault password using the operating system, never preferences or
/// an unprotected sidecar. Tests inject a memory-only implementation.
class VaultUnlockStore {
public:
    virtual ~VaultUnlockStore() = default;
    virtual QByteArray load() = 0;
    virtual bool save(const QByteArray &password) = 0;
};

std::shared_ptr<VaultUnlockStore> systemVaultUnlockStore(const QString &vaultPath);

} // namespace swiftify
