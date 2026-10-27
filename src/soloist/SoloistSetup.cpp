#include "soloist/SoloistSetup.h"

#include "app/Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>

namespace swiftify::soloist {

namespace {
QString metadataPath()
{
    return installDirectory() + QStringLiteral("/install.json");
}
}

QString installDirectory()
{
    return paths::dataDirectory() + QStringLiteral("/soloist/bin");
}

QString executableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("soloist.exe");
#else
    return QStringLiteral("soloist");
#endif
}

QString installedProgram()
{
    return installDirectory() + QLatin1Char('/') + executableName();
}

QString apiKeyPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/soloist/api-key");
}

QString readApiKey()
{
    const QString environment = qEnvironmentVariable("SOLOIST_API_KEY").trimmed();
    if (!environment.isEmpty()) {
        return environment;
    }
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString dotConfig = QDir::homePath() + QStringLiteral("/.config");
    const QStringList candidates{
        configDir + QStringLiteral("/soloist/api-key"),
        dotConfig + QStringLiteral("/soloist/api-key"),
#ifdef Q_OS_WIN
        configDir + QStringLiteral("/soloist/api-key.txt"),
        dotConfig + QStringLiteral("/soloist/api-key.txt"),
#endif
    };
    for (const QString &path : candidates) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            const QString key = QString::fromUtf8(file.readAll()).trimmed();
            if (!key.isEmpty()) {
                return key;
            }
        }
    }
    return {};
}

bool writeApiKey(const QString &key, QString *error)
{
    const QString path = apiKeyPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) {
            *error = QStringLiteral("Could not create %1").arg(QFileInfo(path).absolutePath());
        }
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(key.trimmed().toUtf8() + '\n');
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
#ifndef Q_OS_WIN
    // the key is a secret of the person's own, like a password
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
    return true;
}

QStringList programSearchPaths()
{
    return {
        installDirectory(),
        QCoreApplication::applicationDirPath(),
        QDir::homePath() + QStringLiteral("/.local/bin"),
#ifdef Q_OS_MACOS
        QStringLiteral("/opt/homebrew/bin"),
        QStringLiteral("/usr/local/bin"),
#elif defined(Q_OS_WIN)
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/soloist"),
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Programs/soloist"),
#endif
    };
}

QString findProgram()
{
    const QString fromEnvironment = qEnvironmentVariable("SWIFTIFY_SOLOIST");
    if (!fromEnvironment.isEmpty()) {
        return fromEnvironment;
    }
    const QString onPath = QStandardPaths::findExecutable(executableName());
    if (!onPath.isEmpty()) {
        return onPath;
    }
    // the app's own download comes first: it is the build swiftify updates
    const QString own = installedProgram();
    if (QFile::exists(own)) {
        return own;
    }
    return QStandardPaths::findExecutable(executableName(), programSearchPaths());
}

QString archiveUrl(QStringView os, QStringView architecture)
{
    const QString base = QStringLiteral("https://soloist-builds.spotifycdn.com/");
    const QString system = os.toString().toLower();
    const QString arch = architecture.toString().toLower();
    if (system == QLatin1StringView("linux")) {
        if (arch == QLatin1StringView("x86_64") || arch == QLatin1StringView("amd64")) {
            return base + QStringLiteral("soloist_release_x86_64.tar.gz");
        }
        if (arch == QLatin1StringView("aarch64") || arch == QLatin1StringView("arm64")) {
            return base + QStringLiteral("soloist_release_arm64.tar.gz");
        }
        if (arch == QLatin1StringView("armv7l") || arch == QLatin1StringView("arm")) {
            return base + QStringLiteral("soloist_release_arm32.tar.gz");
        }
        return {};
    }
    // spotify builds soloist for apple silicon only, not for intel macs
    if (system == QLatin1StringView("macos") && arch == QLatin1StringView("arm64")) {
        return base + QStringLiteral("soloist_release_osx_arm64.tar.gz");
    }
    if (system == QLatin1StringView("windows")
        && (arch == QLatin1StringView("x86_64") || arch == QLatin1StringView("amd64"))) {
        return base + QStringLiteral("soloist_release_windows_x86_64.zip");
    }
    return {};
}

QString currentArchitecture()
{
    return QSysInfo::currentCpuArchitecture().toLower();
}

QString archiveUrl()
{
#ifdef Q_OS_LINUX
    const QString os = QStringLiteral("linux");
#elif defined(Q_OS_MACOS)
    const QString os = QStringLiteral("macos");
#elif defined(Q_OS_WIN)
    const QString os = QStringLiteral("windows");
#else
    return {};
#endif
    return archiveUrl(os, currentArchitecture());
}

bool platformSupported()
{
    return !archiveUrl().isEmpty();
}

InstallMetadata readMetadata()
{
    QFile file(metadataPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    InstallMetadata metadata;
    metadata.installedAt = QDateTime::fromString(json.value(QStringLiteral("installedAt")).toString(), Qt::ISODate);
    metadata.version = json.value(QStringLiteral("version")).toString();
    metadata.url = json.value(QStringLiteral("url")).toString();
    metadata.valid = metadata.installedAt.isValid();
    return metadata;
}

void writeMetadata(const InstallMetadata &metadata)
{
    QDir().mkpath(installDirectory());
    QSaveFile file(metadataPath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    const QJsonObject json{
        {QStringLiteral("installedAt"), metadata.installedAt.toString(Qt::ISODate)},
        {QStringLiteral("version"), metadata.version},
        {QStringLiteral("url"), metadata.url},
    };
    file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
    file.commit();
}

bool needsRefresh(const InstallMetadata &metadata, const QDateTime &now)
{
    return metadata.valid && metadata.installedAt.daysTo(now) >= 80;
}

} // namespace swiftify::soloist
