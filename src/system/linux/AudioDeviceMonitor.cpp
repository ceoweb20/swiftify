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

#include "system/linux/AudioDeviceMonitor.h"

#include <QLoggingCategory>
#include <QMetaObject>

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/subscribe.h>
#include <pulse/thread-mainloop.h>

#include <cstdint>

namespace swiftify {

Q_LOGGING_CATEGORY(lcDevices, "swiftify.devices")

namespace {

/// how long the sink that used to be the default still counts as the one we
/// were playing on, when the server renames the default first
constexpr qint64 kDefaultHandoverMs = 5000;

const pa_sink_port_info *activePort(const pa_sink_info *info)
{
    return info->active_port;
}

/// pulseaudio asserts on a null operation, which a context going away returns
void release(pa_operation *operation)
{
    if (operation) {
        pa_operation_unref(operation);
    }
}

} // namespace

AudioDeviceMonitor::AudioDeviceMonitor(QObject *parent)
    : QObject(parent)
{
    m_mainloop = pa_threaded_mainloop_new();
    if (!m_mainloop) {
        return;
    }
    m_context = pa_context_new(pa_threaded_mainloop_get_api(m_mainloop), "Swiftify");
    if (!m_context) {
        return;
    }
    pa_context_set_state_callback(m_context, &AudioDeviceMonitor::onContextState, this);
    if (pa_context_connect(m_context, nullptr, PA_CONTEXT_NOFAIL, nullptr) < 0
        || pa_threaded_mainloop_start(m_mainloop) < 0) {
        qCInfo(lcDevices) << "no pulseaudio server; unplugging headphones will not pause playback";
    }
}

AudioDeviceMonitor::~AudioDeviceMonitor()
{
    if (m_mainloop) {
        pa_threaded_mainloop_stop(m_mainloop);
    }
    if (m_context) {
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
    }
    if (m_mainloop) {
        pa_threaded_mainloop_free(m_mainloop);
    }
}

void AudioDeviceMonitor::onContextState(pa_context *context, void *self)
{
    if (pa_context_get_state(context) != PA_CONTEXT_READY) {
        return;
    }
    pa_context_set_subscribe_callback(
        context,
        [](pa_context *c, pa_subscription_event_type_t type, uint32_t index, void *userdata) {
            onSubscription(c, int(type), index, userdata);
        },
        self);
    release(pa_context_subscribe(
        context, pa_subscription_mask_t(PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SERVER), nullptr, nullptr));
    release(pa_context_get_server_info(context, &AudioDeviceMonitor::onServerInfo, self));
    release(pa_context_get_sink_info_list(context, &AudioDeviceMonitor::onInitialSinkInfo, self));
}

void AudioDeviceMonitor::onSubscription(pa_context *context, int type, uint32_t index, void *self)
{
    auto *monitor = static_cast<AudioDeviceMonitor *>(self);
    const int facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    const int kind = type & PA_SUBSCRIPTION_EVENT_TYPE_MASK;

    if (facility == PA_SUBSCRIPTION_EVENT_SERVER) {
        release(pa_context_get_server_info(context, &AudioDeviceMonitor::onServerInfo, self));
        return;
    }
    if (facility != PA_SUBSCRIPTION_EVENT_SINK) {
        return;
    }
    if (kind == PA_SUBSCRIPTION_EVENT_REMOVE) {
        monitor->sinkRemoved(index);
    } else {
        release(pa_context_get_sink_info_by_index(context, index, &AudioDeviceMonitor::onSinkInfo, self));
    }
}

void AudioDeviceMonitor::onServerInfo(pa_context *, const pa_server_info *info, void *self)
{
    if (info) {
        static_cast<AudioDeviceMonitor *>(self)->serverInfo(info);
    }
}

void AudioDeviceMonitor::onSinkInfo(pa_context *, const pa_sink_info *info, int endOfList, void *self)
{
    if (!endOfList && info) {
        static_cast<AudioDeviceMonitor *>(self)->sinkInfo(info, false);
    }
}

void AudioDeviceMonitor::onInitialSinkInfo(pa_context *, const pa_sink_info *info, int endOfList, void *self)
{
    if (!endOfList && info) {
        static_cast<AudioDeviceMonitor *>(self)->sinkInfo(info, true);
    }
}

void AudioDeviceMonitor::serverInfo(const pa_server_info *info)
{
    const QString defaultSink = QString::fromUtf8(info->default_sink_name ? info->default_sink_name : "");
    if (defaultSink == m_defaultSink) {
        return;
    }
    const bool hadDefault = !m_defaultSink.isEmpty();
    m_previousDefaultSink = m_defaultSink;
    m_defaultSink = defaultSink;
    m_defaultChangedAt = QDateTime::currentDateTimeUtc();
    // a new default that is a device just plugged in may follow up with play.
    if (hadDefault) {
        notifyArrived();
    }
}

void AudioDeviceMonitor::sinkInfo(const pa_sink_info *info, bool initial)
{
    Sink current;
    current.name = QString::fromUtf8(info->name);
    if (const pa_sink_port_info *port = activePort(info)) {
        current.activePort = QString::fromUtf8(port->name);
        current.activePortAvailable = port->available != PA_PORT_AVAILABLE_NO;
    }

    const auto known = m_sinks.constFind(info->index);
    const bool isNew = known == m_sinks.cend();
    const Sink previous = isNew ? Sink {} : *known;
    m_sinks.insert(info->index, current);
    if (initial) {
        return;
    }
    if (isNew) {
        notifyArrived();
        return;
    }

    const bool isDefault = current.name == m_defaultSink;
    // the jack the sound was going to has gone: the active port moved away
    // from one that is now unavailable, or the active port itself went dark.
    bool portLost = previous.activePortAvailable && !current.activePortAvailable;
    if (!portLost && previous.activePort != current.activePort && !previous.activePort.isEmpty()) {
        for (uint32_t index = 0; index < info->n_ports; ++index) {
            const pa_sink_port_info *port = info->ports[index];
            if (QString::fromUtf8(port->name) == previous.activePort) {
                portLost = port->available == PA_PORT_AVAILABLE_NO;
                break;
            }
        }
        if (!portLost) {
            // moved onto a port that just became available: headphones in
            notifyArrived();
        }
    } else if (!previous.activePortAvailable && current.activePortAvailable) {
        notifyArrived();
    }
    if (portLost && isDefault) {
        notifyLost();
    }
}

void AudioDeviceMonitor::sinkRemoved(uint32_t index)
{
    const auto found = m_sinks.constFind(index);
    if (found == m_sinks.cend()) {
        return;
    }
    const QString name = found->name;
    m_sinks.remove(index);
    const bool wasPlayingHere = name == m_defaultSink
        || (name == m_previousDefaultSink && m_defaultChangedAt.isValid()
            && m_defaultChangedAt.msecsTo(QDateTime::currentDateTimeUtc()) < kDefaultHandoverMs);
    if (wasPlayingHere) {
        notifyLost();
    }
}

void AudioDeviceMonitor::notifyLost()
{
    QMetaObject::invokeMethod(this, &AudioDeviceMonitor::outputLost, Qt::QueuedConnection);
}

void AudioDeviceMonitor::notifyArrived()
{
    QMetaObject::invokeMethod(this, &AudioDeviceMonitor::outputArrived, Qt::QueuedConnection);
}

} // namespace swiftify
