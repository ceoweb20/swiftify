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
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// `Reveal.shown` on any item, in place of `visible`: the item springs in,
/// fading up as it grows from a little smaller, past its size and back, and
/// shrinks away fading; it is only hidden once it has gone, so nothing just
/// appears or vanishes. the first value applies at once, so a view is not
/// animated as it is built. performance mode makes every change immediate.
class RevealAttached : public QObject {
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(bool shown READ isShown WRITE setShown NOTIFY shownChanged FINAL)
    /// how small it starts and ends; 1 is a plain fade
    Q_PROPERTY(qreal scaleFrom READ scaleFrom WRITE setScaleFrom NOTIFY scaleFromChanged FINAL)
    /// how long the spring in takes, in milliseconds
    Q_PROPERTY(int duration READ duration WRITE setDuration NOTIFY timingChanged FINAL)
    /// how far past its size it springs; 0 settles without overshooting
    Q_PROPERTY(qreal overshoot READ overshoot WRITE setOvershoot NOTIFY timingChanged FINAL)

public:
    explicit RevealAttached(QObject *parent);

    bool isShown() const { return m_shown; }
    void setShown(bool shown);
    qreal scaleFrom() const { return m_scaleFrom; }
    void setScaleFrom(qreal scale);
    int duration() const { return m_duration; }
    void setDuration(int duration);
    qreal overshoot() const { return m_overshoot; }
    void setOvershoot(qreal overshoot);

signals:
    void shownChanged();
    void scaleFromChanged();
    void timingChanged();

private:
    void animate(bool shown);
    void settle();

    QPointer<QQuickItem> m_item;
    bool m_shown = true;
    bool m_settled = false;
    qreal m_scaleFrom = 0.85;
    int m_duration = 440;
    qreal m_overshoot = 2.2;
    QParallelAnimationGroup m_animation;
};

class Reveal : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reveal is only an attached property.")
    QML_ATTACHED(RevealAttached)

public:
    static RevealAttached *qmlAttachedProperties(QObject *object) { return new RevealAttached(object); }
};

} // namespace swiftify
