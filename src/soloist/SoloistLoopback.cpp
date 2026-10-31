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

#include "soloist/SoloistLoopback.h"

#include <QLoggingCategory>

#include <algorithm>
#include <vector>

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#if __has_include(<audioclientactivationparams.h>)
#include <audioclientactivationparams.h>
#else
// older mingw-w64 headers lack these; they are as the windows sdk has them
enum AUDIOCLIENT_ACTIVATION_TYPE {
    AUDIOCLIENT_ACTIVATION_TYPE_DEFAULT = 0,
    AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK = 1,
};
enum PROCESS_LOOPBACK_MODE {
    PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE = 0,
    PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE = 1,
};
struct AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS {
    DWORD TargetProcessId;
    PROCESS_LOOPBACK_MODE ProcessLoopbackMode;
};
struct AUDIOCLIENT_ACTIVATION_PARAMS {
    AUDIOCLIENT_ACTIVATION_TYPE ActivationType;
    union {
        AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS ProcessLoopbackParams;
    };
};
#define VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK L"VAD\\Process_Loopback"
#endif

Q_DECLARE_LOGGING_CATEGORY(lcSoloist)

namespace swiftify {

namespace {

/// how long a wait for soloist's audio lasts before silence is handed on
constexpr DWORD kQuietMs = 100;
/// how much silence is handed on once soloist goes quiet, enough for the
/// bars to fall all the way
constexpr DWORD kFallMs = 1000;

/// a COM pointer, released on going
template <typename T>
struct Com {
    T *ptr = nullptr;
    ~Com()
    {
        if (ptr) {
            ptr->Release();
        }
    }
    T *operator->() const { return ptr; }
};

/// wakes the recording thread once the loopback client is made. agile, as
/// ActivateAudioInterfaceAsync asks, since it is called on a thread of its own
class Activated final : public IActivateAudioInterfaceCompletionHandler, public IAgileObject {
public:
    Activated()
        : m_done(::CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {
    }
    ~Activated() { ::CloseHandle(m_done); }

    HANDLE done() const { return m_done; }

    STDMETHODIMP QueryInterface(REFIID iid, void **out) override
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IActivateAudioInterfaceCompletionHandler)) {
            *out = static_cast<IActivateAudioInterfaceCompletionHandler *>(this);
        } else if (iid == __uuidof(IAgileObject)) {
            *out = static_cast<IAgileObject *>(this);
        } else {
            *out = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ULONG(++m_refs); }
    STDMETHODIMP_(ULONG) Release() override
    {
        const ULONG refs = ULONG(--m_refs);
        if (refs == 0) {
            delete this;
        }
        return refs;
    }
    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation *) override
    {
        ::SetEvent(m_done);
        return S_OK;
    }

private:
    std::atomic<long> m_refs = 1;
    HANDLE m_done;
};

} // namespace

SoloistLoopback::SoloistLoopback(Sink sink)
    : m_sink(std::move(sink))
    , m_wake(::CreateEventW(nullptr, TRUE, FALSE, nullptr))
{
}

SoloistLoopback::~SoloistLoopback()
{
    stop();
    ::CloseHandle(m_wake);
}

void SoloistLoopback::start(quint32 processId)
{
    stop();
    m_stopping = false;
    ::ResetEvent(m_wake);
    m_thread = std::thread([this, processId] { record(processId); });
}

void SoloistLoopback::stop()
{
    m_stopping = true;
    ::SetEvent(m_wake);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void SoloistLoopback::record(quint32 processId)
{
    const HRESULT initialised = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const auto fail = [&](const char *what, HRESULT result) {
        qCWarning(lcSoloist).nospace() << "Soloist can't be heard by the visualizers: " << what
                                       << " (0x" << Qt::hex << quint32(result) << ")";
    };

    AUDIOCLIENT_ACTIVATION_PARAMS params{};
    params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    params.ProcessLoopbackParams.TargetProcessId = DWORD(processId);
    params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT activation{};
    activation.vt = VT_BLOB;
    activation.blob.cbSize = sizeof(params);
    activation.blob.pBlobData = reinterpret_cast<BYTE *>(&params);

    Com<IAudioClient> client;
    Com<IAudioCaptureClient> capture;
    HANDLE ready = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    [&] {
        auto *activated = new Activated;
        Com<IActivateAudioInterfaceCompletionHandler> handler{activated};
        Com<IActivateAudioInterfaceAsyncOperation> operation;
        HRESULT result = ::ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
                                                       &activation, handler.ptr, &operation.ptr);
        if (FAILED(result)) {
            return fail("windows has no process loopback (it needs windows 10 2004 or later)", result);
        }
        if (::WaitForSingleObject(activated->done(), 5000) != WAIT_OBJECT_0) {
            return fail("the loopback took too long to be made", HRESULT_FROM_WIN32(ERROR_TIMEOUT));
        }
        HRESULT activateResult = E_FAIL;
        IUnknown *unknown = nullptr;
        result = operation->GetActivateResult(&activateResult, &unknown);
        if (FAILED(result) || FAILED(activateResult) || !unknown) {
            return fail("the loopback could not be made", FAILED(result) ? result : activateResult);
        }
        result = unknown->QueryInterface(__uuidof(IAudioClient), reinterpret_cast<void **>(&client.ptr));
        unknown->Release();
        if (FAILED(result)) {
            return fail("the loopback could not be made", result);
        }

        // a process loopback has no format of its own to ask for; it is
        // converted to what is asked
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        format.nChannels = WORD(kChannels);
        format.nSamplesPerSec = kRate;
        format.wBitsPerSample = 32;
        format.nBlockAlign = WORD(kChannels * sizeof(float));
        format.nAvgBytesPerSec = kRate * format.nBlockAlign;
        constexpr REFERENCE_TIME kBuffer = 200 * 10'000; // 200ms, in 100ns
        result = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                                        | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
                                    kBuffer, 0, &format, nullptr);
        if (SUCCEEDED(result)) {
            result = client->SetEventHandle(ready);
        }
        if (SUCCEEDED(result)) {
            result = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void **>(&capture.ptr));
        }
        if (SUCCEEDED(result)) {
            result = client->Start();
        }
        if (FAILED(result)) {
            if (capture.ptr) {
                capture->Release();
                capture.ptr = nullptr;
            }
            return fail("the loopback would not record", result);
        }
        qCInfo(lcSoloist) << "Soloist is heard by the visualizers through a process loopback";
    }();

    if (capture.ptr) {
        std::vector<float> silence(size_t(kRate) * kQuietMs / 1000 * kChannels, 0.0f);
        // silence still to hand on, in samples; none while soloist is quiet
        // past that, so a paused soloist costs a sleeping thread
        const qsizetype fall = qsizetype(kRate) * kFallMs / 1000 * kChannels;
        qsizetype falling = 0;
        const auto quiet = [&](qsizetype count) {
            count = std::min(count, falling);
            falling -= count;
            while (count > 0) {
                const qsizetype run = std::min(count, qsizetype(silence.size()));
                m_sink(silence.data(), run);
                count -= run;
            }
        };
        const HANDLE waits[] = {m_wake, ready};
        while (!m_stopping) {
            const DWORD woke = ::WaitForMultipleObjects(2, waits, FALSE, falling > 0 ? kQuietMs : INFINITE);
            if (woke == WAIT_TIMEOUT) {
                // nothing comes while soloist plays nothing
                quiet(qsizetype(silence.size()));
                continue;
            }
            if (woke != WAIT_OBJECT_0 + 1) {
                break;
            }
            BYTE *data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            while (!m_stopping && capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr) == S_OK) {
                const qsizetype count = qsizetype(frames) * kChannels;
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    quiet(count);
                } else if (count > 0) {
                    m_sink(reinterpret_cast<const float *>(data), count);
                    falling = fall;
                }
                capture->ReleaseBuffer(frames);
            }
        }
        client->Stop();
    }
    ::CloseHandle(ready);
    if (SUCCEEDED(initialised)) {
        ::CoUninitialize();
    }
}

} // namespace swiftify
