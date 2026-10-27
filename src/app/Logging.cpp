#include "app/Logging.h"
#include "app/Paths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>

#include <cstdio>
#include <cstdlib>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <share.h>
#endif

#ifndef SWIFTIFY_VERSION
#define SWIFTIFY_VERSION "0.0.0"
#endif

namespace swiftify::logging {

namespace {

bool s_active = false;
int s_consoleFd = -1;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const char *level = "DEBUG";
    switch (type) {
    case QtDebugMsg:    level = "DEBUG"; break;
    case QtInfoMsg:     level = "INFO "; break;
    case QtWarningMsg:  level = "WARN "; break;
    case QtCriticalMsg: level = "CRIT "; break;
    case QtFatalMsg:    level = "FATAL"; break;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
    const char *category = (context.category && *context.category) ? context.category : "default";

    const QString line = QStringLiteral("[%1] [%2] [%3] %4\n")
        .arg(timestamp, QString::fromLatin1(level), QString::fromUtf8(category), message);
    const QByteArray utf8 = line.toUtf8();

    fwrite(utf8.constData(), 1, utf8.size(), stderr);
    fflush(stderr);

    if (s_consoleFd >= 0) {
#ifdef Q_OS_WIN
        _write(s_consoleFd, utf8.constData(), static_cast<unsigned int>(utf8.size()));
#else
        [[maybe_unused]] const ssize_t written = ::write(s_consoleFd, utf8.constData(), utf8.size());
#endif
    }

    if (type == QtFatalMsg) {
        abort();
    }
}

} // namespace

void applyEnvironmentVariables()
{
    const auto setIfUnset = [](const char *name, const char *value) {
        if (!qEnvironmentVariableIsSet(name)) {
            qputenv(name, value);
        }
    };

    setIfUnset("QT_FORCE_STDERR_LOGGING", "1");
    setIfUnset("QT_LOGGING_RULES", "swiftify.*=true");
    setIfUnset("RUST_LOG", "info,spotify_core=debug,librespot=info");
    setIfUnset("QT_MESSAGE_PATTERN", "[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] [%{category}] %{message}");

#ifdef Q_OS_WIN
    setIfUnset("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
#endif
}

void setupLogging()
{
    QSettings store(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("Swiftify"), QStringLiteral("Swiftify"));
    const bool enabled = store.value(QStringLiteral("loggingEnabled"), false).toBool()
        || qEnvironmentVariableIsSet("SWIFTIFY_LOG");

    if (!enabled) {
        return;
    }

    applyEnvironmentVariables();

    const QString logDir = paths::logDirectory();
    const QString logFile = paths::logFilePath();
    const QString prevLogFile = logDir + QStringLiteral("/swiftify.prev.log");

    if (QFile::exists(logFile)) {
        if (QFile::exists(prevLogFile)) {
            QFile::remove(prevLogFile);
        }
        QFile::rename(logFile, prevLogFile);
    }

#ifdef Q_OS_UNIX
    if (isatty(STDERR_FILENO)) {
        s_consoleFd = dup(STDERR_FILENO);
    }

    fflush(stderr);
    fflush(stdout);

    FILE *fp = fopen(logFile.toUtf8().constData(), "w");
    if (fp) {
        const int fd = fileno(fp);
        dup2(fd, STDERR_FILENO);
        dup2(fd, STDOUT_FILENO);
        fclose(fp);
        setvbuf(stderr, nullptr, _IONBF, 0);
        setvbuf(stdout, nullptr, _IOLBF, 0);
    }
#endif

#ifdef Q_OS_WIN
    if (_isatty(_fileno(stderr))) {
        s_consoleFd = _dup(_fileno(stderr));
    }

    fflush(stderr);
    fflush(stdout);

    const std::wstring wpath = logFile.toStdWString();
    FILE *fp = _wfsopen(wpath.c_str(), L"w", _SH_DENYNO);
    if (!fp) {
        fp = _wfopen(wpath.c_str(), L"w");
    }
    if (fp) {
        const int fd = _fileno(fp);
        _dup2(fd, _fileno(stderr));
        _dup2(fd, _fileno(stdout));
        HANDLE hErr = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr)));
        HANDLE hOut = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout)));
        if (hErr != INVALID_HANDLE_VALUE) {
            SetStdHandle(STD_ERROR_HANDLE, hErr);
        }
        if (hOut != INVALID_HANDLE_VALUE) {
            SetStdHandle(STD_OUTPUT_HANDLE, hOut);
        }
        fclose(fp);
        setvbuf(stderr, nullptr, _IONBF, 0);
        setvbuf(stdout, nullptr, _IOLBF, 0);
    }
#endif

    qInstallMessageHandler(messageHandler);
    s_active = true;

    fprintf(stderr, "============================================================\n");
    fprintf(stderr, "Swiftify %s logging started at %s\n",
            SWIFTIFY_VERSION,
            QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8().constData());
#if defined(Q_OS_WIN)
    fprintf(stderr, "Operating System: Windows\n");
#elif defined(Q_OS_MACOS)
    fprintf(stderr, "Operating System: macOS\n");
#elif defined(Q_OS_LINUX)
    fprintf(stderr, "Operating System: Linux\n");
#else
    fprintf(stderr, "Operating System: Unknown\n");
#endif
    fprintf(stderr, "Log file: %s\n", logFile.toUtf8().constData());
    fprintf(stderr, "Log directory: %s\n", logDir.toUtf8().constData());
    fprintf(stderr, "Qt Version: %s\n", qVersion());
    fprintf(stderr, "QT_LOGGING_RULES: %s\n", qgetenv("QT_LOGGING_RULES").constData());
    fprintf(stderr, "RUST_LOG: %s\n", qgetenv("RUST_LOG").constData());
    fprintf(stderr, "============================================================\n\n");
    fflush(stderr);
}

bool isActive()
{
    return s_active;
}

} // namespace swiftify::logging
