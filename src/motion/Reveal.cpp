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

#include "motion/Reveal.h"

#include "app/Settings.h"

#include <QEasingCurve>
#include <QPropertyAnimation>

#include <algorithm>

namespace swiftify {

RevealAttached::RevealAttached(QObject *parent)
    : QObject(parent)
    , m_item(qobject_cast<QQuickItem *>(parent))
{
    connect(&m_animation, &QAbstractAnimation::finished, this, [this] {
        if (m_item && !m_shown) {
            m_item->setVisible(false);
        }
    });
    if (auto *settings = Settings::instance()) {
        connect(settings, &Settings::performanceModeChanged, this, [this, settings] {
            if (settings->performanceMode()) {
                settle();
            }
        });
    }
}

void RevealAttached::settle()
{
    m_animation.stop();
    if (!m_item) {
        return;
    }
    m_item->setOpacity(m_shown ? 1 : 0);
    m_item->setScale(1);
    m_item->setVisible(m_shown);
}

void RevealAttached::setScaleFrom(qreal scale)
{
    if (qFuzzyCompare(m_scaleFrom, scale)) {
        return;
    }
    m_scaleFrom = scale;
    emit scaleFromChanged();
}

void RevealAttached::setDuration(int duration)
{
    if (m_duration == duration) {
        return;
    }
    m_duration = duration;
    emit timingChanged();
}

void RevealAttached::setOvershoot(qreal overshoot)
{
    if (qFuzzyCompare(m_overshoot, overshoot)) {
        return;
    }
    m_overshoot = overshoot;
    emit timingChanged();
}

void RevealAttached::setShown(bool shown)
{
    if (m_settled && shown == m_shown) {
        return;
    }
    const bool first = !m_settled;
    m_settled = true;
    m_shown = shown;
    emit shownChanged();
    if (!m_item) {
        return;
    }
    const Settings *settings = Settings::instance();
    if (first || (settings && settings->performanceMode())) {
        settle();
        return;
    }
    animate(shown);
}

void RevealAttached::animate(bool shown)
{
    m_animation.stop();
    m_animation.clear();

    if (shown && !m_item->isVisible()) {
        m_item->setOpacity(0);
        m_item->setScale(m_scaleFrom);
    }
    m_item->setVisible(true);

    auto *fade = new QPropertyAnimation(m_item, "opacity");
    fade->setEndValue(shown ? 1.0 : 0.0);
    fade->setDuration(shown ? std::min(200, m_duration) : std::min(150, m_duration));
    fade->setEasingCurve(shown ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
    m_animation.addAnimation(fade);

    auto *grow = new QPropertyAnimation(m_item, "scale");
    grow->setEndValue(shown ? 1.0 : m_scaleFrom);
    grow->setDuration(shown ? m_duration : std::min(170, m_duration));
    QEasingCurve spring(shown ? (m_overshoot > 0 ? QEasingCurve::OutBack : QEasingCurve::OutCubic) : QEasingCurve::InCubic);
    if (shown && m_overshoot > 0) {
        spring.setOvershoot(m_overshoot);
    }
    grow->setEasingCurve(spring);

    m_animation.addAnimation(grow);
    m_animation.start();
}

} // namespace swiftify
