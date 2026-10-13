#include "app/pipeline.h"

#include "dsp/fft.h"
#include "util/log.h"

#include <cmath>
#include <complex>

namespace
{

// Coarse channel detection: FFT the first block and return the offset (Hz) of
// the strongest bin, ignoring the DC spike and the band edges.
double estimateOffset(const float* iq, int nComplex, double fs)
{
    const int win    = 16384;
    const int maxWin = 8;
    int       N      = win;
    if (nComplex < win)
    {
        N = 1;
        while (N * 2 <= nComplex)
        {
            N <<= 1;
        }
    }
    if (N < 64)
    {
        return 0.0;
    }

    double                            binHz = fs / (double)N;
    std::vector<double>               power((size_t)N / 2, 0.0);
    std::vector<std::complex<double>>  buf((size_t)N);

    int windows = 0;
    for (int start = 0; start + N <= nComplex && windows < maxWin; start += N)
    {
        for (int i = 0; i < N; ++i)
        {
            double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (N - 1));
            buf[i]   = std::complex<double>(iq[(start + i) * 2], iq[(start + i) * 2 + 1]) * w;
        }
        fftRadix2(buf);
        for (int k = 0; k < N / 2; ++k)
        {
            power[k] += std::norm(buf[k]);
        }
        ++windows;
    }
    if (windows == 0)
    {
        return 0.0;
    }

    double guard   = 300.0;
    double bestMag = -1.0;
    double bestHz  = 0.0;
    for (int k = 1; k < N; ++k)
    {
        double f = (k < N / 2) ? k * binHz : (k - N) * binHz;
        if (std::fabs(f) < guard || std::fabs(f) > fs / 2.0 - guard)
        {
            continue;
        }
        double mag = power[k < N / 2 ? k : N - k];
        if (mag > bestMag)
        {
            bestMag = mag;
            bestHz  = f;
        }
    }
    return bestHz;
}

} // namespace

Pipeline::Pipeline(const Options& opt, double inputRate, SameDecoder::MessageCb cb)
    : opt_(opt), inputRate_(inputRate), cb_(std::move(cb))
{
    if (!opt_.autoOffset)
    {
        createDdc(opt_.hasOffset ? opt_.offsetHz : 0.0);
    }
}

Pipeline::~Pipeline()
{
    finish();
}

void Pipeline::createDdc(double offset)
{
    ddc_ = std::make_unique<Ddc>(inputRate_, offset, opt_.channelRate, opt_.channelBw);
    double fs = ddc_->outputRate();
    fm_       = std::make_unique<FmDemod>(fs);
    dec_      = std::make_unique<SameDecoder>(fs, cb_);

    logWrite(LogLevel::Info, "channel: %.0f Hz rate, %+.0f Hz offset, %.0f Hz BW", fs, offset, opt_.channelBw);

    if (!opt_.record.empty())
    {
        std::string err;
        if (rec_.open(opt_.record, (int)fs, 1, err))
        {
            logWrite(LogLevel::Info, "recording demodulated audio to %s", opt_.record.c_str());
        }
        else
        {
            logWrite(LogLevel::Info, "record failed: %s", err.c_str());
        }
    }
    if (opt_.play)
    {
        play_.open((int)fs, 1);
    }

    ready_ = true;
}

void Pipeline::ensureDdc(const float* iq, int nComplex)
{
    if (ready_)
    {
        return;
    }
    createDdc(opt_.hasOffset ? opt_.offsetHz : 0.0);
}

void Pipeline::processIq(const float* iq, int nComplex)
{
    ch_.clear();
    ddc_->process(iq, nComplex, ch_);
    int nc = (int)(ch_.size() / 2);
    if (nc <= 0)
    {
        return;
    }

    audio_.clear();
    fm_->process(ch_.data(), nc, audio_);
    if (audio_.empty())
    {
        return;
    }

    if (audioSink_)
    {
        audioSink_(audio_.data(), (int)audio_.size());
    }

    if (rec_.isOpen())
    {
        rec_.write(audio_.data(), (int)audio_.size());
    }
    if (play_.isOpen())
    {
        play_.write(audio_.data(), (int)audio_.size());
    }
    if (dec_)
    {
        dec_->push(audio_.data(), (int)audio_.size());
    }
}

void Pipeline::feed(const float* iq, int nComplex)
{
    if (nComplex <= 0)
    {
        return;
    }

    if (!ready_ && opt_.autoOffset)
    {
        probe_.insert(probe_.end(), iq, iq + (size_t)nComplex * 2);
        if ((double)(probe_.size() / 2) < 0.25 * inputRate_)
        {
            return;
        }
        int    nProbe = (int)(probe_.size() / 2);
        double offset = estimateOffset(probe_.data(), nProbe, inputRate_);
        logWrite(LogLevel::Info, "auto-offset: strongest channel at %+.0f Hz", offset);
        createDdc(offset);
        processIq(probe_.data(), nProbe);
        probe_.clear();
        probe_.shrink_to_fit();
        return;
    }

    ensureDdc(iq, nComplex);
    processIq(iq, nComplex);
}

void Pipeline::finish()
{
    if (dec_)
    {
        dec_->flush();
    }
    rec_.close();
    play_.close();
    ready_ = false;
}
