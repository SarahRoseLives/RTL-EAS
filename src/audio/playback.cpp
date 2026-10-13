#include "audio/playback.h"

#include "util/log.h"

#include <algorithm>
#include <cmath>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#if defined(_WIN32)

struct Playback::Impl
{
    static constexpr int kBuffers   = 12;
    static constexpr int kBufFrames = 4096;

    HWAVEOUT           handle = nullptr;
    WAVEHDR            hdr[kBuffers]{};
    std::vector<short> buf[kBuffers];
    bool               inUse[kBuffers]{};
    bool               prepared[kBuffers]{};
    bool               loggedWrite = false;
};

Playback::~Playback()
{
    close();
}

bool Playback::open(int sampleRate, int channels)
{
    close();

    impl_ = new Impl();
    WAVEFORMATEX wf{};
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = (WORD)channels;
    wf.nSamplesPerSec  = (DWORD)sampleRate;
    wf.wBitsPerSample  = 16;
    wf.nBlockAlign     = (WORD)(channels * 2);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    MMRESULT r = waveOutOpen(&impl_->handle, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL);
    if (r != MMSYSERR_NOERROR || !impl_->handle)
    {
        logWrite(LogLevel::Info, "audio: playback unavailable (waveOutOpen error %u)", (unsigned)r);
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    for (int i = 0; i < Impl::kBuffers; ++i)
    {
        impl_->buf[i].assign((size_t)Impl::kBufFrames * channels, 0);
        WAVEHDR& h          = impl_->hdr[i];
        h.lpData            = (LPSTR)impl_->buf[i].data();
        h.dwBufferLength    = (DWORD)(impl_->buf[i].size() * sizeof(short));
        h.dwFlags           = 0;
        MMRESULT pr         = waveOutPrepareHeader(impl_->handle, &h, sizeof(WAVEHDR));
        impl_->prepared[i]  = (pr == MMSYSERR_NOERROR);
        impl_->inUse[i]     = false;
    }

    open_       = true;
    sampleRate_ = sampleRate;
    channels_   = channels;
    frameBytes_ = channels * 2;
    logWrite(LogLevel::Info, "audio: playback at %d Hz, %d ch", sampleRate, channels);
    return true;
}

void Playback::write(const float* interleaved, int frames)
{
    if (!open_ || !impl_ || frames <= 0 || channels_ <= 0)
    {
        return;
    }

    int written = 0;
    while (written < frames)
    {
        // Find a free (never-used or completed) buffer.
        int idx = -1;
        for (int i = 0; i < Impl::kBuffers; ++i)
        {
            if (!impl_->inUse[i] || (impl_->hdr[i].dwFlags & WHDR_DONE))
            {
                idx = i;
                break;
            }
        }
        if (idx < 0)
        {
            break; // all buffers still playing; drop the remainder
        }

        int chunk = std::min(frames - written, Impl::kBufFrames);
        short* dst = impl_->buf[idx].data();
        const float* src = interleaved + (size_t)written * channels_;
        int nsamples = chunk * channels_;
        for (int n = 0; n < nsamples; ++n)
        {
            float v = src[n];
            if (v > 1.0f)
            {
                v = 1.0f;
            }
            else if (v < -1.0f)
            {
                v = -1.0f;
            }
            dst[n] = (short)std::lround(v * 32767.0f);
        }

        WAVEHDR& h       = impl_->hdr[idx];
        h.dwBufferLength = (DWORD)(chunk * frameBytes_);
        h.dwFlags &= ~WHDR_DONE;
        impl_->inUse[idx] = true;
        if (waveOutWrite(impl_->handle, &h, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
        {
            impl_->inUse[idx] = false;
            break;
        }
        if (!impl_->loggedWrite)
        {
            impl_->loggedWrite = true;
            logWrite(LogLevel::Info, "audio: playback streaming");
        }
        written += chunk;
    }
}

void Playback::close()
{
    if (impl_)
    {
        if (impl_->handle)
        {
            waveOutReset(impl_->handle);
            for (int i = 0; i < Impl::kBuffers; ++i)
            {
                if (impl_->prepared[i])
                {
                    waveOutUnprepareHeader(impl_->handle, &impl_->hdr[i], sizeof(WAVEHDR));
                }
            }
            waveOutClose(impl_->handle);
        }
        delete impl_;
        impl_ = nullptr;
    }
    open_ = false;
}

#else // !_WIN32

struct Playback::Impl
{
};

Playback::~Playback()
{
    close();
}

bool Playback::open(int, int)
{
    logWrite(LogLevel::Info, "audio: playback not implemented on this platform");
    return false;
}

void Playback::write(const float*, int) {}

void Playback::close()
{
    open_ = false;
}

#endif
