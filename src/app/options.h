// Command-line options.
#pragma once

#include <string>

struct Options
{
    bool list = false;

    int    device     = 0;
    double freqMHz    = 162.550;
    double sampleRate = 2400000.0;
    double gainDb     = -1.0; // <0 => auto/AGC
    double ppm        = 0.0;

    bool   hasOffset  = false;
    double offsetHz   = 0.0;
    bool   autoOffset = false;

    double channelRate = 24000.0;
    double channelBw   = 12000.0;

    std::string wav;      // IQ capture input
    bool        wavLoop = false;
    std::string audio;    // already-demodulated audio input

    std::string record;   // demodulated audio output
    bool        play   = false;
    bool        json   = false;
    bool        verbose = false;
    bool        showHelp = false;
    double      duration = 0.0; // seconds; 0 = run until Ctrl-C / EOF
};

bool parseOptions(int argc, char** argv, Options& out, std::string& err);
void printUsage();

double parseFreqMHz(const std::string& s); // "162.55" -> 162.55e6
