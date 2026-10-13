// SAME (NOAA weather radio) demodulator + header decoder.
//
// Input is the FM-discriminated audio (instantaneous frequency) of one narrow
// channel. SAME carries a 520.83 baud AFSK signal, mark = 2083.3 Hz,
// space = 1562.5 Hz. Each of the three identical header repetitions is
// preceded by a 16-byte 0xAB preamble.
//
// The decoder band-limits the audio, runs mark/space tone correlators, recovers
// the bit clock with a first-order phase loop, matches the 0xAB preamble and
// then the "ZCZC" sync word, assembles header bytes LSB-first, and reports
// complete (de-duplicated) messages through the callback.
#pragma once

#include "eas/same_message.h"

#include <complex>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class SameDecoder
{
public:
    using MessageCb = std::function<void(const SameMessage&)>;

    SameDecoder(double sampleRate, MessageCb cb);

    void push(const float* audio, int n);

    // Emits any pending (not-yet-reported) message. Call when the stream ends.
    void flush();

private:
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        double process(double x)
        {
            double y = b0 * x + z1;
            z1       = b1 * x - a1 * y + z2;
            z2       = b2 * x - a2 * y;
            return y;
        }

        static Biquad lowpass(double fs, double f, double q);
        static Biquad highpass(double fs, double f, double q);
    };

    void handleBit(int bit);
    void finishHeader();
    void emitPending();

    double    fs_  = 25000.0;
    double    spb_ = 0.0;
    double    inc_ = 0.0;
    MessageCb cb_;

    Biquad hp_, lp_;

    int                               L_ = 1;
    std::vector<std::complex<double>> mring_, sring_;
    std::complex<double>              markMix_{1.0, 0.0}, spaceMix_{1.0, 0.0};
    std::complex<double>              markRot_{1.0, 0.0}, spaceRot_{1.0, 0.0};
    std::complex<double>              msum_{0.0, 0.0}, ssum_{0.0, 0.0};
    int                               rp_ = 0, rc_ = 0;

    double acc_     = 0.0;
    int    lastSgn_ = 0;

    uint64_t prePat_ = 0;
    uint64_t zcPat_  = 0;
    uint64_t w64_    = 0;
    uint32_t reg_    = 0;
    int      preambleAge_ = 1000000000;

    bool        collecting_ = false;
    int         byteVal_    = 0;
    int         bitInByte_  = 0;
    std::string header_;

    std::string pendingKey_;
    SameMessage pendingMsg_;
    int         pendingRepeats_ = 0;
    int64_t     pendingFirst_   = 0;
    int64_t     pendingLast_    = 0;
    int64_t     samplePos_      = 0;
};
