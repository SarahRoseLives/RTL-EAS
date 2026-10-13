// rtl-eas - NOAA SAME weather radio decoder (SDR-first).
#include "app/options.h"
#include "app/pipeline.h"
#include "eas/same_message.h"
#include "sdr/rtl_sdr_source.h"
#include "sdr/wav_file_source.h"
#include "util/log.h"
#include "util/wav_io.h"
#include "version.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace
{

std::atomic<bool> g_stop{false};

void onSignal(int)
{
    g_stop.store(true);
}

#if defined(_WIN32)
BOOL WINAPI onCtrl(DWORD)
{
    g_stop.store(true);
    return TRUE;
}
#endif

void installSignals()
{
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
#if defined(_WIN32)
    SetConsoleCtrlHandler(onCtrl, TRUE);
#endif
}

void reportMessage(const Options& opt, const SameMessage& m)
{
    if (opt.json)
    {
        std::printf("%s\n", sameJsonMessage(m).c_str());
    }
    else
    {
        std::printf("%s", sameFormatMessage(m).c_str());
    }
    std::fflush(stdout);
}

int listDevices()
{
    RtlSdrSource             src;
    std::vector<SdrDeviceInfo> devs = src.listDevices();
    if (devs.empty())
    {
        std::printf("No RTL-SDR devices found.\n");
        return 1;
    }
    std::printf("RTL-SDR devices:\n");
    for (const SdrDeviceInfo& d : devs)
    {
        std::printf("  [%d] %s  SN:%s\n", d.index, d.name.c_str(), d.serial.c_str());
    }
    return 0;
}

int runIq(const Options& opt, SameDecoder::MessageCb cb)
{
    std::unique_ptr<SdrSource> src;
    double                     inputRate = opt.sampleRate;
    std::unique_ptr<WavFileSource> wavSrc;

    if (!opt.wav.empty())
    {
        WavReader   probe;
        std::string err;
        if (!probe.open(opt.wav, err))
        {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        inputRate = (double)probe.format().sampleRate;
        probe.close();

        wavSrc = std::make_unique<WavFileSource>();
        wavSrc->setPath(opt.wav);
        wavSrc->setLoop(opt.wavLoop);
        src = std::move(wavSrc);
    }
    else
    {
        src = std::make_unique<RtlSdrSource>();
    }

    Pipeline pipeline(opt, inputRate, std::move(cb));

    src->setCenterFreq(opt.freqMHz * 1e6);
    src->setSampleRate(inputRate);
    src->setGain(opt.gainDb);
    src->setPpm(opt.ppm);

    std::string err;
    bool        ok = src->start(opt.device, [&pipeline](const float* iq, int n) { pipeline.feed(iq, n); }, err);
    if (!ok)
    {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    std::printf("rtl-eas %s - decoding SAME from %s @ %.6f MHz. Ctrl-C to stop.\n", RTLEAS_VERSION,
                opt.wav.empty() ? "RTL-SDR" : opt.wav.c_str(), opt.freqMHz);

    auto startTime = std::chrono::steady_clock::now();
    while (!g_stop.load() && src->running())
    {
        if (opt.duration > 0.0)
        {
            double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= opt.duration)
            {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    src->stop();
    pipeline.finish();
    return 0;
}

int runAudio(const Options& opt, SameDecoder::MessageCb cb)
{
    WavReader   reader;
    std::string err;
    if (!reader.open(opt.audio, err))
    {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    double fs       = (double)reader.format().sampleRate;
    int    channels = reader.format().channels;
    SameDecoder dec(fs, std::move(cb));

    WavWriter  rec;
    Playback   play;
    if (!opt.record.empty())
    {
        std::string e;
        if (rec.open(opt.record, (int)fs, 1, e))
        {
            logWrite(LogLevel::Info, "recording to %s", opt.record.c_str());
        }
    }
    if (opt.play)
    {
        play.open((int)fs, 1);
    }

    std::printf("rtl-eas %s - decoding SAME from %s @ %.0f Hz. Ctrl-C to stop.\n", RTLEAS_VERSION, opt.audio.c_str(),
                fs);

    const int          kFrames = 8192;
    std::vector<float> in((size_t)kFrames * channels);
    std::vector<float> mono((size_t)kFrames);

    int frames;
    while (!g_stop.load() && (frames = reader.readFrames(in.data(), kFrames)) > 0)
    {
        for (int i = 0; i < frames; ++i)
        {
            float v = 0.0f;
            for (int c = 0; c < channels; ++c)
            {
                v += in[(size_t)i * channels + c];
            }
            mono[i] = v / (float)channels;
        }
        if (rec.isOpen())
        {
            rec.write(mono.data(), frames);
        }
        if (play.isOpen())
        {
            play.write(mono.data(), frames);
        }
        dec.push(mono.data(), frames);
    }
    dec.flush();
    rec.close();
    play.close();
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    Options     opt;
    std::string err;
    if (!parseOptions(argc, argv, opt, err))
    {
        std::fprintf(stderr, "error: %s\n\n", err.c_str());
        printUsage();
        return 2;
    }
    if (opt.showHelp)
    {
        printUsage();
        return 0;
    }

    logSetVerbose(opt.verbose);
    installSignals();

    if (opt.list)
    {
        return listDevices();
    }

    SameDecoder::MessageCb cb = [&opt](const SameMessage& m) { reportMessage(opt, m); };

    if (!opt.audio.empty())
    {
        return runAudio(opt, std::move(cb));
    }
    return runIq(opt, std::move(cb));
}
