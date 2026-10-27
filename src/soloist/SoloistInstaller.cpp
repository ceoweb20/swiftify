#include "soloist/SoloistInstaller.h"

#include "soloist/SoloistSetup.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlEngine>
#include <QScopeGuard>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcSoloistInstall, "swiftify.soloist.install")

namespace swiftify {

namespace {
SoloistInstaller *s_instance = nullptr;
}

SoloistInstaller::SoloistInstaller(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    refresh();
}

SoloistInstaller::~SoloistInstaller()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

SoloistInstaller *SoloistInstaller::create(QQmlEngine *, QJSEngine *)
{
    if (!s_instance) {
        new SoloistInstaller(qApp);
    }
    // main owns the installer, not the qml engine
    QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
    return s_instance;
}

void SoloistInstaller::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void SoloistInstaller::setProgress(double progress)
{
    if (!qFuzzyCompare(m_progress, progress)) {
        m_progress = progress;
        emit progressChanged();
    }
}

void SoloistInstaller::setStatus(const QString &status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void SoloistInstaller::refresh()
{
    m_supported = soloist::platformSupported();
    m_programPath = soloist::findProgram();
    m_apiKeyConfigured = !soloist::readApiKey().isEmpty();
    const soloist::InstallMetadata metadata = soloist::readMetadata();
    m_version = metadata.version;
    m_updateRecommended = soloist::needsRefresh(metadata, QDateTime::currentDateTime());
    emit refreshed();
}

bool SoloistInstaller::saveApiKey(const QString &key)
{
    QString error;
    if (key.trimmed().isEmpty()) {
        setStatus(QStringLiteral("Paste the Soloist API key from your Spotify developer dashboard."));
        return false;
    }
    if (!soloist::writeApiKey(key, &error)) {
        setStatus(QStringLiteral("Could not save the API key: %1").arg(error));
        return false;
    }
    setStatus(QStringLiteral("API key saved."));
    refresh();
    emit configurationChanged();
    return true;
}

void SoloistInstaller::install()
{
    if (m_busy) {
        return;
    }
    const QString url = soloist::archiveUrl();
    if (url.isEmpty()) {
        setStatus(QStringLiteral("Spotify Soloist has no build for this computer. "
                                 "It runs on Linux, Apple Silicon Macs and 64-bit Windows."));
        return;
    }
    m_extractDir = std::make_unique<QTemporaryDir>();
    if (!m_extractDir->isValid()) {
        setStatus(QStringLiteral("Could not create a temporary directory."));
        return;
    }
    if (!m_archive.open()) {
        setStatus(m_archive.errorString());
        return;
    }
    // a reinstall starts with an empty archive, not last time's bytes
    m_archive.resize(0);
    setBusy(true);
    setProgress(0);
    setStatus(QStringLiteral("Downloading Soloist from Spotify…"));

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(180000);
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_archive.isOpen()) {
            m_archive.write(m_reply->readAll());
        }
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            setProgress(double(received) / double(total));
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        const auto finishReply = qScopeGuard([this] {
            m_reply->deleteLater();
            m_reply = nullptr;
        });
        m_archive.write(m_reply->readAll());
        m_archive.flush();
        if (m_reply->error() != QNetworkReply::NoError) {
            fail(QStringLiteral("Could not download Soloist: %1").arg(m_reply->errorString()));
            return;
        }
        extract();
    });
}

void SoloistInstaller::extract()
{
    setProgress(-1);
    setStatus(QStringLiteral("Unpacking Soloist…"));
    // bsdtar (macos, and tar.exe on windows) reads zip and tar.gz alike;
    // gnu tar on linux detects the gzip itself
    const QString tar = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (tar.isEmpty()) {
        fail(QStringLiteral("The tar utility is needed to unpack Soloist."));
        return;
    }
    m_process.disconnect(this);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) fail(m_process.errorString());
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        if (code != 0) {
            fail(QStringLiteral("Could not unpack Soloist: %1").arg(QString::fromUtf8(m_process.readAll())));
            return;
        }
        installBinary();
    });
    m_process.start(tar, {QStringLiteral("-xf"), m_archive.fileName(),
                          QStringLiteral("-C"), m_extractDir->path()});
}

void SoloistInstaller::installBinary()
{
    QString extracted;
    QDirIterator it(m_extractDir->path(), {soloist::executableName()},
                    QDir::Files | QDir::Executable, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        extracted = it.next();
    }
    if (extracted.isEmpty()) {
        // windows archives do not always carry the executable bit
        QDirIterator any(m_extractDir->path(), {soloist::executableName()}, QDir::Files, QDirIterator::Subdirectories);
        if (any.hasNext()) {
            extracted = any.next();
        }
    }
    if (extracted.isEmpty()) {
        fail(QStringLiteral("The Soloist archive did not contain %1.").arg(soloist::executableName()));
        return;
    }
    QDir().mkpath(soloist::installDirectory());
    const QString destination = soloist::installedProgram();
    // a running soloist holds its exe on windows, so stop playback first
    QFile::remove(destination);
    if (!QFile::copy(extracted, destination)) {
        fail(QStringLiteral("Could not install Soloist into %1. Stop Soloist and try again.")
                 .arg(soloist::installDirectory()));
        return;
    }
    QFile::setPermissions(destination, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);
    setStatus(QStringLiteral("Checking the Soloist build…"));
    verify(destination);
}

void SoloistInstaller::verify(const QString &program)
{
    m_process.disconnect(this);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) fail(m_process.errorString());
    });
    connect(&m_process, &QProcess::finished, this, [this, program](int code, QProcess::ExitStatus) {
        const QString output = QString::fromUtf8(m_process.readAll()).trimmed();
        if (code != 0) {
            fail(QStringLiteral("The installed Soloist did not run: %1").arg(output));
            return;
        }
        finish(output.section(u'\n', 0, 0));
    });
    m_process.start(program, {QStringLiteral("--version")});
}

void SoloistInstaller::finish(const QString &version)
{
    m_archive.close();
    soloist::InstallMetadata metadata;
    metadata.installedAt = QDateTime::currentDateTime();
    metadata.version = version;
    metadata.url = soloist::archiveUrl();
    soloist::writeMetadata(metadata);

    setBusy(false);
    setProgress(-1);
    setStatus(QStringLiteral("Soloist installed. Pick “Swiftify” in Spotify's devices once to pair it."));
    refresh();
    emit configurationChanged();
    qCInfo(lcSoloistInstall).noquote() << "Installed" << version;
}

void SoloistInstaller::fail(const QString &message)
{
    m_archive.close();
    setBusy(false);
    setProgress(-1);
    setStatus(message);
    qCWarning(lcSoloistInstall).noquote() << message;
}

} // namespace swiftify
