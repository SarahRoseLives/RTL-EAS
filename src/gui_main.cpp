// rtl-eas GUI entry point: GLFW + OpenGL3 + Dear ImGui (docking) host.
#include "gui/gui_app.h"
#include "version.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

void drawDockHost(GuiApp& app);
void drawControlPanel(GuiApp& app);
void drawSubscriptionsPanel(GuiApp& app);
void drawAlertsPanel(GuiApp& app);
void drawLogPanel(GuiApp& app);
void drawAboutPanel(GuiApp& app);

namespace
{

void glfwError(int error, const char* desc)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", error, desc);
}

void applyTheme()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding    = 4.0f;
    st.FrameRounding     = 3.0f;
    st.GrabRounding      = 3.0f;
    st.TabRounding       = 3.0f;
    st.ScrollbarRounding = 3.0f;
    st.WindowBorderSize  = 1.0f;
    st.FrameBorderSize   = 0.0f;
    st.ItemSpacing       = ImVec2(8.0f, 6.0f);
    st.FramePadding      = ImVec2(8.0f, 5.0f);

    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg]       = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_ChildBg]        = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_PopupBg]        = ImVec4(0.12f, 0.13f, 0.15f, 0.98f);
    c[ImGuiCol_Border]         = ImVec4(0.24f, 0.26f, 0.30f, 0.70f);
    c[ImGuiCol_FrameBg]        = ImVec4(0.17f, 0.18f, 0.21f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(0.26f, 0.29f, 0.34f, 1.00f);
    c[ImGuiCol_TitleBg]        = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
    c[ImGuiCol_TitleBgActive]  = ImVec4(0.16f, 0.20f, 0.26f, 1.00f);
    c[ImGuiCol_MenuBarBg]      = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
    c[ImGuiCol_Header]         = ImVec4(0.20f, 0.28f, 0.38f, 1.00f);
    c[ImGuiCol_HeaderHovered]  = ImVec4(0.25f, 0.35f, 0.47f, 1.00f);
    c[ImGuiCol_HeaderActive]   = ImVec4(0.29f, 0.41f, 0.55f, 1.00f);
    c[ImGuiCol_Button]         = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
    c[ImGuiCol_ButtonHovered]  = ImVec4(0.26f, 0.32f, 0.40f, 1.00f);
    c[ImGuiCol_ButtonActive]   = ImVec4(0.30f, 0.38f, 0.48f, 1.00f);
    c[ImGuiCol_Tab]            = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
    c[ImGuiCol_TabHovered]     = ImVec4(0.26f, 0.34f, 0.44f, 1.00f);
    c[ImGuiCol_TabSelected]    = ImVec4(0.20f, 0.28f, 0.38f, 1.00f);
    c[ImGuiCol_TableHeaderBg]  = ImVec4(0.15f, 0.17f, 0.20f, 1.00f);
    c[ImGuiCol_TableRowBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]  = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    c[ImGuiCol_Separator]      = ImVec4(0.24f, 0.26f, 0.30f, 0.70f);
}

void loadFonts(ImGuiIO& io)
{
    const char* paths[] = {
        "third_party/imgui/misc/fonts/Roboto-Medium.ttf",
        "../third_party/imgui/misc/fonts/Roboto-Medium.ttf",
        "../../third_party/imgui/misc/fonts/Roboto-Medium.ttf",
    };
    for (const char* p : paths)
    {
        if (io.Fonts->AddFontFromFileTTF(p, 16.0f))
        {
            return;
        }
    }
    io.Fonts->AddFontDefault();
}

} // namespace

int main(int argc, char** argv)
{
    glfwSetErrorCallback(glfwError);
    if (!glfwInit())
    {
        return 1;
    }

    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1040, 680, "rtl-eas - SAME Weather Alert Decoder", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "rtleas.ini";
    {
        std::ifstream working("rtleas.ini");
        std::ifstream deflt("rtleas.default.ini");
        if (working.good())
        {
            ImGui::LoadIniSettingsFromDisk("rtleas.ini");
        }
        else if (deflt.good())
        {
            ImGui::LoadIniSettingsFromDisk("rtleas.default.ini");
        }
    }

    applyTheme();
    loadFonts(io);
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::GetStyle().WindowRounding = 0.0f;
        ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    GuiApp app;
    bool   autoStart  = false;
    int    argChannel = -1;
    int    argBias    = -1;
    int    argPlay    = -1;
    int    argRecord  = -1;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--start")
        {
            autoStart = true;
        }
        else if (a == "--channel" && i + 1 < argc)
        {
            argChannel = std::atoi(argv[++i]);
        }
        else if (a == "--bias")
        {
            argBias = 1;
        }
        else if (a == "--play")
        {
            argPlay = 1;
        }
        else if (a == "--record")
        {
            argRecord = 1;
        }
    }

    app.scanDevices();
    app.loadConfig();

    if (argChannel >= 0 && argChannel < kNoaaChannelCount)
    {
        app.channelIdx = argChannel;
    }
    if (argBias == 1)
    {
        app.biasTee = true;
    }
    if (argPlay == 1)
    {
        app.play = true;
    }
    if (argRecord == 1)
    {
        app.record = true;
    }

    app.addLog("rtl-eas " + std::string(RTLEAS_VERSION) + " ready");
    app.addLog(app.devices.empty() ? "No RTL-SDR devices found - connect one and Refresh"
                                   : std::string("Found ") + std::to_string(app.devices.size()) + " RTL-SDR device(s)");
    app.addLog(std::string("Subscribed to ") + std::to_string(app.subscribed.size()) + " alert type(s)");

    if (autoStart)
    {
        app.start();
    }

    const ImVec4 clearColor = ImVec4(0.06f, 0.07f, 0.09f, 1.0f);

    while (!glfwWindowShouldClose(window) && !app.wantExit)
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawDockHost(app);
        drawControlPanel(app);
        drawSubscriptionsPanel(app);
        drawAlertsPanel(app);
        drawLogPanel(app);
        drawAboutPanel(app);

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup);
        }

        glfwSwapBuffers(window);
    }

    ImGui::SaveIniSettingsToDisk(io.IniFilename);

    app.saveConfig();
    app.stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
