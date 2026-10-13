// Minimal WAV read/write helpers.
//
// WavReader parses RIFF/WAVE files with 8/16/24/32-bit integer or 32-bit float
// samples and 1-2 channels, returning interleaved float frames in [-1,1].
// WavWriter produces a canonical 16-bit PCM WAV, finalizing the RIFF/data sizes
// on close so the file is always playable even if the process is killed.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

struct WavFormat
{
    int  channels   = 1;
    int  sampleRate = 0;
    int  bits       = 16;
    bool isFloat    = false;
};

class WavReader
{
public:
    WavReader() = default;
    ~WavReader();
    WavReader(const WavReader&)            = delete;
    WavReader& operator=(const WavReader&) = delete;

    bool open(const std::string& path, std::string& err);
    void close();

    const WavFormat& format() const { return fmt_; }
    uint64_t totalFrames() const { return totalFrames_; }
    uint64_t framePos() const { return framePos_; }
    void     rewind();

    // Reads up to maxFrames interleaved frames. Returns frames read (0 at EOF).
    int readFrames(float* out, int maxFrames);

private:
    std::FILE* f_          = nullptr;
    WavFormat  fmt_{};
    long       dataOffset_ = 0;
    uint64_t   dataBytes_  = 0;
    uint64_t   totalFrames_ = 0;
    uint64_t   framePos_    = 0;
};

class WavWriter
{
public:
    WavWriter() = default;
    ~WavWriter();
    WavWriter(const WavWriter&)            = delete;
    WavWriter& operator=(const WavWriter&) = delete;

    bool open(const std::string& path, int sampleRate, int channels, std::string& err);
    void write(const float* interleaved, int frames);
    bool isOpen() const { return f_ != nullptr; }
    void close();

private:
    std::FILE* f_ = nullptr;
    int        channels_ = 1;
    uint64_t   frames_   = 0;
};
