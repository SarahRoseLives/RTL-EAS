#include "sdr/rtl_sdr_source.h"

#include "util/log.h"

#include <cmath>
#include <cstdlib>

#include <rtl-sdr.h>

RtlSdrSource::~RtlSdrSource()
{
    stop();
}

std::vector<SdrDeviceInfo> RtlSdrSource::listDevices()
{
    std::vector<SdrDeviceInfo> out;
    uint32_t                   count = rtlsdr_get_device_count();
    for (uint32_t i = 0; i < count; ++i)
    {
        char manufact[256] = {0}, product[256] = {0}, serial[256] = {0};
        rtlsdr_get_device_usb_strings(i, manufact, product, serial);
        SdrDeviceInfo info;
        info.index  = (int)i;
        info.name   = std::string(rtlsdr_get_device_name(i));
        info.serial = serial;
        out.push_back(info);
    }
    return out;
}

void RtlSdrSource::applyGain()
{
    if (!dev_)
    {
        return;
    }

    if (gainDb_ < 0.0)
    {
        rtlsdr_set_tuner_gain_mode(dev_, 0);
        rtlsdr_set_agc_mode(dev_, 1);
        return;
    }

    rtlsdr_set_agc_mode(dev_, 0);
    rtlsdr_set_tuner_gain_mode(dev_, 1);

    int num = rtlsdr_get_tuner_gains(dev_, nullptr);
    if (num <= 0)
    {
        return;
    }
    std::vector<int> gains((size_t)num);
    rtlsdr_get_tuner_gains(dev_, gains.data());

    int target  = (int)std::lround(gainDb_ * 10.0);
    int best    = gains[0];
    int bestDif = std::abs(target - gains[0]);
    for (int i = 1; i < num; ++i)
    {
        int diff = std::abs(target - gains[i]);
        if (diff < bestDif)
        {
            best    = gains[i];
            bestDif = diff;
        }
    }
    rtlsdr_set_tuner_gain(dev_, best);
}

bool RtlSdrSource::start(int deviceIndex, SdrSampleCb cb, std::string& err)
{
    if (running_.load())
    {
        return true;
    }

    uint32_t count = rtlsdr_get_device_count();
    if (count == 0)
    {
        err = "no RTL-SDR devices found";
        return false;
    }
    if (deviceIndex < 0 || (uint32_t)deviceIndex >= count)
    {
        err = "RTL-SDR device index out of range";
        return false;
    }

    if (rtlsdr_open(&dev_, (uint32_t)deviceIndex) < 0 || !dev_)
    {
        err = "failed to open RTL-SDR device";
        dev_ = nullptr;
        return false;
    }

    switch (rtlsdr_get_tuner_type(dev_))
    {
    case RTLSDR_TUNER_E4000:  tunerType_ = "E4000";  break;
    case RTLSDR_TUNER_FC0012: tunerType_ = "FC0012"; break;
    case RTLSDR_TUNER_FC0013: tunerType_ = "FC0013"; break;
    case RTLSDR_TUNER_FC2580: tunerType_ = "FC2580"; break;
    case RTLSDR_TUNER_R820T:  tunerType_ = "R820T";  break;
    case RTLSDR_TUNER_R828D:  tunerType_ = "R828D";  break;
    default:                  tunerType_ = "unknown"; break;
    }

    rtlsdr_set_sample_rate(dev_, (uint32_t)sampleRate_);
    uint32_t actual = rtlsdr_get_sample_rate(dev_);
    if (actual)
    {
        sampleRate_ = (double)actual;
    }

    if (ppm_ != 0.0)
    {
        rtlsdr_set_freq_correction(dev_, (int)std::lround(ppm_));
    }

    rtlsdr_set_center_freq(dev_, (uint32_t)centerFreq_);
    actualCenter_.store((double)rtlsdr_get_center_freq(dev_));
    applyGain();
    rtlsdr_set_bias_tee(dev_, biasTee_ ? 1 : 0);
    rtlsdr_reset_buffer(dev_);

    cb_       = std::move(cb);
    dcOffRe_  = dcOffIm_ = 0.0f;
    dcRate_   = (float)(50.0 / sampleRate_);
    running_.store(true);

    readThread_ = std::thread([this]() {
        rtlsdr_read_async(dev_, &RtlSdrSource::asyncTrampoline, this, 15, 32768);
        running_.store(false);
    });

    logWrite(LogLevel::Info, "rtl-sdr: device %d, tuner %s, %.6f MHz, %.0f Hz S/s%s", deviceIndex, tunerType_.c_str(),
             actualCenter_.load() / 1e6, sampleRate_, gainDb_ < 0.0 ? ", AGC" : "");
    return true;
}

void RtlSdrSource::stop()
{
    if (dev_)
    {
        rtlsdr_cancel_async(dev_);
    }
    if (readThread_.joinable())
    {
        readThread_.join();
    }
    if (dev_)
    {
        rtlsdr_close(dev_);
        dev_ = nullptr;
    }
    running_.store(false);
    cb_ = nullptr;
}

void RtlSdrSource::asyncTrampoline(unsigned char* buf, uint32_t len, void* ctx)
{
    static_cast<RtlSdrSource*>(ctx)->onAsync(buf, len);
}

void RtlSdrSource::onAsync(unsigned char* buf, uint32_t len)
{
    if (!running_.load() || !cb_)
    {
        return;
    }

    uint32_t num = len / 2;
    if (scratch_.size() < (size_t)num * 2)
    {
        scratch_.resize((size_t)num * 2);
    }

    float* out    = scratch_.data();
    float  offRe  = dcOffRe_;
    float  offIm  = dcOffIm_;
    const float rate = dcRate_;
    for (uint32_t i = 0; i < num; ++i)
    {
        float re = ((int)buf[i * 2] - 128) * (1.0f / 128.0f);
        float im = ((int)buf[i * 2 + 1] - 128) * (1.0f / 128.0f);
        float ore = re - offRe;
        offRe += ore * rate;
        float oim = im - offIm;
        offIm += oim * rate;
        out[i * 2]     = ore;
        out[i * 2 + 1] = oim;
    }
    dcOffRe_ = offRe;
    dcOffIm_ = offIm;

    cb_(out, (int)num);
}

void RtlSdrSource::setCenterFreq(double hz)
{
    centerFreq_ = hz;
    if (dev_)
    {
        rtlsdr_set_center_freq(dev_, (uint32_t)hz);
        actualCenter_.store((double)rtlsdr_get_center_freq(dev_));
    }
}

void RtlSdrSource::setSampleRate(double hz)
{
    sampleRate_ = hz;
    if (dev_)
    {
        rtlsdr_set_sample_rate(dev_, (uint32_t)hz);
        uint32_t actual = rtlsdr_get_sample_rate(dev_);
        if (actual)
        {
            sampleRate_ = (double)actual;
        }
    }
}

void RtlSdrSource::setGain(double db)
{
    gainDb_ = db;
    if (dev_)
    {
        applyGain();
    }
}

void RtlSdrSource::setPpm(double ppm)
{
    ppm_ = ppm;
    if (dev_)
    {
        rtlsdr_set_freq_correction(dev_, (int)std::lround(ppm));
    }
}

void RtlSdrSource::setBiasTee(bool on)
{
    biasTee_ = on;
    if (dev_)
    {
        rtlsdr_set_bias_tee(dev_, on ? 1 : 0);
    }
}
