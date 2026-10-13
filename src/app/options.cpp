#include "app/options.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

double parseFreqMHz(const std::string& s)
{
    return std::atof(s.c_str()) * 1e6;
}

void printUsage()
{
    std::printf(
        "rtl-eas - NOAA SAME weather radio decoder (SDR-first)\n"
        "\n"
        "Usage:\n"
        "  rtl-eas [options]\n"
        "\n"
        "Source:\n"
        "  --device N        RTL-SDR device index (default 0)\n"
        "  --freq MHZ        center frequency (default 162.550)\n"
        "  --rate HZ         sample rate (default 2400000)\n"
        "  --gain DB         tuner gain in dB, or 'auto' (default auto)\n"
        "  --ppm N           frequency correction in ppm\n"
        "  --offset HZ       channel offset from center (default 0)\n"
        "  --auto-offset     auto-detect the strongest channel offset\n"
        "  --wav FILE        decode an IQ WAV capture instead of the RTL-SDR\n"
        "  --loop            loop the WAV capture\n"
        "  --audio FILE      decode an already-demodulated audio WAV file\n"
        "\n"
        "Output:\n"
        "  --record FILE     write the demodulated audio to a WAV file\n"
        "  --play            play the demodulated audio (Windows)\n"
        "  --json            emit each message as a single JSON line\n"
        "  --verbose         verbose logging\n"
        "  --duration N      stop after N seconds (0 = run until Ctrl-C)\n"
        "  --list            list RTL-SDR devices and exit\n"
        "  -h, --help        show this help\n"
        "\n"
        "NOAA weather radio channels (MHz): 162.400 162.425 162.450 162.475\n"
        "                                    162.500 162.525 162.550\n");
}

namespace
{

bool needValue(int argc, char** argv, int& i, const char* name, std::string& err)
{
    if (i + 1 >= argc)
    {
        err = std::string("missing value for ") + name;
        return false;
    }
    ++i;
    return true;
}

} // namespace

bool parseOptions(int argc, char** argv, Options& o, std::string& err)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "-h" || a == "--help")
        {
            o.showHelp = true;
        }
        else if (a == "--list")
        {
            o.list = true;
        }
        else if (a == "--device")
        {
            if (!needValue(argc, argv, i, "--device", err))
            {
                return false;
            }
            o.device = std::atoi(argv[i]);
        }
        else if (a == "--freq")
        {
            if (!needValue(argc, argv, i, "--freq", err))
            {
                return false;
            }
            o.freqMHz = std::atof(argv[i]) > 1000.0 ? std::atof(argv[i]) / 1e6 : std::atof(argv[i]);
        }
        else if (a == "--rate")
        {
            if (!needValue(argc, argv, i, "--rate", err))
            {
                return false;
            }
            o.sampleRate = std::atof(argv[i]);
        }
        else if (a == "--gain")
        {
            if (!needValue(argc, argv, i, "--gain", err))
            {
                return false;
            }
            if (std::strcmp(argv[i], "auto") == 0)
            {
                o.gainDb = -1.0;
            }
            else
            {
                o.gainDb = std::atof(argv[i]);
            }
        }
        else if (a == "--ppm")
        {
            if (!needValue(argc, argv, i, "--ppm", err))
            {
                return false;
            }
            o.ppm = std::atof(argv[i]);
        }
        else if (a == "--offset")
        {
            if (!needValue(argc, argv, i, "--offset", err))
            {
                return false;
            }
            o.offsetHz  = std::atof(argv[i]);
            o.hasOffset = true;
        }
        else if (a == "--auto-offset")
        {
            o.autoOffset = true;
        }
        else if (a == "--channel-rate")
        {
            if (!needValue(argc, argv, i, "--channel-rate", err))
            {
                return false;
            }
            o.channelRate = std::atof(argv[i]);
        }
        else if (a == "--wav")
        {
            if (!needValue(argc, argv, i, "--wav", err))
            {
                return false;
            }
            o.wav = argv[i];
        }
        else if (a == "--loop")
        {
            o.wavLoop = true;
        }
        else if (a == "--audio")
        {
            if (!needValue(argc, argv, i, "--audio", err))
            {
                return false;
            }
            o.audio = argv[i];
        }
        else if (a == "--record")
        {
            if (!needValue(argc, argv, i, "--record", err))
            {
                return false;
            }
            o.record = argv[i];
        }
        else if (a == "--play")
        {
            o.play = true;
        }
        else if (a == "--json")
        {
            o.json = true;
        }
        else if (a == "--duration")
        {
            if (!needValue(argc, argv, i, "--duration", err))
            {
                return false;
            }
            o.duration = std::atof(argv[i]);
        }
        else if (a == "--verbose")
        {
            o.verbose = true;
        }
        else
        {
            err = "unknown option: " + a;
            return false;
        }
    }
    return true;
}
