#include "spotify/VaultUnlockStore.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#ifdef Q_OS_MACOS
#include <Security/Security.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#include <dpapi.h>
#endif

namespace swiftify {
namespace {

#ifdef Q_OS_MACOS
class KeychainUnlockStore final : public VaultUnlockStore {
public:
    explicit KeychainUnlockStore(QByteArray account) : m_account(std::move(account)) {}

    QByteArray load() override
    {
        CFMutableDictionaryRef query = makeQuery();
        CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
        CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
        // Startup must never repeatedly open a system authorization dialog.
        CFDictionarySetValue(query, kSecUseAuthenticationUI, kSecUseAuthenticationUIFail);
        CFTypeRef value = nullptr;
        const OSStatus status = SecItemCopyMatching(query, &value);
        CFRelease(query);
        QByteArray result;
        if (status == errSecSuccess && value && CFGetTypeID(value) == CFDataGetTypeID()) {
            const auto data = static_cast<CFDataRef>(value);
            result = QByteArray(reinterpret_cast<const char *>(CFDataGetBytePtr(data)), CFDataGetLength(data));
        }
        if (value) CFRelease(value);
        return result;
    }

    bool save(const QByteArray &password) override
    {
        CFMutableDictionaryRef query = makeQuery();
        CFDataRef data = CFDataCreate(kCFAllocatorDefault,
            reinterpret_cast<const UInt8 *>(password.constData()), password.size());
        CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(attributes, kSecValueData, data);
        OSStatus status = SecItemUpdate(query, attributes);
        if (status == errSecItemNotFound) {
            CFDictionarySetValue(query, kSecValueData, data);
            status = SecItemAdd(query, nullptr);
        }
        CFRelease(attributes);
        CFRelease(data);
        CFRelease(query);
        return status == errSecSuccess;
    }

private:
    CFMutableDictionaryRef makeQuery() const
    {
        auto query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFStringRef account = CFStringCreateWithBytes(kCFAllocatorDefault,
            reinterpret_cast<const UInt8 *>(m_account.constData()), m_account.size(), kCFStringEncodingUTF8, false);
        CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
        CFDictionarySetValue(query, kSecAttrService, CFSTR("org.swiftify.vault-unlock"));
        CFDictionarySetValue(query, kSecAttrAccount, account);
        CFRelease(account);
        return query;
    }
    QByteArray m_account;
};
#elif defined(Q_OS_WIN)
class DpapiUnlockStore final : public VaultUnlockStore {
public:
    explicit DpapiUnlockStore(QString path) : m_path(std::move(path)) {}

    QByteArray load() override
    {
        const QFileInfo info(m_path);
        QFile file(m_path);
        if (!info.isFile() || info.isSymLink() || info.size() > 16384 || !file.open(QIODevice::ReadOnly)) return {};
        QByteArray encrypted = file.read(16385);
        if (encrypted.size() > 16384) return {};
        DATA_BLOB input{DWORD(encrypted.size()), reinterpret_cast<BYTE *>(encrypted.data())};
        DATA_BLOB output{};
        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return {};
        QByteArray password(reinterpret_cast<const char *>(output.pbData), output.cbData);
        SecureZeroMemory(output.pbData, output.cbData);
        LocalFree(output.pbData);
        return password;
    }

    bool save(const QByteArray &password) override
    {
        if (QFileInfo(m_path).isSymLink()) return false;
        DATA_BLOB input{DWORD(password.size()), reinterpret_cast<BYTE *>(const_cast<char *>(password.constData()))};
        DATA_BLOB output{};
        if (!CryptProtectData(&input, L"Swiftify vault", nullptr, nullptr, nullptr,
                              CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
        QSaveFile file(m_path);
        file.setDirectWriteFallback(false);
        const bool saved = file.open(QIODevice::WriteOnly)
            && file.write(reinterpret_cast<const char *>(output.pbData), output.cbData) == output.cbData
            && file.commit();
        LocalFree(output.pbData);
        return saved;
    }
private:
    QString m_path;
};
#endif

} // namespace

std::shared_ptr<VaultUnlockStore> systemVaultUnlockStore(const QString &vaultPath)
{
#ifdef Q_OS_MACOS
    const QByteArray identity = QCryptographicHash::hash(QFileInfo(vaultPath).absoluteFilePath().toUtf8(),
                                                        QCryptographicHash::Sha256).toHex();
    return std::make_shared<KeychainUnlockStore>(identity);
#elif defined(Q_OS_WIN)
    return std::make_shared<DpapiUnlockStore>(vaultPath + QStringLiteral(".dpapi"));
#else
    Q_UNUSED(vaultPath)
    return {};
#endif
}

} // namespace swiftify
