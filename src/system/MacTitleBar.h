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
#include <QPointer>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// the top of a window on macos: the content runs up under a transparent
/// title bar, and of the system's bar only the traffic lights stay: no
/// title, no bar, no line beneath it. the window asks for the content under
/// the bar with Qt::ExpandedClientAreaHint and Qt::NoTitleBarBackgroundHint,
/// and this hides the rest. it does nothing elsewhere, where the app draws
/// its own top.
class MacTitleBar : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickWindow *target READ target WRITE setTarget NOTIFY targetChanged FINAL)

public:
    explicit MacTitleBar(QObject *parent = nullptr);

    QQuickWindow *target() const { return m_target; }
    void setTarget(QQuickWindow *target);

signals:
    void targetChanged();

private:
    void apply();

    QPointer<QQuickWindow> m_target;
};

} // namespace swiftify
