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

#include "system/MacTitleBar.h"

#ifdef Q_OS_MACOS
#include "system/macos/MacWindow.h"
#endif

namespace swiftify {

MacTitleBar::MacTitleBar(QObject *parent)
    : QObject(parent)
{
}

void MacTitleBar::setTarget(QQuickWindow *target)
{
    if (m_target == target) {
        return;
    }
    if (m_target) {
        disconnect(m_target, nullptr, this, nullptr);
    }
    m_target = target;
    emit targetChanged();
#ifdef Q_OS_MACOS
    if (m_target) {
        // applied each time the window shows or changes state, after qt has
        // set the frame up for it
        connect(m_target, &QWindow::visibleChanged, this, &MacTitleBar::apply);
        connect(m_target, &QWindow::windowStateChanged, this, &MacTitleBar::apply);
        apply();
    }
#endif
}

void MacTitleBar::apply()
{
#ifdef Q_OS_MACOS
    if (m_target && m_target->isVisible()) {
        macos::styleTitleBar(m_target);
    }
#endif
}

} // namespace swiftify
