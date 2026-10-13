// Digital downconverter: NCO mix to baseband + cascaded FIR decimation.
// Extracts one narrow NFM channel from the wideband IQ stream and produces
// interleaved double IQ at ~targetRate.
#pragma once

#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class FirDecimator
{
public:
    void design(int factor, double inRate, double cutoffHz, int taps)
    {
        factor_ = factor;
        if (taps < factor * 2)
        {
            taps = factor * 2;
        }
        if ((taps & 1) == 0)
        {
            ++taps;
        }
        h_.resize(taps);
        double fc  = cutoffHz / inRate;
        double sum = 0.0;
        int    M   = taps - 1;
        for (int n = 0; n < taps; ++n)
        {
            double x    = n - M / 2.0;
            double sinc = (x == 0.0) ? 2.0 * fc : std::sin(2.0 * M_PI * fc * x) / (M_PI * x);
            double w    = 0.54 - 0.46 * std::cos(2.0 * M_PI * n / M);
            h_[n]       = sinc * w;
            sum += h_[n];
        }
        for (double& v : h_)
        {
            v /= sum;
        }

        size_t cap = 1;
        while (cap < (size_t)taps)
        {
            cap <<= 1;
        }
        mask_ = cap - 1;
        delay_.assign(cap, std::complex<double>(0, 0));
        wpos_  = 0;
        count_ = 0;
    }

    bool push(std::complex<double> in, std::complex<double>* out)
    {
        delay_[wpos_ & mask_] = in;
        ++wpos_;
        if (++count_ < factor_)
        {
            return false;
        }
        count_ = 0;

        std::complex<double> acc(0, 0);
        int                  L   = (int)h_.size();
        uint64_t             idx = wpos_ - 1;
        for (int k = 0; k < L; ++k)
        {
            acc += delay_[(idx - (uint64_t)k) & mask_] * h_[k];
        }
        *out = acc;
        return true;
    }

private:
    int                             factor_ = 1;
    int                             count_  = 0;
    uint64_t                        wpos_   = 0;
    uint64_t                        mask_   = 0;
    std::vector<double>             h_;
    std::vector<std::complex<double>> delay_;
};

class Ddc
{
public:
    Ddc(double Fs, double offsetHz, double targetRate, double channelBwHz)
    {
        Fs_       = Fs;
        double inc = -2.0 * M_PI * offsetHz / Fs;
        rot_      = std::complex<double>(std::cos(inc), std::sin(inc));
        osc_      = std::complex<double>(1.0, 0.0);

        int totalDecim = (int)std::lround(Fs / targetRate);
        if (totalDecim < 1)
        {
            totalDecim = 1;
        }

        auto smallFactors = [](int n) {
            for (int p : {2, 3, 5, 7})
            {
                while (n % p == 0)
                {
                    n /= p;
                }
            }
            return n == 1;
        };
        if (totalDecim > 1 && !smallFactors(totalDecim))
        {
            for (int delta = 1; delta <= totalDecim / 2; ++delta)
            {
                if (totalDecim - delta >= 1 && smallFactors(totalDecim - delta))
                {
                    totalDecim -= delta;
                    break;
                }
                if (smallFactors(totalDecim + delta))
                {
                    totalDecim += delta;
                    break;
                }
            }
        }

        std::vector<int> factors;
        int              rem = totalDecim;
        while (rem > 1)
        {
            int f = 1;
            for (int cand = 8; cand >= 2; --cand)
            {
                if (rem % cand == 0)
                {
                    f = cand;
                    break;
                }
            }
            if (f == 1)
            {
                factors.push_back(rem);
                rem = 1;
            }
            else
            {
                factors.push_back(f);
                rem /= f;
            }
        }
        if (factors.empty())
        {
            factors.push_back(1);
        }

        double rate = Fs_;
        stages_.resize(factors.size());
        for (size_t i = 0; i < factors.size(); ++i)
        {
            double outRate = rate / factors[i];
            bool   last    = (i + 1 == factors.size());
            double cutoff  = last ? std::min(channelBwHz * 0.5, outRate * 0.45) : outRate * 0.40;
            int    taps    = last ? 24 * factors[i] + 128 : 16 * factors[i] + 64;
            stages_[i].design(factors[i], rate, cutoff, taps);
            rate = outRate;
        }
        outRate_ = rate;
    }

    double outputRate() const { return outRate_; }

    void process(const float* iq, int nComplex, std::vector<double>& out)
    {
        for (int i = 0; i < nComplex; ++i)
        {
            std::complex<double> s((double)iq[i * 2], (double)iq[i * 2 + 1]);
            s *= osc_;
            osc_ *= rot_;
            if ((++oscCount_ & 1023) == 0)
            {
                osc_ /= std::abs(osc_);
            }
            feedStages(s, 0, out);
        }
    }

private:
    void feedStages(std::complex<double> s, size_t stage, std::vector<double>& out)
    {
        if (stage >= stages_.size())
        {
            out.push_back(s.real());
            out.push_back(s.imag());
            return;
        }
        std::complex<double> o;
        if (stages_[stage].push(s, &o))
        {
            feedStages(o, stage + 1, out);
        }
    }

    double                            Fs_ = 0, outRate_ = 0;
    std::complex<double>              osc_{1.0, 0.0}, rot_{1.0, 0.0};
    uint64_t                          oscCount_ = 0;
    std::vector<FirDecimator>         stages_;
};
