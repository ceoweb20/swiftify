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

#include <QDBusUnixFileDescriptor>
#include <QObject>

namespace swiftify {

class PlaybackController;

/// systemd-logind's sleep notice. while something plays, a delay inhibitor
/// gives the player time to pause before the machine suspends, so it wakes up
/// paused rather than playing wherever it is by then.
class SleepMonitor : public QObject {
    Q_OBJECT

public:
    explicit SleepMonitor(PlaybackController *player, QObject *parent = nullptr);

private slots:
    void prepareForSleep(bool sleeping);

private:
    void updateInhibitor();

    PlaybackController *m_player;
    bool m_available = false;
    QDBusUnixFileDescriptor m_inhibitor;
};

} // namespace swiftify
