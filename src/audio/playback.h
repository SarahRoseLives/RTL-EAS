// Simple waveform-audio playback of the demodulated channel. Windows (winmm)
// implementation; a no-op stub elsewhere.
#pragma once

#include <cstdint>
#include <vector>

class Playback
{
public:
    Playback() = default;
    ~Playback();

    // Returns false if the device rejects the format; playback is then disabled.
    bool open(int sampleRate, int channels);
    void write(const float* interleaved, int frames);
    void close();
    bool isOpen() const { return open_; }

private:
    bool open_ = false;
    int  sampleRate_ = 0;
    int  channels_ = 0;
    int  frameBytes_ = 0;

    struct Impl;
    Impl* impl_ = nullptr;
};
