#include "system/Updates.h"
#include "app/Settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVersionNumber>
#include <memory>

namespace swiftify {
namespace {
Updates *s_instance = nullptr;
QString releasePrefix() { return QStringLiteral("https://github.com/%1/releases/download/").arg(Updates::repository()); }
QUrl releaseApi() { return QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(Updates::repository())); }
}

QString Updates::repository()
{
    // owner/name of the GitHub repository whose published releases are offered
    return QStringLiteral(SWIFTIFY_UPDATE_REPOSITORY);
}

QString Updates::platform()
{
#ifdef Q_OS_MACOS
    const QString os = QStringLiteral("macos");
#elif defined(Q_OS_WIN)
    const QString os = QStringLiteral("windows");
#else
    const QString os = QStringLiteral("linux");
#endif
#if defined(Q_PROCESSOR_ARM_64)
    return os + QStringLiteral("-aarch64");
#elif defined(Q_PROCESSOR_X86_64)
    return os + QStringLiteral("-x86_64");
#else
    return {};
#endif
}

bool Updates::newerStable(const QString &candidate, const QString &current)
{
    static const QRegularExpression stable(QStringLiteral("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"));
    return stable.match(candidate).hasMatch() && stable.match(current).hasMatch()
        && QVersionNumber::fromString(candidate) > QVersionNumber::fromString(current);
}

bool Updates::trustedDownload(const QUrl &url)
{
    if (url.scheme() != QLatin1StringView("https") || !url.userInfo().isEmpty()
        || (url.port(-1) != -1 && url.port() != 443)) return false;
    if (url == releaseApi()) return true;
    if (url.host() == QLatin1StringView("github.com")) return url.toString().startsWith(releasePrefix());
    // GitHub release assets redirect to these CDNs. Never forward credentials
    // or cookies, never follow arbitrary or plaintext redirects.
    return url.host() == QLatin1StringView("release-assets.githubusercontent.com")
        || url.host() == QLatin1StringView("objects.githubusercontent.com");
}

QUrl Updates::releaseAsset(const QJsonObject &release, const QString &name, const QString &version)
{
    const QString expected = releasePrefix() + QStringLiteral("v") + version + QLatin1Char('/') + name;
    QUrl result;
    for (const auto &value : release.value(QStringLiteral("assets")).toArray()) {
        const auto asset = value.toObject();
        if (asset.value(QStringLiteral("name")).toString() != name) continue;
        const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
        if (!result.isEmpty() || url.toString() != expected || !trustedDownload(url)) return {};
        result = url;
    }
    return result;
}

QString Updates::installationRoot(const QString &executable)
{
    QDir directory(QFileInfo(executable).absolutePath());
#ifdef Q_OS_MACOS
    if (directory.dirName() != QLatin1StringView("MacOS") || !directory.cdUp()
        || directory.dirName() != QLatin1StringView("Contents") || !directory.cdUp()
        || !directory.dirName().endsWith(QLatin1StringView(".app"))) return {};
#else
    if (directory.dirName() != QLatin1StringView("bin") || !directory.cdUp()) return {};
#endif
    return directory.canonicalPath();
}

Updates::Updates(Settings *settings, QObject *parent) : QObject(parent), m_settings(settings)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_root = installationRoot(QCoreApplication::applicationFilePath());
    m_helperPath = QCoreApplication::applicationDirPath() + QStringLiteral("/swiftify-update-helper");
#ifdef Q_OS_WIN
    m_helperPath += QStringLiteral(".exe");
#endif
#ifdef Q_OS_MACOS
    QFile marker(m_root + QStringLiteral("/Contents/Resources/swiftify-portable.json"));
#else
    QFile marker(m_root + QStringLiteral("/.swiftify-portable.json"));
#endif
    QJsonObject identity;
    if (!m_root.isEmpty() && marker.open(QIODevice::ReadOnly) && marker.size() <= 1024)
        identity = QJsonDocument::fromJson(marker.readAll()).object();
    m_supported = !QStringLiteral(SWIFTIFY_UPDATE_PUBLIC_KEY).isEmpty() && !platform().isEmpty()
        && QFileInfo(m_helperPath).isExecutable() && identity.value(QStringLiteral("product")) == QLatin1StringView("Swiftify")
        && identity.value(QStringLiteral("format")).toInt() == 1
        && identity.value(QStringLiteral("target")).toString() == platform();
    m_status = m_supported ? tr("Check for a new Swiftify release.")
        : tr("In-app installation requires an official portable build with an update verification key. Use your package manager for system installations.");
    m_installPoll.setInterval(200);
    connect(&m_installPoll, &QTimer::timeout, this, [this] {
        QFile error(m_work + QStringLiteral("/error.txt"));
        if (error.exists()) { m_installPoll.stop(); fail(tr("The installer could not prepare the update. Your installation was not changed.")); }
        else if (QFileInfo::exists(m_work + QStringLiteral("/ready"))) {
            m_installPoll.stop();
            QFile approval(m_work + QStringLiteral("/approved"));
            if (!approval.open(QIODevice::WriteOnly) || approval.write("approved") != 8) {
                fail(tr("Could not approve the update installation.")); return;
            }
            approval.close();
            m_status = tr("Restarting to install the update…"); emit changed(); emit restartRequested();
        } else if (++m_pollCount > 4500) {
            // The helper will not replace a running application. Its independent
            // wait timeout prevents a delayed install after this UI gives up.
            m_installPoll.stop(); fail(tr("The installer did not become ready. Your installation was not changed."));
        }
    });
}

Updates::~Updates()
{
    m_installPoll.stop();
    m_helper.disconnect(this);
    if (m_helper.state() != QProcess::NotRunning) {
        m_helper.kill();
        m_helper.waitForFinished(1000);
    }
    for (auto *reply : m_network.findChildren<QNetworkReply *>()) {
        reply->disconnect(this);
        reply->abort();
    }
    if (s_instance == this) s_instance = nullptr;
}

Updates *Updates::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool Updates::automatic() const
{
    return m_settings && m_settings->value(QStringLiteral("updates/automatic"), true).toBool();
}

bool Updates::installAutomatically() const
{
    return m_settings && m_settings->value(QStringLiteral("updates/install"), true).toBool();
}

void Updates::setInstallAutomatically(bool enabled)
{
    if (!m_settings || installAutomatically() == enabled) return;
    m_settings->setValue(QStringLiteral("updates/install"), enabled);
    emit changed();
}

void Updates::setAutomatic(bool enabled)
{
    if (!m_settings || automatic() == enabled) return;
    m_settings->setValue(QStringLiteral("updates/automatic"), enabled);
    emit changed();
    if (enabled) scheduleAutomaticCheck();
}

void Updates::scheduleAutomaticCheck()
{
    const auto checkIfDue = [this] {
        if (!automatic() || !supported() || m_busy || m_available) return;
        const auto last = m_settings->value(QStringLiteral("updates/lastCheck")).toDateTime();
        if (!last.isValid() || last.secsTo(QDateTime::currentDateTimeUtc()) >= 86400) check();
    };
    QTimer::singleShot(10000, this, checkIfDue);
    // an app left open for days looks again too, once a day is up
    if (!m_dueCheck.isActive()) {
        m_dueCheck.setInterval(3600 * 1000);
        connect(&m_dueCheck, &QTimer::timeout, this, checkIfDue);
        m_dueCheck.start();
    }
}

void Updates::fail(const QString &message)
{
    m_busy = false; m_progress = 0; m_status = message; emit changed();
}

QByteArray Updates::read(const QString &name) const
{
    QFile file(m_work + QLatin1Char('/') + name);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return {};
    return file.readAll();
}

void Updates::fetch(const QUrl &url, const QString &file, qint64 limit, Completed done, int redirects)
{
    if (!trustedDownload(url) || redirects > 5) { fail(tr("The update download URL was refused.")); return; }
    auto output = std::make_shared<QSaveFile>(m_work + QLatin1Char('/') + file);
    if (!output->open(QIODevice::WriteOnly)) { fail(tr("Could not write the update download.")); return; }
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Swiftify-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json, application/octet-stream");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setTransferTimeout(30000);
    auto *reply = m_network.get(request);
    reply->setReadBufferSize(256 * 1024);
    const auto total = std::make_shared<qint64>(0);
    const auto consume = [reply, output, total, limit] {
        // Redirect bodies are ignored; final response bytes are bounded on disk.
        if (reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) { reply->readAll(); return; }
        const QByteArray bytes = reply->readAll();
        *total += bytes.size();
        if (*total > limit || output->write(bytes) != bytes.size()) reply->abort();
    };
    connect(reply, &QNetworkReply::readyRead, this, consume);
    connect(reply, &QNetworkReply::downloadProgress, this, [this, file](qint64 received, qint64 totalSize) {
        if (file == QLatin1StringView("package.zip") && totalSize > 0) {
            m_progress = qBound(0., double(received) / totalSize, 1.); emit changed();
        }
    });
    // A peer sending one byte every 29 seconds cannot keep a check alive forever.
    QTimer::singleShot(15 * 60 * 1000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, output, done, consume, url, file, limit, redirects] {
        consume(); reply->deleteLater();
        const QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status < 400 && !redirect.isEmpty()) {
            output->cancelWriting(); fetch(url.resolved(redirect), file, limit, done, redirects + 1); return;
        }
        if (reply->error() != QNetworkReply::NoError || status != 200 || !output->commit()) {
            fail(tr("Could not download the update. Check your connection or try later (GitHub may be rate limiting requests).")); return;
        }
        done();
    });
}

void Updates::check()
{
    if (m_busy || !supported()) return;
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    m_directory = std::make_unique<QTemporaryDir>(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/swiftify-update-XXXXXX"));
    if (!m_directory->isValid()) {
        fail(tr("Could not create an update directory. Try again.")); return;
    }
    m_work = m_directory->path();
    m_busy = true; m_available = false; m_version.clear(); m_manifest = {}; m_progress = 0;
    m_status = tr("Checking for updates…"); emit changed();
    fetch(releaseApi(), QStringLiteral("release.json"), 1024 * 1024, [this] {
        m_release = QJsonDocument::fromJson(read(QStringLiteral("release.json"))).object();
        const QString tag = m_release.value(QStringLiteral("tag_name")).toString();
        const QString version = tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : QString();
        if (m_release.value(QStringLiteral("draft")).toBool(true)
            || m_release.value(QStringLiteral("prerelease")).toBool(true) || version.isEmpty()) {
            fail(tr("GitHub returned an invalid stable release.")); return;
        }
        if (m_settings) m_settings->setValue(QStringLiteral("updates/lastCheck"), QDateTime::currentDateTimeUtc());
        if (!newerStable(version, QStringLiteral(SWIFTIFY_VERSION))) {
            m_busy = false; m_status = tr("Swiftify is up to date."); emit changed(); return;
        }
        m_version = version;
        const QString name = QStringLiteral("swiftify-update-") + platform();
        const QUrl manifest = releaseAsset(m_release, name + QStringLiteral(".json"), version);
        const QUrl signature = releaseAsset(m_release, name + QStringLiteral(".sig"), version);
        if (manifest.isEmpty() || signature.isEmpty()) { fail(tr("This release has no signed update for this platform.")); return; }
        fetch(manifest, QStringLiteral("manifest.json"), 16384, [this, signature] {
            fetch(signature, QStringLiteral("manifest.sig"), 256, [this] { inspectManifest(); });
        });
    });
}

void Updates::runHelper(const QString &command, Completed done)
{
    m_helper.disconnect(this);
    connect(&m_helper, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        fail(tr("The update verifier could not run."));
    });
    connect(&m_helper, &QProcess::finished, this, [this, done](int code, QProcess::ExitStatus exitStatus) {
        if (code != 0 || exitStatus != QProcess::NormalExit) {
            fail(tr("Update signature or package verification failed. Nothing was installed.")); return;
        }
        done();
    });
    m_helper.setProgram(m_helperPath);
    m_helper.setArguments({command, m_work});
    m_helper.start();
}

void Updates::inspectManifest()
{
    runHelper(QStringLiteral("inspect"), [this] {
        m_manifest = QJsonDocument::fromJson(m_helper.readAllStandardOutput()).object();
        const QString archive = m_manifest.value(QStringLiteral("archive")).toString();
        if (m_manifest.value(QStringLiteral("version")).toString() != m_version
            || releaseAsset(m_release, archive, m_version).isEmpty()) {
            fail(tr("The signed update does not match this release.")); return;
        }
        m_busy = false; m_available = true;
        m_status = tr("Swiftify %1 is available.").arg(m_version); emit changed(); emit updateFound();
    });
}

void Updates::install()
{
    if (m_busy || !m_available || !supported()) return;
    m_busy = true; m_progress = 0; m_status = tr("Downloading and verifying the update…"); emit changed();
    const QString archive = m_manifest.value(QStringLiteral("archive")).toString();
    const qint64 size = m_manifest.value(QStringLiteral("size")).toInteger();
    if (size < 1 || size > qint64(2) * 1024 * 1024 * 1024) { fail(tr("Invalid update package size.")); return; }
    fetch(releaseAsset(m_release, archive, m_version), QStringLiteral("package.zip"), size, [this] {
        runHelper(QStringLiteral("verify"), [this] { launchInstaller(); });
    });
}

void Updates::launchInstaller()
{
    m_available = false; // A detached installer is never retried using the same work directory.
    const QString helper = m_work + QLatin1Char('/') + QFileInfo(m_helperPath).fileName();
    // The installer must outlive Qt and remain outside the tree it replaces.
    if (!QFile::copy(m_helperPath, helper)
        || !QFile::setPermissions(helper, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
        fail(tr("Could not stage the update installer.")); return;
    }
    QProcess process;
    process.setProgram(helper);
    process.setArguments({QStringLiteral("install"), m_work, m_root, QString::number(QCoreApplication::applicationPid())});
    process.setWorkingDirectory(m_work);
    process.setStandardOutputFile(m_work + QStringLiteral("/installer.log"));
    process.setStandardErrorFile(m_work + QStringLiteral("/installer.log"), QIODevice::Append);
    m_directory->setAutoRemove(false);
    if (!process.startDetached()) {
        m_directory->setAutoRemove(true);
        fail(tr("Could not start the update installer.")); return;
    }
    m_status = tr("Preparing the update. Swiftify will restart when it is ready…"); emit changed();
    m_pollCount = 0; m_installPoll.start();
}
} // namespace swiftify
