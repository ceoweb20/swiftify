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

#include <QDateTime>
#include <QHash>
#include <QObject>

#include <cstdint>

struct pa_context;
struct pa_server_info;
struct pa_sink_info;
struct pa_threaded_mainloop;

namespace swiftify {

/// watches pulseaudio (pipewire-pulse) for the output going away or arriving.
///
/// on linux the core does not notice headphones being pulled: pipewire just
/// moves the stream to the speakers. the mac app's rule still holds, so here
/// the default sink disappearing, or its active port (the headphone jack)
/// becoming unavailable, counts as the output being lost. a new sink or port
/// counts as one arriving, which some devices follow with a "play".
class AudioDeviceMonitor : public QObject {
    Q_OBJECT

public:
    explicit AudioDeviceMonitor(QObject *parent = nullptr);
    ~AudioDeviceMonitor() override;

signals:
    void outputLost();
    void outputArrived();

private:
    struct Sink {
        QString name;
        QString activePort;
        bool activePortAvailable = true;
    };

    static void onContextState(pa_context *context, void *self);
    static void onSubscription(pa_context *context, int type, uint32_t index, void *self);
    static void onServerInfo(pa_context *context, const pa_server_info *info, void *self);
    static void onSinkInfo(pa_context *context, const pa_sink_info *info, int endOfList, void *self);
    static void onInitialSinkInfo(pa_context *context, const pa_sink_info *info, int endOfList, void *self);

    // these run on pulseaudio's thread and only touch the state below it
    void serverInfo(const pa_server_info *info);
    void sinkInfo(const pa_sink_info *info, bool initial);
    void sinkRemoved(uint32_t index);
    void notifyLost();
    void notifyArrived();

    pa_threaded_mainloop *m_mainloop = nullptr;
    pa_context *m_context = nullptr;

    QHash<uint32_t, Sink> m_sinks;
    QString m_defaultSink;
    /// pipewire can name a new default before it removes the old one
    QString m_previousDefaultSink;
    QDateTime m_defaultChangedAt;
};

} // namespace swiftify
