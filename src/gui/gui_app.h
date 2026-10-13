// GUI application state and lifecycle. Owns the RTL-SDR source and the decode
// pipeline, and marshals decoded alerts and demodulated audio from the SDR
// thread to the UI thread.
#pragma once

#include "app/pipeline.h"
#include "audio/playback.h"
#include "eas/noaa_channels.h"
#include "eas/same_message.h"
#include "sdr/rtl_sdr_source.h"
#include "util/wav_io.h"

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

struct Alert
{
    SameMessage msg;
    std::string stamp; // local wall-clock "HH:MM:SS"
};

struct LogLine
{
    std::string stamp;
    std::string text;
};

struct GuiApp
{
    // ---- device selection ----
    std::vector<SdrDeviceInfo> devices;
    int                        deviceIdx      = 0;
    bool                       devicesScanned = false;

    // ---- tuner / channel ----
    int   channelIdx = 6; // kNoaaChannels[6] == 162.550
    int   rateIdx    = 6; // kRtlRates[6] == 2.4e6
    bool  autoGain   = true;
    float gainDb     = 40.0f;
    float ppm        = 0.0f;
    bool  biasTee    = false;

    // ---- audio ----
    bool  play   = false;                 // continuous monitoring audio
    bool  autoPlayAlerts = true;          // play audio when an alert arrives
    float volume = 0.8f;
    bool  record = false;                 // record each decoded alert to WAV
    char  recordDir[256] = "recordings";  // output folder for alert WAVs

    // ---- runtime ----
    bool                          running = false;
    std::string                   status  = "Idle";
    std::unique_ptr<RtlSdrSource> src;
    std::unique_ptr<Pipeline>     pipeline;

    // Audio plumbing. Touched by both the UI thread (open/close) and the SDR
    // thread (write), guarded by audioMtx.
    std::mutex                 audioMtx;
    std::unique_ptr<Playback>  playback;
    std::unique_ptr<WavWriter> alertRec;   // active alert recording, if any
    std::string                alertRecName;
    double                     alertRecStart = 0.0;
    std::deque<float>          preroll;    // rolling pre-roll for alert clips
    std::size_t                prerollCap = 0;
    std::vector<float>         audioScratch;
    std::atomic<float>         level{0.0f};
    double                     channelFs = 24000.0;
    double                     audioSec  = 0.0;
    bool                       autoPlaying = false;
    double                     autoPlayUntil = 0.0;

    // ---- decoded alerts ----
    std::mutex               msgMtx;
    std::vector<Alert>       messages;
    int                      selectedMsg = -1;
    bool                     msgFollow   = true;
    bool                     showAllAlerts = false;

    // ---- alert subscriptions ----
    std::set<std::string> subscribed; // empty means "not yet configured"
    char                  subFilter[64] = "";
    mutable std::mutex    subMtx;

    // ---- log ----
    std::mutex          logMtx;
    std::deque<LogLine> log;

    // ---- ui ----
    bool showAbout          = false;
    bool forceDefaultLayout = false;
    bool saveLayoutDefault  = false;
    bool wantExit           = false;

    // Lifecycle.
    void scanDevices();
    void start();
    void stop();

    // Live tuner updates (safe while running).
    void onChannelChanged();
    void onRateChanged();
    void applyLiveTuner();
    void setRecording(bool on);
    void ensurePlayback();
    bool openPlayback();
    void beginAlertRecording(const SameMessage& m); // SDR thread

    bool isSubscribed(const std::string& code) const;
    void setSubscribed(const std::string& code, bool on);
    void clearSubscribed();
    int  subscribedCount() const;
    void loadConfig();
    void saveConfig();
    void subscribeAll(const char* category = nullptr); // nullptr => every event

    // SDR-thread callbacks.
    void pushMessage(const SameMessage& m);
    void onAudio(const float* a, int n);

    // UI helpers.
    void addLog(const std::string& text);
    bool isComplexSource() const { return true; }
};

// Formatting helpers shared by the panels.
std::string nowStamp();
const char* channelDisplay(int idx);
