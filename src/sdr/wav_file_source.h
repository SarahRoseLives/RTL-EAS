// WAV-file IQ source: replays an offline capture through the same pipeline as
// a live SDR. Stereo files are I (ch0) / Q (ch1); mono files are treated as a
// real signal with Q = 0. Playback is paced to real time so a long capture is
// processed at the rate the DSP expects.
#pragma once

#include "sdr/sdr_source.h"
#include "util/wav_io.h"

#include <atomic>
#include <thread>

class WavFileSource : public SdrSource
{
public:
    WavFileSource()           = default;
    ~WavFileSource() override;

    void setPath(const std::string& path) { path_ = path; }
    void setLoop(bool on) { loop_.store(on); }

    std::vector<SdrDeviceInfo> listDevices() override { return {}; }

    void setCenterFreq(double hz) override { centerFreq_ = hz; }
    void setSampleRate(double hz) override { sampleRate_ = hz; }
    void setGain(double) override {}
    void setPpm(double) override {}

    double centerFreq() const override { return centerFreq_; }
    double sampleRate() const override { return sampleRate_; }
    double progress() const { return progress_.load(); }
    bool   isComplex() const { return channels_ == 2; }

    bool start(int deviceIndex, SdrSampleCb cb, std::string& err) override;
    void stop() override;
    bool running() const override { return running_.load(); }

private:
    void playLoop();

    std::string         path_;
    std::thread         thread_;
    std::atomic<bool>   running_{false};
    std::atomic<bool>   loop_{false};
    std::atomic<double> progress_{0.0};
    SdrSampleCb         cb_;

    double centerFreq_ = 162.550e6;
    double sampleRate_ = 0.0;
    int    channels_   = 2;
    int    bits_       = 16;
};
