#pragma once

#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
#include <functional>
#include <memory>

class QQmlEngine;
class QJSEngine;

namespace swiftify {
class Settings;

/// GitHub's published stable releases only. A separate, dependency-free helper
/// verifies signed metadata and packages, then replaces a portable installation.
class Updates : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool supported READ supported CONSTANT FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY changed FINAL)
    Q_PROPERTY(bool available READ available NOTIFY changed FINAL)
    Q_PROPERTY(bool automatic READ automatic WRITE setAutomatic NOTIFY changed FINAL)
    Q_PROPERTY(QString status READ status NOTIFY changed FINAL)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed FINAL)
    Q_PROPERTY(double progress READ progress NOTIFY changed FINAL)

public:
    explicit Updates(Settings *settings, QObject *parent = nullptr);
    ~Updates() override;
    static Updates *create(QQmlEngine *, QJSEngine *);

    bool supported() const { return m_supported; }
    bool busy() const { return m_busy; }
    bool available() const { return m_available; }
    bool automatic() const;
    void setAutomatic(bool enabled);
    QString status() const { return m_status; }
    QString latestVersion() const { return m_version; }
    double progress() const { return m_progress; }

    Q_INVOKABLE void check();
    Q_INVOKABLE void install();
    void scheduleAutomaticCheck();

    // Pure policy helpers, also used by focused tests.
    static QString repository();
    static QString platform();
    static bool newerStable(const QString &candidate, const QString &current);
    static bool trustedDownload(const QUrl &url);
    static QUrl releaseAsset(const QJsonObject &release, const QString &name, const QString &version);
    static QString installationRoot(const QString &executable);

signals:
    void changed();
    void updateFound();
    void restartRequested();

private:
    using Completed = std::function<void()>;
    void fail(const QString &message);
    void fetch(const QUrl &url, const QString &file, qint64 limit, Completed done, int redirects = 0);
    void runHelper(const QString &command, Completed done);
    void inspectManifest();
    void launchInstaller();
    QByteArray read(const QString &name) const;

    Settings *m_settings;
    QNetworkAccessManager m_network;
    QProcess m_helper;
    QTimer m_installPoll;
    QJsonObject m_release;
    QJsonObject m_manifest;
    std::unique_ptr<QTemporaryDir> m_directory;
    QString m_root, m_helperPath, m_work, m_version, m_status;
    bool m_supported = false;
    bool m_busy = false;
    bool m_available = false;
    double m_progress = 0;
    int m_pollCount = 0;
};
} // namespace swiftify
