#include "playback/PlaybackState.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <limits>

namespace swiftify::playbackstate {
namespace {
constexpr qsizetype maximumBytes = 4 * 1024 * 1024;

bool restorable(const Song &song)
{
    static const QRegularExpression spotify(QStringLiteral("^spotify:track:[A-Za-z0-9]{22}$"));
    return spotify.match(song.uri).hasMatch();
}

quint32 milliseconds(const QJsonValue &value)
{
    const double raw = value.toDouble(0);
    return raw >= 0 && raw <= std::numeric_limits<int>::max() ? quint32(raw) : 0;
}
}

QByteArray encode(const QList<Song> &queue, const QList<Song> &shuffled,
                  const QList<Song> &history, const Song &current,
                  quint32 positionMs, quint32 durationMs, bool shuffle, int repeat)
{
    if (!restorable(current)) return {};
    auto found = std::find_if(queue.cbegin(), queue.cend(), [&](const Song &song) { return song.isSameInstance(current); });
    const int originalIndex = found == queue.cend() ? -1 : int(std::distance(queue.cbegin(), found));
    // Bound disk/memory usage on exceptionally large queues; retain a window
    // around the current occurrence, never accidentally omit the current song.
    const int start = std::max(0, originalIndex - maximumQueue / 2);
    const auto kept = originalIndex < 0 ? QList<Song> {current} : queue.mid(start, maximumQueue);
    QJsonArray items;
    QHash<quint64, int> indices;
    for (const Song &song : kept) {
        if (!restorable(song)) return {};
        indices.insert(song.instanceId, int(items.size()));
        items.append(song.toJson());
    }
    QJsonArray order;
    QSet<int> seen;
    for (const Song &song : shuffled) {
        const auto index = indices.constFind(song.instanceId);
        if (index != indices.cend() && !seen.contains(index.value())) {
            order.append(index.value());
            seen.insert(index.value());
        }
    }
    // A snapshot with shuffle enabled must contain a complete permutation.
    for (int index = 0; index < items.size(); ++index) if (!seen.contains(index)) order.append(index);
    QJsonArray previous;
    for (const Song &song : history.mid(std::max(0, int(history.size()) - 100))) {
        if (!restorable(song)) continue;
        const auto index = indices.constFind(song.instanceId);
        previous.append(index != indices.cend()
            ? QJsonObject {{QStringLiteral("queueIndex"), index.value()}}
            : QJsonObject {{QStringLiteral("song"), song.toJson()}});
    }
    const quint32 duration = durationMs > 0 ? durationMs : quint32(std::max(0, current.durationMs));
    const auto state = QJsonObject {
        {QStringLiteral("version"), 1}, {QStringLiteral("queue"), items},
        {QStringLiteral("currentIndex"), originalIndex < 0 ? 0 : originalIndex - start},
        {QStringLiteral("shuffledOrder"), order}, {QStringLiteral("history"), previous},
        {QStringLiteral("positionMs"), qint64(duration > 0 ? std::min(positionMs, duration) : positionMs)},
        {QStringLiteral("durationMs"), qint64(duration)}, {QStringLiteral("shuffle"), shuffle},
        {QStringLiteral("repeat"), std::clamp(repeat, 0, 2)}};
    const QByteArray bytes = QJsonDocument(state).toJson(QJsonDocument::Compact);
    return bytes.size() <= maximumBytes ? bytes : QByteArray();
}

std::optional<Snapshot> decode(const QByteArray &bytes)
{
    if (bytes.isEmpty() || bytes.size() > maximumBytes) return {};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    const auto object = document.object();
    if (object.value(QStringLiteral("version")).toInt(-1) != 1) return {};
    const auto items = object.value(QStringLiteral("queue")).toArray();
    const int currentIndex = object.value(QStringLiteral("currentIndex")).toInt(-1);
    if (items.isEmpty() || items.size() > maximumQueue || currentIndex < 0 || currentIndex >= items.size()) return {};
    Snapshot state;
    state.currentIndex = currentIndex;
    for (const auto &value : items) {
        const Song song = Song::fromJson(value.toObject());
        if (!restorable(song)) return {};
        state.queue.append(song);
    }
    const auto order = object.value(QStringLiteral("shuffledOrder")).toArray();
    QSet<int> seen;
    if (order.size() != items.size()) return {};
    for (const auto &value : order) {
        const int index = value.toInt(-1);
        if (index < 0 || index >= items.size() || seen.contains(index)) return {};
        seen.insert(index);
        state.shuffled.append(state.queue.at(index));
    }
    const auto previous = object.value(QStringLiteral("history")).toArray();
    if (previous.size() > 100) return {};
    for (const auto &value : previous) {
        const auto entry = value.toObject();
        const int index = entry.value(QStringLiteral("queueIndex")).toInt(-1);
        if (index >= 0 && index < state.queue.size()) {
            state.history.append(state.queue.at(index));
            continue;
        }
        const Song song = Song::fromJson(entry.value(QStringLiteral("song")).toObject());
        if (restorable(song)) state.history.append(song);
    }
    state.durationMs = milliseconds(object.value(QStringLiteral("durationMs")));
    if (state.durationMs == 0) state.durationMs = quint32(std::max(0, state.queue.at(currentIndex).durationMs));
    state.positionMs = milliseconds(object.value(QStringLiteral("positionMs")));
    if (state.durationMs > 0) state.positionMs = std::min(state.positionMs, state.durationMs);
    state.shuffle = object.value(QStringLiteral("shuffle")).toBool();
    state.repeat = std::clamp(object.value(QStringLiteral("repeat")).toInt(), 0, 2);
    return state;
}

} // namespace swiftify::playbackstate
