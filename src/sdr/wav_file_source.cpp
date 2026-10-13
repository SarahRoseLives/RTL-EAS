#include "sdr/wav_file_source.h"

#include "util/log.h"

#include <algorithm>
#include <chrono>
#include <vector>

WavFileSource::~WavFileSource()
{
    stop();
}

bool WavFileSource::start(int, SdrSampleCb cb, std::string& err)
{
    if (running_.load())
    {
        return true;
    }

    WavReader probe;
    if (!probe.open(path_, err))
    {
        return false;
    }
    sampleRate_ = (double)probe.format().sampleRate;
    channels_   = probe.format().channels;
    bits_       = probe.format().bits;
    probe.close();

    if (sampleRate_ <= 0.0)
    {
        err = "WAV has no sample rate";
        return false;
    }

    logWrite(LogLevel::Info, "wav: %s (%dch, %d-bit, %.0f Hz, %s)", path_.c_str(), channels_, bits_, sampleRate_,
             channels_ == 2 ? "IQ" : "real");
    if (channels_ != 2)
    {
        logWrite(LogLevel::Info, "wav: mono file treated as real signal (Q=0); use --offset to pick a channel");
    }

    cb_ = std::move(cb);
    progress_.store(0.0);
    running_.store(true);
    thread_ = std::thread([this]() { playLoop(); });
    return true;
}

void WavFileSource::stop()
{
    running_.store(false);
    if (thread_.joinable())
    {
        thread_.join();
    }
    cb_ = nullptr;
}

void WavFileSource::playLoop()
{
    WavReader reader;
    std::string err;
    if (!reader.open(path_, err))
    {
        logWrite(LogLevel::Info, "wav: reopen failed: %s", err.c_str());
        running_.store(false);
        return;
    }

    const int kFrames = 32768;
    std::vector<float> raw((size_t)kFrames * channels_);
    std::vector<float> iq((size_t)kFrames * 2);

    using clock = std::chrono::steady_clock;
    auto t0     = clock::now();
    uint64_t played = 0;

    while (running_.load())
    {
        int frames = reader.readFrames(raw.data(), kFrames);
        if (frames == 0)
        {
            if (loop_.load())
            {
                reader.rewind();
                t0     = clock::now();
                played = 0;
                continue;
            }
            break;
        }

        if (channels_ == 2)
        {
            cb_(raw.data(), frames);
        }
        else
        {
            for (int i = 0; i < frames; ++i)
            {
                iq[(size_t)i * 2]     = raw[(size_t)i];
                iq[(size_t)i * 2 + 1] = 0.0f;
            }
            cb_(iq.data(), frames);
        }

        played += (uint64_t)frames;
        uint64_t total = reader.totalFrames();
        progress_.store(total ? (double)reader.framePos() / (double)total : 0.0);

        auto target = t0 + std::chrono::duration_cast<clock::duration>(
                               std::chrono::duration<double>((double)played / sampleRate_));
        std::this_thread::sleep_until(target);
    }

    running_.store(false);
}
