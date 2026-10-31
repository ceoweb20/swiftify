/*
 * Swiftify, a spotify player for desktop
 * Copyright (C) 2026  shred0s
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
        setvbuf(stdout, nullptr, _IONBF, 0);
    }
#endif

#ifdef Q_OS_WIN
    HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hStdErr != INVALID_HANDLE_VALUE && hStdErr != nullptr) {
        const DWORD type = GetFileType(hStdErr);
        if (type == FILE_TYPE_CHAR || type == FILE_TYPE_PIPE) {
            HANDLE hConsole = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), hStdErr,
                                GetCurrentProcess(), &hConsole,
                                0, FALSE, DUPLICATE_SAME_ACCESS)) {
                s_consoleFd = _open_osfhandle(reinterpret_cast<intptr_t>(hConsole), _O_TEXT);
            }
        }
    }

    fflush(stderr);
    fflush(stdout);

    // In a GUI subsystem application (WIN32_EXECUTABLE), stderr and stdout do not have
    // backing CRT descriptors by default (_fileno returns -2). We must initialize them
    // (e.g. to NUL) before _dup2 can duplicate our log file descriptor onto them.
    FILE *dummy = nullptr;
    if (_fileno(stderr) < 0) {
        _wfreopen_s(&dummy, L"NUL", L"w", stderr);
    }
    if (_fileno(stdout) < 0) {
        _wfreopen_s(&dummy, L"NUL", L"w", stdout);
    }

    const std::wstring wpath = logFile.toStdWString();
    FILE *fp = _wfsopen(wpath.c_str(), L"w", _SH_DENYNO);
    if (!fp) {
        fp = _wfopen(wpath.c_str(), L"w");
    }
    if (fp) {
        const int fd = _fileno(fp);
        const int errFd = _fileno(stderr);
        const int outFd = _fileno(stdout);
        if (errFd >= 0) {
            _dup2(fd, errFd);
        }
        if (outFd >= 0) {
            _dup2(fd, outFd);
        }
        HANDLE hErr = reinterpret_cast<HANDLE>(_get_osfhandle(errFd >= 0 ? errFd : fd));
        HANDLE hOut = reinterpret_cast<HANDLE>(_get_osfhandle(outFd >= 0 ? outFd : fd));
        if (hErr != INVALID_HANDLE_VALUE && hErr != nullptr) {
            SetStdHandle(STD_ERROR_HANDLE, hErr);
        }
        if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
            SetStdHandle(STD_OUTPUT_HANDLE, hOut);
        }
        fclose(fp);
        setvbuf(stderr, nullptr, _IONBF, 0);
        setvbuf(stdout, nullptr, _IONBF, 0);
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
