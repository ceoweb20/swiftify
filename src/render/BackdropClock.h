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
#include <QObject>
#include <QTimer>
#include <QVector4D>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// what moves the living backdrop (ArtworkBackdrop.swift): a clock that runs
/// while music plays and holds where it stopped on pause, and the five band
/// energies scaled by the reactivity setting. both refresh at 30 hz, and only
/// while the backdrop flows and something is playing; a held field gets no
/// energy, so it settles the moment playback stops.
class BackdropClock : public QObject {
    Q_OBJECT
    QML_ELEMENT

    /// the backdrop is on screen and allowed to move
    Q_PROPERTY(bool running READ isRunning WRITE setRunning NOTIFY runningChanged FINAL)
    Q_PROPERTY(bool playing READ isPlaying WRITE setPlaying NOTIFY playingChanged FINAL)
    Q_PROPERTY(qreal reactivity READ reactivity WRITE setReactivity NOTIFY reactivityChanged FINAL)

    /// seconds the field has flowed for
    Q_PROPERTY(qreal time READ time NOTIFY frame FINAL)
    /// sub-bass, bass, upper bass and vocal
    Q_PROPERTY(QVector4D energy READ energy NOTIFY frame FINAL)
    Q_PROPERTY(qreal lead READ lead NOTIFY frame FINAL)

public:
    static constexpr int kIntervalMs = 33;

    explicit BackdropClock(QObject *parent = nullptr);

    bool isRunning() const { return m_running; }
    void setRunning(bool running);
    bool isPlaying() const { return m_playing; }
    void setPlaying(bool playing);
    qreal reactivity() const { return m_reactivity; }
    void setReactivity(qreal reactivity);

    qreal time() const { return m_time; }
    QVector4D energy() const { return m_energy; }
    qreal lead() const { return m_lead; }

signals:
    void runningChanged();
    void playingChanged();
    void reactivityChanged();
    void frame();

private:
    void update();
    void tick();

    bool m_running = false;
    bool m_playing = false;
    qreal m_reactivity = 1;

    QTimer m_timer;
    QElapsedTimer m_clock;
    /// the time the clock stood at when it last stopped
    qreal m_held = 0;
    qreal m_time = 0;
    QVector4D m_energy;
    qreal m_lead = 0;
};

} // namespace swiftify
