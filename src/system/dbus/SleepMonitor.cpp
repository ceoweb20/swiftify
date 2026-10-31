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

#include "system/dbus/SleepMonitor.h"

#include "playback/PlaybackController.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QLoggingCategory>

namespace swiftify {

Q_LOGGING_CATEGORY(lcSleep, "swiftify.sleep")

namespace {
const QString kService = QStringLiteral("org.freedesktop.login1");
const QString kPath = QStringLiteral("/org/freedesktop/login1");
const QString kManager = QStringLiteral("org.freedesktop.login1.Manager");
} // namespace

SleepMonitor::SleepMonitor(PlaybackController *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        return;
    }
    m_available = bus.connect(kService, kPath, kManager, QStringLiteral("PrepareForSleep"), this, SLOT(prepareForSleep(bool)));
    if (!m_available) {
        qCInfo(lcSleep) << "logind is not available; playback will not pause for sleep";
        return;
    }
    connect(m_player, &PlaybackController::playingChanged, this, &SleepMonitor::updateInhibitor);
}

void SleepMonitor::updateInhibitor()
{
    if (!m_available) {
        return;
    }
    if (!m_player->isPlaying()) {
        // closing the descriptor releases the inhibitor
        m_inhibitor = {};
        return;
    }
    if (m_inhibitor.isValid()) {
        return;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kManager, QStringLiteral("Inhibit"));
    call << QStringLiteral("sleep") << QStringLiteral("Swiftify") << QStringLiteral("Pausing playback before sleep")
         << QStringLiteral("delay");
    const QDBusReply<QDBusUnixFileDescriptor> reply = QDBusConnection::systemBus().call(call);
    if (reply.isValid()) {
        m_inhibitor = reply.value();
    }
}

void SleepMonitor::prepareForSleep(bool sleeping)
{
    if (sleeping) {
        m_player->prepareForSleep();
        m_inhibitor = {};
    } else {
        updateInhibitor();
    }
}

} // namespace swiftify
