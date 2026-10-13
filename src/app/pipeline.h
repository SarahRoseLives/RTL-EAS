// Channel pipeline: wideband IQ -> DDC -> FM demod -> SAME decoder, with
// optional demodulated-audio recording and playback.
#pragma once

#include "app/options.h"
#include "audio/playback.h"
#include "dsp/ddc.h"
#include "dsp/fm_demod.h"
#include "eas/same_decoder.h"
#include "util/wav_io.h"

#include <functional>
#include <memory>
#include <vector>

class Pipeline
{
public:
    Pipeline(const Options& opt, double inputRate, SameDecoder::MessageCb cb);
    ~Pipeline();

    void feed(const float* iq, int nComplex);
    void finish();

    // Demodulated mono audio (at channelRate()) is delivered here at the source
    // rate, on the SDR thread. Used by the GUI for playback, recording and a
    // level meter.
    void setAudioSink(std::function<void(const float*, int)> cb) { audioSink_ = std::move(cb); }

    double channelRate() const { return ddc_ ? ddc_->outputRate() : 0.0; }

private:
    void ensureDdc(const float* iq, int nComplex);
    void createDdc(double offset);
    void processIq(const float* iq, int nComplex);

    Options                     opt_;
    double                      inputRate_ = 0.0;
    SameDecoder::MessageCb      cb_;
    std::function<void(const float*, int)> audioSink_;
    std::unique_ptr<Ddc>        ddc_;
    std::unique_ptr<FmDemod>    fm_;
    std::unique_ptr<SameDecoder> dec_;
    WavWriter                   rec_;
    Playback                    play_;

    std::vector<double> ch_;
    std::vector<float>  audio_;
    std::vector<float>  probe_;
    bool                ready_ = false;
};
