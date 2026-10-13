// FM quadrature discriminator. Converts a complex baseband channel to a real
// audio stream (instantaneous frequency), with a one-pole DC blocker and a
// boost-only AGC so quiet stations stay audible and loud ones do not clip.
#pragma once

#include <complex>
#include <vector>

class FmDemod
{
public:
    explicit FmDemod(double sampleRate);

    void reset()
    {
        haveLast_ = false;
        dc_       = 0.0f;
        env_      = 0.0f;
        gain_     = 1.0f;
    }

    void process(const double* iq, int nComplex, std::vector<float>& out);

private:
    double               fs_ = 24000.0;
    std::complex<double> last_{1.0, 0.0};
    bool                 haveLast_ = false;
    float                dc_       = 0.0f;

    // AGC state.
    float env_  = 0.0f;
    float gain_ = 1.0f;
    float attackRate_  = 0.0f;
    float releaseRate_ = 0.0f;
    float gainRate_    = 0.0f;
    float target_      = 0.6f;
    float maxGain_     = 8.0f;
};
