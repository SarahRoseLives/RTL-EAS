#include "gui/gui_app.h"

#include "eas/noaa_channels.h"
#include "version.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace
{

std::string joinLocations(const std::vector<std::string>& locs)
{
    std::string s;
    for (size_t i = 0; i < locs.size(); ++i)
    {
        if (i)
        {
            s += ", ";
        }
        s += locs[i];
    }
    return s;
}

void statusLed(bool running)
{
    ImVec4 col = running ? ImVec4(0.20f, 0.90f, 0.30f, 1.0f) : ImVec4(0.85f, 0.25f, 0.25f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted("\xE2\x97\x8F"); // U+25CF black circle
    ImGui::PopStyleColor();
    ImGui::SameLine();
}

bool fileExists(const char* path)
{
    std::ifstream f(path);
    return f.good();
}

} // namespace

void drawControlPanel(GuiApp& app)
{
    ImGui::Begin("Control###Control");

    // ---- SDR device ----
    ImGui::SeparatorText("SDR device");
    const char* devLabel = app.devices.empty() ? "(no RTL-SDR found)" : app.devices[app.deviceIdx].name.c_str();
    ImGui::SetNextItemWidth(-90.0f);
    ImGui::BeginDisabled(app.running || app.devices.empty());
    if (ImGui::BeginCombo("##device", devLabel))
    {
        for (int i = 0; i < (int)app.devices.size(); ++i)
        {
            std::string lbl = app.devices[i].name + "  [SN:" + app.devices[i].serial + "]";
            bool        sel = (i == app.deviceIdx);
            if (ImGui::Selectable(lbl.c_str(), sel))
            {
                app.deviceIdx = i;
            }
            if (sel)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Refresh", ImVec2(-1, 0)))
    {
        app.stop();
        app.scanDevices();
        app.addLog(app.devices.empty() ? "No RTL-SDR devices found"
                                       : std::string("Found ") + std::to_string(app.devices.size()) + " device(s)");
    }

    // ---- channel ----
    ImGui::SeparatorText("Channel");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##channel", channelDisplay(app.channelIdx)))
    {
        for (int i = 0; i < kNoaaChannelCount; ++i)
        {
            std::string lbl = std::string(kNoaaChannels[i].label) + "   " + kNoaaChannels[i].name;
            bool        sel = (i == app.channelIdx);
            if (ImGui::Selectable(lbl.c_str(), sel))
            {
                app.channelIdx = i;
                app.onChannelChanged();
            }
            if (sel)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // ---- receiver ----
    ImGui::SeparatorText("Receiver");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##rate", kRtlRateLabels[app.rateIdx]))
    {
        for (int i = 0; i < kRtlRateCount; ++i)
        {
            bool sel = (i == app.rateIdx);
            std::string lbl = std::string(kRtlRateLabels[i]) + " MS/s";
            if (ImGui::Selectable(lbl.c_str(), sel) && i != app.rateIdx)
            {
                app.rateIdx = i;
                app.onRateChanged();
            }
            if (sel)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    bool changed = false;
    if (ImGui::Checkbox("Automatic gain", &app.autoGain))
    {
        changed = true;
    }
    ImGui::BeginDisabled(app.autoGain);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##gain", &app.gainDb, 0.0f, 49.6f, "Manual gain  %.1f dB"))
    {
        changed = true;
    }
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputFloat("##ppm", &app.ppm, 0.5f, 5.0f, "PPM correction  %.1f"))
    {
        changed = true;
    }

    if (ImGui::Checkbox("Bias tee (bias-T power)", &app.biasTee))
    {
        app.applyLiveTuner();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Feeds 4.5 V up the coax for an inline preamp/antenna.\n"
                          "Only RTL-SDR Blog V3-class dongles can supply it; harmless otherwise.");
    }
    if (changed)
    {
        app.applyLiveTuner();
    }

    // ---- audio ----
    ImGui::SeparatorText("Audio");
    if (ImGui::Checkbox("Play continuously", &app.play))
    {
        if (app.play)
        {
            app.ensurePlayback();
        }
    }
    ImGui::Checkbox("Auto-play alerts", &app.autoPlayAlerts);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Play the audio automatically whenever a subscribed alert arrives.");
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##vol", &app.volume, 0.0f, 1.0f, "Volume  %.2f");

    bool rec = app.record;
    if (ImGui::Checkbox("Record alerts to WAV", &rec))
    {
        app.setRecording(rec);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Saves each subscribed alert (with a few seconds of lead-in)\n"
                          "as alert_<event>_<timestamp>.wav in the folder below.");
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##recdir", "output folder", app.recordDir, sizeof(app.recordDir));

    float lvl = app.level.load();
    if (lvl > 1.0f)
    {
        lvl = 1.0f;
    }
    ImGui::ProgressBar(lvl, ImVec2(-1.0f, 10.0f), "");

    // ---- status / control ----
    ImGui::SeparatorText("Status");
    statusLed(app.running);
    ImGui::TextWrapped("%s", app.status.c_str());
    ImGui::Spacing();

    if (app.running)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.16f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
        if (ImGui::Button("Stop monitoring", ImVec2(-1, 34)))
        {
            app.stop();
        }
        ImGui::PopStyleColor(3);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.45f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.58f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.36f, 0.18f, 1.0f));
        if (ImGui::Button("Start monitoring", ImVec2(-1, 34)))
        {
            app.start();
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::End();
}

void drawAlertsPanel(GuiApp& app)
{
    ImGui::Begin("Alerts###Alerts", nullptr, ImGuiWindowFlags_NoScrollbar);

    std::lock_guard<std::mutex> lk(app.msgMtx);

    if (ImGui::Button("Clear"))
    {
        app.messages.clear();
        app.selectedMsg = -1;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Follow newest", &app.msgFollow);
    ImGui::SameLine();
    bool onlySub = !app.showAllAlerts;
    if (ImGui::Checkbox("Only subscribed", &onlySub))
    {
        app.showAllAlerts = !onlySub;
    }
    ImGui::SameLine();
    if (ImGui::Button("Export JSON") && !app.messages.empty())
    {
        std::ofstream f("same_alerts.jsonl", std::ios::app);
        if (f)
        {
            for (const Alert& a : app.messages)
            {
                f << sameJsonMessage(a.msg) << "\n";
            }
            app.addLog("Exported alerts to same_alerts.jsonl");
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%d alert%s", (int)app.messages.size(), app.messages.size() == 1 ? "" : "s");

    const ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                               ImGuiTableFlags_SizingStretchProp;

    // Size the table so it and the detail pane exactly fill the window, so the
    // Alerts tab itself never needs to scroll.
    const float detailH = 210.0f;
    ImVec2      avail   = ImGui::GetContentRegionAvail();
    float       tableH  = avail.y - detailH - ImGui::GetStyle().ItemSpacing.y;
    if (tableH < 80.0f)
    {
        tableH = 80.0f;
    }

    if (ImGui::BeginTable("alerts", 6, tf, ImVec2(0, tableH)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Event");
        ImGui::TableSetupColumn("Org");
        ImGui::TableSetupColumn("Areas", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableSetupColumn("Purge", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Copies", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)app.messages.size(); ++i)
        {
            const Alert& a   = app.messages[i];
            bool         sub = app.isSubscribed(a.msg.event);
            if (!sub && !app.showAllAlerts)
            {
                continue;
            }
            if (!sub)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable((a.stamp + "##" + std::to_string(i)).c_str(), app.selectedMsg == i,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
            {
                app.selectedMsg = i;
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", a.msg.event.c_str());
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", sameEventName(a.msg.event));
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", a.msg.org.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(joinLocations(a.msg.locations).c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", a.msg.purgeMinutes);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%d", a.msg.repeats);
            if (!sub)
            {
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::BeginChild("##alertdetail", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (app.selectedMsg >= 0 && app.selectedMsg < (int)app.messages.size())
    {
        const Alert& a = app.messages[app.selectedMsg];
        ImGui::TextColored(ImVec4(0.60f, 0.85f, 1.0f, 1.0f), "%s", a.msg.raw.c_str());
        ImGui::Spacing();
        ImGui::Text("Event      : %s - %s", a.msg.event.c_str(), sameEventName(a.msg.event));
        ImGui::Text("Originator : %s - %s", a.msg.org.c_str(), sameOrgName(a.msg.org));
        ImGui::Text("Areas      : %s", joinLocations(a.msg.locations).c_str());
        for (const std::string& loc : a.msg.locations)
        {
            ImGui::BulletText("%s", sameLocationName(loc).c_str());
        }
        ImGui::Text("Purge      : %d min", a.msg.purgeMinutes);
        ImGui::Text("Issued     : day %s %s:%s UTC", a.msg.julianTime.substr(0, 3).c_str(),
                    a.msg.julianTime.substr(3, 2).c_str(), a.msg.julianTime.substr(5, 2).c_str());
        ImGui::Text("Station    : %s", a.msg.callsign.c_str());
        ImGui::Text("Copies     : %d of 3", a.msg.repeats);
    }
    else
    {
        ImGui::TextDisabled("No alert selected. Decoded SAME headers appear here.");
    }
    ImGui::EndChild();

    ImGui::End();
}

void drawLogPanel(GuiApp& app)
{
    ImGui::Begin("Log###Log");
    std::lock_guard<std::mutex> lk(app.logMtx);
    ImGui::BeginChild("##log", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    for (const LogLine& l : app.log)
    {
        ImGui::TextDisabled("%s", l.stamp.c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", l.text.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

void drawSubscriptionsPanel(GuiApp& app)
{
    ImGui::Begin("Subscriptions###Subscriptions");

    ImGui::TextWrapped("Choose which alerts you want to see. Unchecked events are hidden from the Alerts panel.");
    ImGui::Spacing();
    if (ImGui::Button("All"))
    {
        app.subscribeAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("None"))
    {
        app.clearSubscribed();
    }
    ImGui::SameLine();
    if (ImGui::Button("Warnings"))
    {
        app.subscribeAll("Warning");
    }
    ImGui::SameLine();
    if (ImGui::Button("Watches"))
    {
        app.subscribeAll("Watch");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%d selected", app.subscribedCount());

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##subfilter", "filter events...", app.subFilter, sizeof(app.subFilter));

    std::string filter = app.subFilter;
    for (char& c : filter)
    {
        c = (char)std::tolower((unsigned char)c);
    }

    auto matches = [&](const SameEventInfo& e) {
        if (filter.empty())
        {
            return true;
        }
        std::string hay = std::string(e.code) + " " + e.name;
        for (char& c : hay)
        {
            c = (char)std::tolower((unsigned char)c);
        }
        return hay.find(filter) != std::string::npos;
    };

    const char* cats[] = {"Warning", "Watch", "Advisory", "Statement", "Test", "Other"};
    int         n           = 0;
    const SameEventInfo* ev = sameEventList(n);

    ImGui::Separator();
    ImGui::BeginChild("##subs", ImVec2(0, 0));
    for (const char* cat : cats)
    {
        bool any = false;
        for (int i = 0; i < n; ++i)
        {
            if (std::strcmp(ev[i].category, cat) == 0 && matches(ev[i]))
            {
                any = true;
                break;
            }
        }
        if (!any)
        {
            continue;
        }

        int onCount = 0;
        for (int i = 0; i < n; ++i)
        {
            if (std::strcmp(ev[i].category, cat) == 0 && app.isSubscribed(ev[i].code))
            {
                ++onCount;
            }
        }
        std::string hdr = std::string(cat) + "  (" + std::to_string(onCount) + ")";
        if (ImGui::CollapsingHeader(hdr.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(8.0f);
            if (ImGui::SmallButton((std::string("all##") + cat).c_str()))
            {
                app.subscribeAll(cat);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("none##") + cat).c_str()))
            {
                for (int i = 0; i < n; ++i)
                {
                    if (std::strcmp(ev[i].category, cat) == 0)
                    {
                        app.setSubscribed(ev[i].code, false);
                    }
                }
            }
            for (int i = 0; i < n; ++i)
            {
                if (std::strcmp(ev[i].category, cat) != 0 || !matches(ev[i]))
                {
                    continue;
                }
                bool        on    = app.isSubscribed(ev[i].code);
                std::string label = std::string(ev[i].code) + "  -  " + ev[i].name;
                if (ImGui::Checkbox(label.c_str(), &on))
                {
                    app.setSubscribed(ev[i].code, on);
                }
            }
            ImGui::Unindent(8.0f);
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void drawAboutPanel(GuiApp& app)
{
    if (!app.showAbout)
    {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("About###About", &app.showAbout))
    {
        ImGui::Text("rtl-eas  %s", RTLEAS_VERSION);
        ImGui::TextDisabled("NOAA SAME weather-radio alert decoder");
        ImGui::Separator();
        ImGui::TextWrapped(
            "Decodes the SAME digital alert headers that NOAA Weather Radio and the "
            "Emergency Alert System transmit ahead of a message. Tune any of the seven "
            "NWR channels; the header repeats three times and is combined for reliability.");
        ImGui::Spacing();
        ImGui::SeparatorText("NOAA Weather Radio channels");
        for (int i = 0; i < kNoaaChannelCount; ++i)
        {
            ImGui::BulletText("%s  %s", kNoaaChannels[i].label, kNoaaChannels[i].name);
        }
        ImGui::Spacing();
        ImGui::SeparatorText("Common event codes");
        const char* codes[] = {"TOR", "SVR", "FFW", "FLW", "SVA", "TOA", "RWT", "RMT", "DMO", "CAE"};
        for (const char* c : codes)
        {
            ImGui::BulletText("%s  %s", c, sameEventName(c));
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Bias tee supplies power for an inline preamp (RTL-SDR Blog V3-class dongles).");
    }
    ImGui::End();
}

void drawDockHost(GuiApp& app)
{
    static bool forceLayout = false;
    if (app.forceDefaultLayout)
    {
        app.forceDefaultLayout = false;
        if (fileExists("rtleas.default.ini"))
        {
            ImGui::LoadIniSettingsFromDisk("rtleas.default.ini");
            forceLayout = false;
        }
        else
        {
            forceLayout = true; // fall back to the built-in layout
        }
    }
    if (app.saveLayoutDefault)
    {
        app.saveLayoutDefault = false;
        ImGui::SaveIniSettingsToDisk("rtleas.default.ini");
        app.addLog("Saved current layout as default (rtleas.default.ini)");
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##RtlEasHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockId = ImGui::GetID("RtlEasDockSpace");
    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_None);

    if (forceLayout || ImGui::DockBuilderGetNode(dockId) == nullptr)
    {
        forceLayout = false;
        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);

        ImGuiID left, right, bot, sub;
        ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.32f, &left, &right);
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.26f, &bot, &right);
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.46f, &sub, &left);

        ImGui::DockBuilderDockWindow("Control###Control", left);
        ImGui::DockBuilderDockWindow("Subscriptions###Subscriptions", sub);
        ImGui::DockBuilderDockWindow("Alerts###Alerts", right);
        ImGui::DockBuilderDockWindow("Log###Log", bot);
        ImGui::DockBuilderFinish(dockId);
    }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Refresh devices"))
            {
                app.scanDevices();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                app.wantExit = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Save current layout as default"))
            {
                app.saveLayoutDefault = true;
            }
            if (ImGui::MenuItem("Reset layout to default"))
            {
                app.forceDefaultLayout = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About"))
            {
                app.showAbout = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::End();
}
