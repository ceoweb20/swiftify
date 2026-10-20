#pragma once

#include <QString>

#include <exception>
#include <utility>

namespace swiftify {

/// a failure the user may be shown. the message is already user-facing, worded
/// as the swift app words it.
class SpotifyError : public std::exception {
public:
    enum class Kind {
        InvalidUrl,
        InvalidResponse,
        InvalidData,
        Unauthorized,
        Forbidden,
        RateLimited,
        RequestFailed,
        OAuth,
        /// spotify will not refresh this login again (invalid_grant): it was
        /// revoked, or rotated and an old copy reused. only signing in again
        /// brings it back
        LoginRevoked,
        Credentials,
        Session,
        Core,
        /// the core gave up on a request because a newer one replaced it
        Superseded,
        /// spotify will not give this account the track
        Unavailable,
    };

    SpotifyError(Kind kind, QString message, int retryAfterSeconds = -1, int httpStatus = 0)
        : m_kind(kind)
        , m_message(std::move(message))
        , m_retryAfterSeconds(retryAfterSeconds)
        , m_httpStatus(httpStatus)
        , m_utf8(m_message.toUtf8())
    {
    }

    Kind kind() const { return m_kind; }
    const QString &message() const { return m_message; }
    int retryAfterSeconds() const { return m_retryAfterSeconds; }
    int httpStatus() const { return m_httpStatus; }
    bool shouldRefreshAccessToken() const { return m_kind == Kind::Unauthorized; }
    const char *what() const noexcept override { return m_utf8.constData(); }

    static SpotifyError invalidData(const QString &detail)
    {
        return {Kind::InvalidData, QStringLiteral("Could not read Spotify's response: %1").arg(detail)};
    }
    static SpotifyError requestFailed(const QString &detail, int httpStatus = 0)
    {
        return {Kind::RequestFailed, QStringLiteral("Spotify API request failed: %1").arg(detail), -1, httpStatus};
    }

private:
    Kind m_kind;
    QString m_message;
    int m_retryAfterSeconds;
    int m_httpStatus;
    QByteArray m_utf8;
};

/// a request that was withdrawn: by a newer one, a logout, or navigation. the
/// equivalent of swift's CancellationError, never shown to anyone.
struct Cancelled : std::exception {
    const char *what() const noexcept override { return "cancelled"; }
};

} // namespace swiftify
