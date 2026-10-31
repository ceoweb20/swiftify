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

#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <algorithm>

namespace swiftify {

/// how the volume is taken down as a sleep timer runs out (PlayerInteractionState.swift)
enum class SleepFade { None, FinalSeconds, Entire };

inline SleepFade sleepFadeFromString(const QString &value)
{
    if (value == QLatin1StringView("none")) return SleepFade::None;
    if (value == QLatin1StringView("entire")) return SleepFade::Entire;
    return SleepFade::FinalSeconds;
}

inline QString sleepFadeName(SleepFade fade)
{
    switch (fade) {
    case SleepFade::None: return QStringLiteral("none");
    case SleepFade::Entire: return QStringLiteral("entire");
    case SleepFade::FinalSeconds: return QStringLiteral("finalSeconds");
    }
    return QStringLiteral("finalSeconds");
}

/// volume as a fraction of where it started, given how much of the timer is
/// left. silence lands exactly on the deadline in both fading modes.
inline double sleepFadeLevel(SleepFade fade, double remainingSeconds, double durationSeconds)
{
    if (durationSeconds <= 0) {
        return 1;
    }
    switch (fade) {
    case SleepFade::None:
        return 1;
    case SleepFade::FinalSeconds: {
        const double window = std::min(10.0, durationSeconds);
        return remainingSeconds < window ? std::max(0.0, remainingSeconds / window) : 1;
    }
    case SleepFade::Entire:
        return std::clamp(remainingSeconds / durationSeconds, 0.0, 1.0);
    }
    return 1;
}

/// keeps a seek drag tied to the song that was playing when it began
/// (PlaybackScrubSession.swift). a song change mid-drag means no seek at all.
class ScrubSession : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool editing READ isEditing NOTIFY editingChanged FINAL)

public:
    using QObject::QObject;

    bool isEditing() const { return m_editing; }

    Q_INVOKABLE void begin(quint64 songInstanceId)
    {
        m_songId = songInstanceId;
        m_cancelled = songInstanceId == 0;
        setEditing(true);
    }

    Q_INVOKABLE bool acceptsChanges(quint64 songInstanceId) const
    {
        return m_editing && !m_cancelled && m_songId == songInstanceId;
    }

    Q_INVOKABLE void cancel() { m_cancelled = true; }

    /// the position to seek to, or -1 when the drag should be dropped
    Q_INVOKABLE double finish(quint64 songInstanceId, double position)
    {
        const double target = acceptsChanges(songInstanceId) ? position : -1;
        m_songId = 0;
        m_cancelled = false;
        setEditing(false);
        return target;
    }

signals:
    void editingChanged();

private:
    void setEditing(bool editing)
    {
        if (m_editing != editing) {
            m_editing = editing;
            emit editingChanged();
        }
    }

    bool m_editing = false;
    bool m_cancelled = false;
    quint64 m_songId = 0;
};

} // namespace swiftify
