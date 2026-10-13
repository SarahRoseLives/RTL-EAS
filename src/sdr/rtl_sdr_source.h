// RTL-SDR source using librtlsdr directly. The async read callback converts
// unsigned 8-bit IQ to float and applies a gentle one-pole DC blocker.
#pragma once

#include "sdr/sdr_source.h"

#include <atomic>
#include <thread>
#include <vector>

struct rtlsdr_dev;

class RtlSdrSource : public SdrSource
{
public:
    RtlSdrSource()           = default;
    ~RtlSdrSource() override;

    std::vector<SdrDeviceInfo> listDevices() override;

    void setCenterFreq(double hz) override;
    void setSampleRate(double hz) override;
    void setGain(double db) override;
    void setPpm(double ppm) override;
    void setBiasTee(bool on) override;

    double centerFreq() const override { return centerFreq_; }
    double sampleRate() const override { return sampleRate_; }
    double actualCenterFreq() const { return actualCenter_.load(); }
    const std::string& tunerType() const { return tunerType_; }

    bool start(int deviceIndex, SdrSampleCb cb, std::string& err) override;
    void stop() override;
    bool running() const override { return running_.load(); }

private:
    static void asyncTrampoline(unsigned char* buf, uint32_t len, void* ctx);
    void        onAsync(unsigned char* buf, uint32_t len);
    void        applyGain();

    rtlsdr_dev*        dev_ = nullptr;
    std::thread        readThread_;
    std::atomic<bool>  running_{false};
    SdrSampleCb        cb_;

    double centerFreq_ = 162.550e6;
    double sampleRate_ = 2.4e6;
    double gainDb_     = -1.0;
    double ppm_        = 0.0;
    bool   biasTee_    = false;

    std::atomic<double> actualCenter_{0.0};
    std::string         tunerType_ = "-";

    float dcOffRe_ = 0.0f, dcOffIm_ = 0.0f;
    float dcRate_ = 2.0e-5f;

    std::vector<float> scratch_;
};
