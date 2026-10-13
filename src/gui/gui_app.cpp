#include "gui/gui_app.h"

#include "eas/noaa_channels.h"
#include "util/log.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>

std::string nowStamp()
{
    std::time_t t = std::time(nullptr);
    std::tm     tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

const char* channelDisplay(int idx)
{
    if (idx < 0 || idx >= kNoaaChannelCount)
    {
        idx = 0;
    }
    return kNoaaChannels[idx].label;
}

namespace
{
std::string nowFileStamp()
{
    std::time_t t = std::time(nullptr);
    std::tm     tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}
} // namespace

void GuiApp::scanDevices()
{
    RtlSdrSource probe;
    devices        = probe.listDevices();
    devicesScanned = true;
    if (deviceIdx >= (int)devices.size())
    {
        deviceIdx = 0;
    }
}

void GuiApp::addLog(const std::string& text)
{
    std::lock_guard<std::mutex> lk(logMtx);
    log.push_back({nowStamp(), text});
    while (log.size() > 500)
    {
        log.pop_front();
    }
}

void GuiApp::pushMessage(const SameMessage& m)
{
    bool sub = isSubscribed(m.event);
    {
        std::lock_guard<std::mutex> lk(msgMtx);
        messages.push_back({m, nowStamp()});
        while (messages.size() > 1000)
        {
            messages.erase(messages.begin());
            if (selectedMsg > 0)
            {
                --selectedMsg;
            }
        }
        if (msgFollow && sub)
        {
            selectedMsg = (int)messages.size() - 1;
        }
    }

    if (record && running && sub)
    {
        beginAlertRecording(m);
    }

    if (autoPlayAlerts && running && sub)
    {
        if (openPlayback())
        {
            autoPlaying    = true;
            autoPlayUntil  = audioSec + 180.0;
            addLog(std::string("Auto-playing alert audio - ") + sameEventName(m.event));
        }
    }
}

void GuiApp::beginAlertRecording(const SameMessage& m)
{
    std::error_code ec;
    if (recordDir[0] != 0)
    {
        std::filesystem::create_directories(recordDir, ec);
    }

    std::string dir = recordDir[0] ? recordDir : ".";
    std::string file = dir + "/alert_" + (m.event.empty() ? std::string("UNK") : m.event) + "_" + nowFileStamp() + ".wav";

    std::lock_guard<std::mutex> lk(audioMtx);
    if (alertRec)
    {
        alertRec->close();
        addLog("Alert clip saved: " + alertRecName);
        alertRec.reset();
    }

    alertRec = std::make_unique<WavWriter>();
    std::string err;
    if (!alertRec->open(file, (int)channelFs, 1, err))
    {
        addLog("Alert record failed: " + err);
        alertRec.reset();
        return;
    }
    alertRecName  = file;
    alertRecStart = audioSec;

    if (!preroll.empty())
    {
        std::vector<float> pr(preroll.begin(), preroll.end());
        alertRec->write(pr.data(), (int)pr.size());
    }
    addLog("Recording alert (" + m.event + ") to " + file);
}

void GuiApp::onAudio(const float* a, int n)
{
    if (n <= 0)
    {
        return;
    }
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float v = std::fabs(a[i]);
        if (v > peak)
        {
            peak = v;
        }
    }
    float cur = level.load();
    level.store(cur * 0.85f + peak * 0.15f);

    std::lock_guard<std::mutex> lk(audioMtx);
    audioSec += (double)n / channelFs;

    if (prerollCap > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            preroll.push_back(a[i]);
        }
        while (preroll.size() > prerollCap)
        {
            preroll.pop_front();
        }
    }

    if (alertRec)
    {
        alertRec->write(a, n);
        if (audioSec - alertRecStart > 300.0)
        {
            alertRec->close();
            addLog("Alert clip saved: " + alertRecName);
            alertRec.reset();
        }
    }

    if (autoPlaying && audioSec > autoPlayUntil)
    {
        autoPlaying = false;
        addLog("Alert audio finished");
    }

    if ((play || autoPlaying) && playback && playback->isOpen())
    {
        audioScratch.resize((size_t)n);
        for (int i = 0; i < n; ++i)
        {
            audioScratch[(size_t)i] = a[i] * volume;
        }
        playback->write(audioScratch.data(), n);
    }
}

void GuiApp::start()
{
    if (running)
    {
        return;
    }
    if (!devicesScanned)
    {
        scanDevices();
    }
    if (devices.empty())
    {
        status = "No RTL-SDR device found";
        addLog(status);
        return;
    }

    Options opt;
    opt.device      = deviceIdx;
    opt.freqMHz     = kNoaaChannels[channelIdx].freqMHz;
    opt.sampleRate  = kRtlRates[rateIdx];
    opt.gainDb      = autoGain ? -1.0 : (double)gainDb;
    opt.ppm         = ppm;
    opt.channelRate = 24000.0;
    opt.channelBw   = 12000.0;

    src = std::make_unique<RtlSdrSource>();
    src->setCenterFreq(opt.freqMHz * 1e6);
    src->setSampleRate(opt.sampleRate);
    src->setGain(opt.gainDb);
    src->setPpm(opt.ppm);
    src->setBiasTee(biasTee);

    pipeline = std::make_unique<Pipeline>(opt, opt.sampleRate,
                                          [this](const SameMessage& m) { pushMessage(m); });
    pipeline->setAudioSink([this](const float* a, int n) { onAudio(a, n); });

    double fs = pipeline->channelRate();
    if (fs <= 0.0)
    {
        fs = 24000.0;
    }
    channelFs  = fs;
    prerollCap = (std::size_t)(fs * 5.0); // 5 s of pre-roll for alert clips
    audioSec   = 0.0;

    std::lock_guard<std::mutex> lk(audioMtx);
    preroll.clear();
    if (play)
    {
        playback = std::make_unique<Playback>();
        if (!playback->open((int)fs, 1))
        {
            playback.reset();
            play = false;
        }
    }

    std::string err;
    bool        ok = src->start(deviceIdx, [this](const float* iq, int n) { pipeline->feed(iq, n); }, err);
    if (!ok)
    {
        status = "Error: " + err;
        addLog(status);
        if (alertRec)
        {
            alertRec->close();
            alertRec.reset();
        }
        if (playback)
        {
            playback->close();
            playback.reset();
        }
        pipeline.reset();
        src.reset();
        return;
    }

    running = true;
    status  = std::string("Monitoring ") + kNoaaChannels[channelIdx].label;
    addLog("Started - " + status + (biasTee ? " (bias tee on)" : ""));
    saveConfig();
}

void GuiApp::stop()
{
    if (!running && !src)
    {
        return;
    }
    if (src)
    {
        src->stop();
    }
    {
        std::lock_guard<std::mutex> lk(audioMtx);
        if (alertRec)
        {
            alertRec->close();
            addLog("Alert clip saved: " + alertRecName);
            alertRec.reset();
        }
        if (playback)
        {
            playback->close();
            playback.reset();
        }
        preroll.clear();
    }
    pipeline.reset();
    src.reset();
    running = false;
    autoPlaying = false;
    status  = "Stopped";
    addLog("Stopped");
    saveConfig();
}

void GuiApp::onChannelChanged()
{
    if (running && src)
    {
        double hz = kNoaaChannels[channelIdx].freqMHz * 1e6;
        src->setCenterFreq(hz);
        status = std::string("Monitoring ") + kNoaaChannels[channelIdx].label;
        addLog(std::string("Tuned to ") + kNoaaChannels[channelIdx].label);
    }
}

void GuiApp::onRateChanged()
{
    if (running)
    {
        stop();
        start();
    }
}

void GuiApp::applyLiveTuner()
{
    if (running && src)
    {
        src->setGain(autoGain ? -1.0 : (double)gainDb);
        src->setPpm(ppm);
        src->setBiasTee(biasTee);
    }
}

void GuiApp::setRecording(bool on)
{
    record = on;
    if (!running)
    {
        return;
    }
    if (on)
    {
        addLog("Alert recording armed - the next subscribed alert will be saved to WAV");
    }
    else if (running)
    {
        std::lock_guard<std::mutex> lk(audioMtx);
        if (alertRec)
        {
            alertRec->close();
            addLog("Alert clip saved: " + alertRecName);
            alertRec.reset();
        }
    }
    saveConfig();
}

bool GuiApp::openPlayback()
{
    if (!running || !pipeline)
    {
        return false;
    }
    std::lock_guard<std::mutex> lk(audioMtx);
    if (playback)
    {
        return true;
    }
    double fs = pipeline->channelRate();
    if (fs <= 0.0)
    {
        fs = 24000.0;
    }
    playback = std::make_unique<Playback>();
    if (!playback->open((int)fs, 1))
    {
        playback.reset();
        return false;
    }
    return true;
}

void GuiApp::ensurePlayback()
{
    if (openPlayback())
    {
        return;
    }
    play = false;
    addLog("playback device unavailable");
}

void GuiApp::subscribeAll(const char* category)
{
    {
        std::lock_guard<std::mutex> lk(subMtx);
        int                 n  = 0;
        const SameEventInfo* ev = sameEventList(n);
        if (!category)
        {
            subscribed.clear();
        }
        for (int i = 0; i < n; ++i)
        {
            if (!category || std::strcmp(ev[i].category, category) == 0)
            {
                subscribed.insert(ev[i].code);
            }
        }
    }
    saveConfig();
}

bool GuiApp::isSubscribed(const std::string& code) const
{
    std::lock_guard<std::mutex> lk(subMtx);
    return subscribed.count(code) > 0;
}

void GuiApp::setSubscribed(const std::string& code, bool on)
{
    {
        std::lock_guard<std::mutex> lk(subMtx);
        if (on)
        {
            subscribed.insert(code);
        }
        else
        {
            subscribed.erase(code);
        }
    }
    saveConfig();
}

void GuiApp::clearSubscribed()
{
    {
        std::lock_guard<std::mutex> lk(subMtx);
        subscribed.clear();
    }
    saveConfig();
}

int GuiApp::subscribedCount() const
{
    std::lock_guard<std::mutex> lk(subMtx);
    return (int)subscribed.size();
}

void GuiApp::loadConfig()
{
    std::ifstream f("rtleas.cfg");
    if (!f)
    {
        subscribeAll();
        return;
    }

    std::string            line;
    int                    subsCount = -1;
    std::set<std::string>  subs;
    while (std::getline(f, line))
    {
        size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        if (k == "channel")
        {
            channelIdx = std::atoi(v.c_str());
        }
        else if (k == "rate")
        {
            rateIdx = std::atoi(v.c_str());
        }
        else if (k == "autogain")
        {
            autoGain = std::atoi(v.c_str()) != 0;
        }
        else if (k == "gain")
        {
            gainDb = (float)std::atof(v.c_str());
        }
        else if (k == "ppm")
        {
            ppm = (float)std::atof(v.c_str());
        }
        else if (k == "bias")
        {
            biasTee = std::atoi(v.c_str()) != 0;
        }
        else if (k == "volume")
        {
            volume = (float)std::atof(v.c_str());
        }
        else if (k == "play")
        {
            play = std::atoi(v.c_str()) != 0;
        }
        else if (k == "autoplay")
        {
            autoPlayAlerts = std::atoi(v.c_str()) != 0;
        }
        else if (k == "record")
        {
            record = std::atoi(v.c_str()) != 0;
        }
        else if (k == "recorddir" || k == "recordpath")
        {
            std::snprintf(recordDir, sizeof(recordDir), "%s", v.c_str());
        }
        else if (k == "showall")
        {
            showAllAlerts = std::atoi(v.c_str()) != 0;
        }
        else if (k == "subs")
        {
            subsCount = std::atoi(v.c_str());
        }
        else if (k == "sub")
        {
            subs.insert(v);
        }
    }

    if (subsCount >= 0)
    {
        subscribed = subs;
    }
    else
    {
        subscribeAll();
    }

    if (channelIdx < 0 || channelIdx >= kNoaaChannelCount)
    {
        channelIdx = 6;
    }
    if (rateIdx < 0 || rateIdx >= kRtlRateCount)
    {
        rateIdx = 6;
    }
}

void GuiApp::saveConfig()
{
    std::ofstream f("rtleas.cfg", std::ios::trunc);
    if (!f)
    {
        return;
    }
    f << "channel=" << channelIdx << "\n";
    f << "rate=" << rateIdx << "\n";
    f << "autogain=" << (autoGain ? 1 : 0) << "\n";
    f << "gain=" << gainDb << "\n";
    f << "ppm=" << ppm << "\n";
    f << "bias=" << (biasTee ? 1 : 0) << "\n";
    f << "volume=" << volume << "\n";
    f << "play=" << (play ? 1 : 0) << "\n";
    f << "autoplay=" << (autoPlayAlerts ? 1 : 0) << "\n";
    f << "record=" << (record ? 1 : 0) << "\n";
    f << "recorddir=" << recordDir << "\n";
    f << "showall=" << (showAllAlerts ? 1 : 0) << "\n";
    {
        std::lock_guard<std::mutex> lk(subMtx);
        f << "subs=" << subscribed.size() << "\n";
        for (const std::string& s : subscribed)
        {
            f << "sub=" << s << "\n";
        }
    }
}
