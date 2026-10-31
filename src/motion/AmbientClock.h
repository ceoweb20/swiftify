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
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// the clock the app's slow, ambient motion runs on: the shapes and
/// squiggles that turn while music plays. `value` grows at `speed` units a
/// second, and moves on the shared 30 hz tick (FrameTicker) rather than with
/// every frame the display shows: a turn this slow looks the same at 30
/// frames a second, and between ticks nothing in the window changes, so
/// nothing is drawn. on a per-frame clock the whole window was drawn at the
/// display's rate for as long as anything turned. it stops at speed 0.
class AmbientClock : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal speed READ speed WRITE setSpeed NOTIFY speedChanged FINAL)
    Q_PROPERTY(qreal value READ value NOTIFY valueChanged FINAL)

public:
    explicit AmbientClock(QObject *parent = nullptr);
    ~AmbientClock() override;

    /// the clock everything that turns with the music follows (UiState.spin,
    /// MaterialShape.spinRate). made on first use and never freed: shapes
    /// stop following it as they go, which can be after the application has
    static AmbientClock *spin();

    qreal speed() const { return m_speed; }
    void setSpeed(qreal speed);
    qreal value() const { return m_value; }
    bool isTicking() const { return m_ticking; }

signals:
    void speedChanged();
    void valueChanged();

private:
    /// counts the time since the last count at the current speed
    void accumulate();
    void tick();

    QElapsedTimer m_clock;
    qreal m_speed = 0;
    qreal m_value = 0;
    bool m_ticking = false;
};

} // namespace swiftify
