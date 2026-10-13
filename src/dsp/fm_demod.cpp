#include "dsp/fm_demod.h"

#include <cmath>

FmDemod::FmDemod(double sampleRate) : fs_(sampleRate)
{
    attackRate_  = 1.0f - std::exp(-1.0f / (float)(fs_ * 0.003));
    releaseRate_ = 1.0f - std::exp(-1.0f / (float)(fs_ * 0.250));
    gainRate_    = 1.0f - std::exp(-1.0f / (float)(fs_ * 0.030));
}

void FmDemod::process(const double* iq, int nComplex, std::vector<float>& out)
{
    // Instantaneous frequency (Hz) scaled so a nominal +/-5 kHz NBFM deviation
    // maps to roughly +/-0.6 before the AGC.
    const double scale = fs_ / (2.0 * M_PI) / 8000.0;
    const float  dcRate = 0.002f;

    for (int i = 0; i < nComplex; ++i)
    {
        std::complex<double> s(iq[i * 2], iq[i * 2 + 1]);
        if (!haveLast_)
        {
            last_     = s;
            haveLast_ = true;
            continue;
        }
        std::complex<double> d  = s * std::conj(last_);
        last_                   = s;
        double               ph = std::atan2(d.imag(), d.real());
        float                y  = (float)(ph * scale);

        dc_ += (y - dc_) * dcRate;
        y -= dc_;

        // Boost-only AGC: raise quiet audio toward the target, never attenuate
        // loud audio, so the level is consistent between stations.
        float ay   = std::fabs(y);
        float rate = (ay > env_) ? attackRate_ : releaseRate_;
        env_ += (ay - env_) * rate;
        float g = target_ / (env_ + 1.0e-4f);
        if (g < 1.0f)
        {
            g = 1.0f;
        }
        if (g > maxGain_)
        {
            g = maxGain_;
        }
        gain_ += (g - gain_) * gainRate_;

        float o = y * gain_;
        if (o > 1.0f)
        {
            o = 1.0f;
        }
        else if (o < -1.0f)
        {
            o = -1.0f;
        }
        out.push_back(o);
    }
}
