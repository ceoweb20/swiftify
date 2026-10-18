#include "spotify/PlaylistLinks.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace swiftify::playlistlinks {
bool validId(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9]{22}$"));
    return pattern.match(value).hasMatch();
}

std::optional<QString> id(const QString &reference)
{
    const QString text = reference.trimmed();
    if (text.size() > 2048) return {};
    const QString prefix = QStringLiteral("spotify:playlist:");
    if (text.startsWith(prefix)) {
        const auto value = text.mid(prefix.size());
        return validId(value) ? std::optional(value) : std::nullopt;
    }
    // Never fetch the supplied URL. Extract an ID and construct a fixed API URL.
    const QUrl url(text, QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != QLatin1StringView("https")
        || url.host() != QLatin1StringView("open.spotify.com") || !url.userInfo().isEmpty()
        || (url.port(-1) != -1 && url.port() != 443)) return {};
    static const QRegularExpression path(QStringLiteral("^/(?:intl-[A-Za-z-]+/)?playlist/([A-Za-z0-9]{22})/?$"));
    const auto match = path.match(url.path(QUrl::FullyEncoded));
    return match.hasMatch() ? std::optional(match.captured(1)) : std::nullopt;
}

QStringList recentIds(const QJsonObject &history)
{
    QStringList result;
    QSet<QString> seen;
    for (const auto &value : history.value(QStringLiteral("items")).toArray()) {
        const auto context = value.toObject().value(QStringLiteral("context")).toObject();
        if (context.value(QStringLiteral("type")).toString() != QLatin1StringView("playlist")) continue;
        const QString uri = context.value(QStringLiteral("uri")).toString();
        if (!uri.startsWith(QLatin1StringView("spotify:playlist:"))) continue;
        const auto key = id(uri);
        if (key && !seen.contains(*key)) { result.append(*key); seen.insert(*key); }
    }
    return result;
}
} // namespace swiftify::playlistlinks
