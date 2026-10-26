#include "spotify/Discovery.h"
#include "spotify/SpotifyError.h"
#include "spotify/SpotifyJson.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

namespace swiftify::discovery {
namespace {
std::optional<Playlist> playlist(const QJsonValue &value)
{
    const auto object = value.toObject();
    static const QRegularExpression id(QStringLiteral("^[A-Za-z0-9]{22}$"));
    const QString key = object.value(QStringLiteral("id")).toString();
    if (!id.match(key).hasMatch() || object.value(QStringLiteral("name")).toString().trimmed().isEmpty()) return {};
    // Discovery is read-only. Never infer edit rights from a personalized card.
    auto parsed = json::playlist(object);
    parsed.uri = QStringLiteral("spotify:playlist:") + key;
    return parsed;
}
}

Snapshot parse(const QString &payload)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(payload.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        throw SpotifyError::invalidData(QStringLiteral("Spotify returned invalid personalized collections."));
    }
    const auto object = document.object();
    if (object.value(QStringLiteral("userId")).toString().isEmpty()
        || !object.value(QStringLiteral("personalizedAvailable")).isBool()
        || !object.value(QStringLiteral("libraryAvailable")).isBool()
        || !object.value(QStringLiteral("sections")).isArray()
        || !object.value(QStringLiteral("library")).isArray()) {
        throw SpotifyError::invalidData(QStringLiteral("Spotify returned invalid personalized collections."));
    }
    Snapshot result;
    result.userId = object.value(QStringLiteral("userId")).toString();
    result.personalizedAvailable = object.value(QStringLiteral("personalizedAvailable")).toBool();
    result.libraryAvailable = object.value(QStringLiteral("libraryAvailable")).toBool();
    result.partial = object.value(QStringLiteral("partial")).toBool();
    QSet<QString> saved;
    for (const auto &value : object.value(QStringLiteral("library")).toArray()) {
        if (const auto item = playlist(value); item && !saved.contains(item->id)) {
            saved.insert(item->id);
            result.saved.append(*item);
        }
    }
    QSet<QString> seen;
    for (const auto &value : object.value(QStringLiteral("sections")).toArray()) {
        const auto section = value.toObject();
        QVariantList items;
        for (const auto &value : section.value(QStringLiteral("items")).toArray()) {
            if (auto item = playlist(value); item && !seen.contains(item->id)) {
                item->ownerId.clear();
                item->isCollaborative = false;
                seen.insert(item->id);
                items.append(QVariant::fromValue(*item));
                result.personalized.append(*item);
            }
        }
        if (!items.isEmpty()) result.sections.append(QVariantMap {
            {QStringLiteral("title"), section.value(QStringLiteral("title")).toString()},
            {QStringLiteral("items"), items}});
    }
    return result;
}

} // namespace swiftify::discovery
