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

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <array>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

class PlaybackController;
class PlaybackCore;

/// the analyser's 48 bands, polled at 30 hz only while something can show
/// them (AudioSpectrumModel), and the loading pulse while a song loads.
/// visualizers interpolate between polls at the display's own rate.
class SpectrumSource : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Spectrum)
    QML_SINGLETON

    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged FINAL)

public:
    static constexpr int kBands = 48;

    SpectrumSource(PlaybackCore *core, PlaybackController *player, QObject *parent = nullptr);
    ~SpectrumSource() override;

    static SpectrumSource *instance();
    static SpectrumSource *create(QQmlEngine *, QJSEngine *);

    const QList<float> &levels() const { return m_levels; }
    bool isLoading() const { return m_loading; }
    float loadingPhase() const { return m_loadingPhase; }

    /// means of the raw bands over the backdrop's five ranges: sub-bass, bass,
    /// upper bass, vocal and lead
    std::array<float, 5> energies() const;
    /// takes a new set of bands, sanitised; what each poll does
    void publish(const float *levels, int count);

signals:
    void levelsChanged();
    void loadingChanged();

private:
    void updateActivity();
    void poll();
    void tickLoading();
    void clear();

    PlaybackCore *m_core;
    PlaybackController *m_player;
    QList<float> m_levels;
    QTimer m_pollTimer;
    QTimer m_loadingTimer;
    QElapsedTimer m_loadingClock;
    bool m_loading = false;
    float m_loadingPhase = 0;
};

} // namespace swiftify
