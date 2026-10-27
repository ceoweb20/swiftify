#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace swiftify::soloist {

/// where swiftify keeps the soloist build it downloads itself, and the key it
/// is given; nothing here ships with the app, spotify forbids redistributing
/// soloist binaries
QString installDirectory();
QString installedProgram();
QString apiKeyPath();
QString readApiKey();
bool writeApiKey(const QString &key, QString *error = nullptr);
/// besides the PATH: the app's own install, and where soloist's guide puts it
QStringList programSearchPaths();
QString findProgram();
QString executableName();

/// the official archive for an os ("linux", "macos", "windows") and a machine
/// architecture; empty where spotify has no build, such as an intel mac
QString archiveUrl(QStringView os, QStringView architecture);
QString archiveUrl();
bool platformSupported();
QString currentArchitecture();

struct InstallMetadata {
    QDateTime installedAt;
    QString version;
    QString url;
    bool valid = false;
};

InstallMetadata readMetadata();
void writeMetadata(const InstallMetadata &metadata);
/// soloist builds exit with code 10 once they are 90 days old, so the app's
/// own build is replaced before that; `installedAt` is when it was downloaded
bool needsRefresh(const InstallMetadata &metadata, const QDateTime &now);

} // namespace swiftify::soloist
