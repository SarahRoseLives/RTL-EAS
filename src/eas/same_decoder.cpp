#include "eas/same_decoder.h"

#include <cmath>
#include <cstring>

namespace
{

constexpr double kMarkHz  = 2083.3333;
constexpr double kSpaceHz = 1562.5;
constexpr double kBaud    = 520.8333;

constexpr double kHpHz = 900.0;
constexpr double kLpHz = 2700.0;

bool isHeaderChar(int c)
{
    if (c < 32 || c >= 127)
    {
        return false;
    }
    return std::strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-+ ", c) != nullptr;
}

int popcount64(uint64_t v)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(v);
#else
    int c = 0;
    while (v)
    {
        v &= v - 1;
        ++c;
    }
    return c;
#endif
}

} // namespace

SameDecoder::Biquad SameDecoder::Biquad::lowpass(double fs, double f, double q)
{
    double w  = 2.0 * M_PI * f / fs;
    double cw = std::cos(w), sw = std::sin(w);
    double al = sw / (2.0 * q);
    double a0 = 1.0 + al;
    Biquad bq;
    bq.b0 = (1.0 - cw) / 2.0 / a0;
    bq.b1 = (1.0 - cw) / a0;
    bq.b2 = bq.b0;
    bq.a1 = (-2.0 * cw) / a0;
    bq.a2 = (1.0 - al) / a0;
    return bq;
}

SameDecoder::Biquad SameDecoder::Biquad::highpass(double fs, double f, double q)
{
    double w  = 2.0 * M_PI * f / fs;
    double cw = std::cos(w), sw = std::sin(w);
    double al = sw / (2.0 * q);
    double a0 = 1.0 + al;
    Biquad bq;
    bq.b0 = (1.0 + cw) / 2.0 / a0;
    bq.b1 = -(1.0 + cw) / a0;
    bq.b2 = bq.b0;
    bq.a1 = (-2.0 * cw) / a0;
    bq.a2 = (1.0 - al) / a0;
    return bq;
}

SameDecoder::SameDecoder(double sampleRate, MessageCb cb) : fs_(sampleRate), cb_(std::move(cb))
{
    spb_ = fs_ / kBaud;
    inc_ = kBaud / fs_;

    L_ = (int)std::lround(spb_);
    if (L_ < 1)
    {
        L_ = 1;
    }

    hp_ = Biquad::highpass(fs_, kHpHz, 0.7071);
    lp_ = Biquad::lowpass(fs_, kLpHz, 0.7071);

    mring_.assign(L_, std::complex<double>(0.0, 0.0));
    sring_.assign(L_, std::complex<double>(0.0, 0.0));

    markRot_  = std::polar(1.0, -2.0 * M_PI * kMarkHz / fs_);
    spaceRot_ = std::polar(1.0, -2.0 * M_PI * kSpaceHz / fs_);

    for (int j = 0; j < 64; ++j)
    {
        prePat_ |= (uint64_t)((0xAB >> (j % 8)) & 1) << j;
    }
    const char* sync = "ZCZC";
    for (int i = 0; i < 4; ++i)
    {
        for (int b = 0; b < 8; ++b)
        {
            zcPat_ |= (uint64_t)((sync[i] >> b) & 1) << (i * 8 + b);
        }
    }
}

void SameDecoder::push(const float* audio, int n)
{
    for (int i = 0; i < n; ++i)
    {
        ++samplePos_;
        double x = lp_.process(hp_.process((double)audio[i]));

        std::complex<double> mm = x * markMix_;
        std::complex<double> sm = x * spaceMix_;
        markMix_ *= markRot_;
        spaceMix_ *= spaceRot_;

        if (rc_ == L_)
        {
            msum_ -= mring_[rp_];
            ssum_ -= sring_[rp_];
        }
        mring_[rp_] = mm;
        sring_[rp_] = sm;
        msum_ += mm;
        ssum_ += sm;
        if (++rp_ >= L_)
        {
            rp_ = 0;
        }
        if (rc_ < L_)
        {
            ++rc_;
        }

        double soft = std::abs(msum_) - std::abs(ssum_);
        int    sgn  = soft > 0.0 ? 1 : -1;

        double prev = acc_;
        acc_ += inc_;
        bool crossed = (prev < 0.5 && acc_ >= 0.5);
        while (acc_ >= 1.0)
        {
            acc_ -= 1.0;
        }
        if (crossed)
        {
            handleBit(soft > 0.0 ? 1 : 0);
        }

        if (rc_ == L_ && lastSgn_ != 0 && sgn != lastSgn_)
        {
            double err = acc_;
            if (err > 0.5)
            {
                err -= 1.0;
            }
            acc_ -= 0.05 * err;
        }
        lastSgn_ = sgn;
    }

    if (!pendingKey_.empty() && samplePos_ - pendingLast_ > (int64_t)(2.5 * fs_))
    {
        emitPending();
    }
}

void SameDecoder::handleBit(int bit)
{
    w64_ = (w64_ >> 1) | ((uint64_t)bit << 63);
    reg_ = (reg_ >> 1) | ((uint32_t)bit << 31);
    ++preambleAge_;

    if (popcount64(w64_ ^ prePat_) <= 4)
    {
        preambleAge_ = 0;
    }

    if (collecting_)
    {
        byteVal_ |= bit << bitInByte_;
        if (++bitInByte_ == 8)
        {
            int c = byteVal_;
            byteVal_   = 0;
            bitInByte_ = 0;
            if (isHeaderChar(c))
            {
                header_.push_back((char)c);
                if (header_.size() > 200)
                {
                    finishHeader();
                }
            }
            else
            {
                finishHeader();
            }
        }
    }

    if (preambleAge_ <= 200 && popcount64((uint64_t)(reg_ ^ zcPat_)) <= 2)
    {
        collecting_  = true;
        header_      = "ZCZC";
        byteVal_     = 0;
        bitInByte_   = 0;
        preambleAge_ = 1000000000;
    }
}

void SameDecoder::finishHeader()
{
    std::string raw = header_;
    collecting_     = false;
    header_.clear();
    byteVal_   = 0;
    bitInByte_ = 0;

    SameMessage m;
    if (!parseSameHeader(raw, m))
    {
        return;
    }

    std::string key = sameCanonicalKey(m);
    if (!pendingKey_.empty() && key == pendingKey_ && samplePos_ - pendingLast_ < (int64_t)(2.5 * fs_))
    {
        ++pendingRepeats_;
        pendingLast_ = samplePos_;
    }
    else
    {
        emitPending();
        pendingKey_     = key;
        pendingMsg_     = m;
        pendingRepeats_ = 1;
        pendingFirst_   = samplePos_;
        pendingLast_    = samplePos_;
    }
}

void SameDecoder::emitPending()
{
    if (pendingKey_.empty())
    {
        return;
    }
    if (cb_)
    {
        pendingMsg_.repeats = pendingRepeats_;
        cb_(pendingMsg_);
    }
    pendingKey_.clear();
    pendingRepeats_ = 0;
}

void SameDecoder::flush()
{
    emitPending();
}
