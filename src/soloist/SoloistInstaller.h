#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

/// downloads spotify soloist from spotify's own cdn at the person's request.
/// soloist's terms forbid shipping its binaries with an app, so swiftify
/// installs it on the machine instead: linux x86_64/aarch64/armv7, apple
/// silicon macos, and 64-bit windows get the build for their platform.
/// every user needs their own api key from the developer dashboard.
class SoloistInstaller : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Soloist)
    QML_SINGLETON
    Q_PROPERTY(bool supported READ supported NOTIFY refreshed FINAL)
    Q_PROPERTY(bool installed READ installed NOTIFY refreshed FINAL)
    Q_PROPERTY(QString programPath READ programPath NOTIFY refreshed FINAL)
    Q_PROPERTY(QString version READ version NOTIFY refreshed FINAL)
    Q_PROPERTY(bool apiKeyConfigured READ apiKeyConfigured NOTIFY refreshed FINAL)
    Q_PROPERTY(bool updateRecommended READ updateRecommended NOTIFY refreshed FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged FINAL)
    /// 0..1 while the archive downloads, -1 while it extracts and verifies
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged FINAL)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged FINAL)

public:
    explicit SoloistInstaller(QObject *parent = nullptr);
    ~SoloistInstaller() override;
    static SoloistInstaller *create(QQmlEngine *, QJSEngine *);

    bool supported() const { return m_supported; }
    bool installed() const { return !m_programPath.isEmpty(); }
    QString programPath() const { return m_programPath; }
    QString version() const { return m_version; }
    bool apiKeyConfigured() const { return m_apiKeyConfigured; }
    bool updateRecommended() const { return m_updateRecommended; }
    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    QString status() const { return m_status; }

    Q_INVOKABLE void install();
    Q_INVOKABLE bool saveApiKey(const QString &key);
    Q_INVOKABLE void refresh();

signals:
    void refreshed();
    void busyChanged();
    void progressChanged();
    void statusChanged();
    /// a binary or a key changed, so the playback core re-reads its config
    void configurationChanged();

private:
    void setBusy(bool busy);
    void setProgress(double progress);
    void setStatus(const QString &status);
    void fail(const QString &message);
    void extract();
    void installBinary();
    void verify(const QString &program);
    void finish(const QString &version);

    QNetworkAccessManager m_network;
    QNetworkReply *m_reply = nullptr;
    QTemporaryFile m_archive;
    std::unique_ptr<QTemporaryDir> m_extractDir;
    QProcess m_process;

    bool m_supported = false;
    QString m_programPath;
    QString m_version;
    bool m_apiKeyConfigured = false;
    bool m_updateRecommended = false;
    bool m_busy = false;
    double m_progress = -1;
    QString m_status;
};

} // namespace swiftify
