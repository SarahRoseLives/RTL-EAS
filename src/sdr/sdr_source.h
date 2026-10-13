// Abstract SDR/IQ source. A source runs on its own thread and pushes
// interleaved float IQ (I,Q pairs in [-1,1]) through the supplied callback.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct SdrDeviceInfo
{
    int         index = 0;
    std::string name;
    std::string serial;
};

using SdrSampleCb = std::function<void(const float* iq, int nComplex)>;

class SdrSource
{
public:
    virtual ~SdrSource() = default;

    virtual std::vector<SdrDeviceInfo> listDevices() = 0;

    virtual void setCenterFreq(double hz) = 0;
    virtual void setSampleRate(double hz) = 0;
    virtual void setGain(double db)       = 0; // <0 => auto/AGC
    virtual void setPpm(double ppm)       = 0;
    virtual void setBiasTee(bool on) { (void)on; }

    virtual double centerFreq() const = 0;
    virtual double sampleRate() const = 0;

    virtual bool start(int deviceIndex, SdrSampleCb cb, std::string& err) = 0;
    virtual void stop()                                                    = 0;
    virtual bool running() const                                           = 0;
};
