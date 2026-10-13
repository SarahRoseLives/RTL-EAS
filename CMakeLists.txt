cmake_minimum_required(VERSION 3.16)
project(RtlEas CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(RTLSDR QUIET librtlsdr)
endif()
if(NOT RTLSDR_FOUND)
    find_path(RTLSDR_INCLUDE_DIRS NAMES rtl-sdr.h REQUIRED)
    find_library(RTLSDR_LIBRARIES NAMES rtlsdr librtlsdr REQUIRED)
endif()

find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG QUIET)
if(NOT glfw3_FOUND)
    if(NOT PkgConfig_FOUND)
        find_package(PkgConfig REQUIRED)
    endif()
    pkg_check_modules(GLFW REQUIRED glfw3)
endif()

# ---------------------------------------------------------------------------
# rtleascore: shared decoder core (SDR sources, DSP, SAME, audio, pipeline).
# ---------------------------------------------------------------------------
add_library(rtleascore STATIC
    src/util/log.cpp
    src/util/wav_io.cpp
    src/sdr/rtl_sdr_source.cpp
    src/sdr/wav_file_source.cpp
    src/dsp/fm_demod.cpp
    src/eas/same_decoder.cpp
    src/eas/same_message.cpp
    src/app/pipeline.cpp
    src/audio/playback.cpp
)
target_include_directories(rtleascore PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${RTLSDR_INCLUDE_DIRS}
)
target_compile_definitions(rtleascore PUBLIC _USE_MATH_DEFINES)
target_link_libraries(rtleascore PUBLIC ${RTLSDR_LIBRARIES} Threads::Threads)
target_link_directories(rtleascore PUBLIC ${RTLSDR_LIBRARY_DIRS})
if(WIN32)
    target_link_libraries(rtleascore PUBLIC winmm)
endif()

# ---------------------------------------------------------------------------
# rtl-eas: command-line decoder.
# ---------------------------------------------------------------------------
add_executable(rtl-eas
    src/main.cpp
    src/app/options.cpp
)
target_link_libraries(rtl-eas PRIVATE rtleascore)

# ---------------------------------------------------------------------------
# Dear ImGui (docking branch, vendored) as a static library for the GUI.
# ---------------------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC OpenGL::GL)
if(glfw3_FOUND)
    target_link_libraries(imgui PUBLIC glfw)
else()
    target_include_directories(imgui PUBLIC ${GLFW_INCLUDE_DIRS})
    target_link_libraries(imgui PUBLIC ${GLFW_LIBRARIES})
    target_link_directories(imgui PUBLIC ${GLFW_LIBRARY_DIRS})
endif()

# ---------------------------------------------------------------------------
# rtl-eas-gui: focused desktop app (device/channel picker + alert console).
# ---------------------------------------------------------------------------
add_executable(rtl-eas-gui
    src/gui_main.cpp
    src/gui/gui_app.cpp
    src/gui/panels.cpp
)
target_include_directories(rtl-eas-gui PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rtl-eas-gui PRIVATE rtleascore imgui OpenGL::GL)
if(glfw3_FOUND)
    target_link_libraries(rtl-eas-gui PRIVATE glfw)
else()
    target_include_directories(rtl-eas-gui PRIVATE ${GLFW_INCLUDE_DIRS})
    target_link_libraries(rtl-eas-gui PRIVATE ${GLFW_LIBRARIES})
    target_link_directories(rtl-eas-gui PRIVATE ${GLFW_LIBRARY_DIRS})
endif()

# ---------------------------------------------------------------------------
# Windows: copy runtime DLLs next to each executable so they run standalone.
# ---------------------------------------------------------------------------
if(WIN32)
    get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(RTLEAS_RUNTIME_DLLS
        libgcc_s_seh-1.dll
        libwinpthread-1.dll
        libstdc++-6.dll
        librtlsdr.dll
        libusb-1.0.dll
    )
    set(RTLEAS_GUI_DLLS ${RTLEAS_RUNTIME_DLLS} glfw3.dll)
    foreach(target rtl-eas rtl-eas-gui)
        if(target STREQUAL "rtl-eas-gui")
            set(dlls ${RTLEAS_GUI_DLLS})
        else()
            set(dlls ${RTLEAS_RUNTIME_DLLS})
        endif()
        foreach(dll ${dlls})
            if(EXISTS "${MINGW_BIN}/${dll}")
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${MINGW_BIN}/${dll}" "$<TARGET_FILE_DIR:${target}>"
                    COMMENT "Copying ${dll}")
            else()
                message(WARNING "Runtime DLL not found: ${MINGW_BIN}/${dll}")
            endif()
        endforeach()
    endforeach()

    # The GUI is a windowed app: no console window alongside it.
    set_target_properties(rtl-eas-gui PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
