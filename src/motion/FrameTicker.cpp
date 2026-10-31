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

#include "motion/FrameTicker.h"

namespace swiftify {

FrameTicker *FrameTicker::instance()
{
    // never freed: items unsubscribe as they are destroyed, which can be
    // after the application object has gone
    static FrameTicker *ticker = new FrameTicker;
    return ticker;
}

FrameTicker::FrameTicker(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(kIntervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &FrameTicker::tick);
}

void FrameTicker::subscribe(QObject *subscriber)
{
    if (!subscriber || m_subscribers.contains(subscriber)) {
        return;
    }
    m_subscribers.insert(subscriber);
    connect(subscriber, &QObject::destroyed, this, [this, subscriber] {
        m_subscribers.remove(subscriber);
        update();
    });
    update();
}

void FrameTicker::unsubscribe(QObject *subscriber)
{
    if (m_subscribers.remove(subscriber)) {
        disconnect(subscriber, &QObject::destroyed, this, nullptr);
        update();
    }
}

void FrameTicker::update()
{
    if (m_subscribers.isEmpty()) {
        m_timer.stop();
    } else if (!m_timer.isActive()) {
        m_timer.start();
    }
}

} // namespace swiftify
