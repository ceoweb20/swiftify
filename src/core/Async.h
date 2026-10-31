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

#include "spotify/SpotifyError.h"

#include <QCoroTask>

#include <memory>
#include <utility>

namespace swiftify {

/// set when the work holding it has been withdrawn. checked after every await,
/// the way swift code calls Task.checkCancellation().
using CancelToken = std::shared_ptr<bool>;

inline bool isCancelled(const CancelToken &token)
{
    return token && *token;
}

inline void throwIfCancelled(const CancelToken &token)
{
    if (isCancelled(token)) {
        throw Cancelled();
    }
}

/// one kind of work that only ever runs once at a time: starting it again
/// withdraws the previous run, as reassigning a swift Task property after
/// cancelling it does.
class TaskSlot {
public:
    CancelToken renew()
    {
        cancel();
        m_token = std::make_shared<bool>(false);
        return m_token;
    }

    void cancel()
    {
        if (m_token) {
            *m_token = true;
            m_token.reset();
        }
    }

    /// clears the slot if `token` is still the one in it
    void finish(const CancelToken &token)
    {
        if (m_token == token) {
            m_token.reset();
        }
    }

    bool isRunning() const { return m_token != nullptr; }

private:
    CancelToken m_token;
};

/// lets a coroutine run on its own. qcoro reference-counts the frame, so it
/// lives until it finishes; the caller is expected to have caught everything.
template <typename T>
void detach(QCoro::Task<T> &&task)
{
    [[maybe_unused]] auto discarded = std::move(task);
}

} // namespace swiftify
