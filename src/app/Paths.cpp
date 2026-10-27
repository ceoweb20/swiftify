#include "app/Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace swiftify::paths {

namespace {

QString ensured(const QString &path)
{
    QDir().mkpath(path);
    return path;
}

} // namespace

QString dataDirectory()
{
    return ensured(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                   + QStringLiteral("/Swiftify"));
}

QString cacheDirectory()
{
    return ensured(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                   + QStringLiteral("/Swiftify"));
}

QString logDirectory()
{
    return ensured(dataDirectory() + QStringLiteral("/logs"));
}

QString logFilePath()
{
    return logDirectory() + QStringLiteral("/swiftify.log");
}

} // namespace swiftify::paths
