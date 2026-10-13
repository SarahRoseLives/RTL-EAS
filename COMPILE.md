# Building rtl-eas

`rtl-eas` is a C++17 SDR-first NOAA SAME weather-radio decoder. It builds on
Windows with the **MSYS2 / MinGW-w64** toolchain (GCC), **CMake**, and **Ninja**.
The only external dependency is **librtlsdr** (which pulls in libusb).

> Important: GCC must be invoked from inside the **MINGW64** environment. If you
> run `g++.exe` directly from PowerShell/cmd without that environment set up,
> `cc1plus` can die silently with exit code 1 and no diagnostics. Always build
> from the **MSYS2 MINGW64** shell, or enter it from PowerShell as shown in
> section 4.

## 1. Install MSYS2

Download and install from <https://www.msys2.org>, then open the
**"MSYS2 MINGW64"** shell (not the plain "MSYS2 MSYS" shell) from the Start menu.

Update the package database once:

```bash
pacman -Syu
# close and reopen the MINGW64 shell if it asks you to, then:
pacman -Su
```

## 2. Install build tools and dependencies

```bash
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-pkgconf \
  mingw-w64-x86_64-rtl-sdr \
  mingw-w64-x86_64-libusb \
  mingw-w64-x86_64-glfw
```

This provides GCC/G++, CMake, Ninja, pkg-config, librtlsdr, libusb and GLFW.
Windows also needs `winmm` and OpenGL, which ship with the toolchain. Dear ImGui
(docking branch) is vendored in `third_party/imgui` — no extra packages needed.

## 3. Configure and build

From the **MINGW64** shell, in the project root:

```bash
cmake -S . -B build -G Ninja
ninja -C build
```

The executables and the runtime DLLs they need are placed in `build/`:

```
build/rtl-eas.exe        (command-line decoder)
build/rtl-eas-gui.exe    (docked GUI)
build/librtlsdr.dll, libusb-1.0.dll, glfw3.dll,
      libgcc_s_seh-1.dll, libwinpthread-1.dll, libstdc++-6.dll
```

The DLLs are copied next to each `.exe` automatically (a POST_BUILD step), so
they run standalone from a double-click or from an IDE without MSYS2 on `PATH`.

Run them:

```bash
./build/rtl-eas.exe --list
./build/rtl-eas-gui.exe
```

## 4. Building from PowerShell (optional)

If you prefer to drive the build from PowerShell, enter the MINGW64 environment
first:

```powershell
$env:MSYSTEM = "MINGW64"
$env:CHERE_INVOKING = "1"
& C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/Rose/Projects/RTL-EAS && ninja -C build"
```

The `-l` (login) shell with `MSYSTEM=MINGW64` sources `/etc/profile`, which puts
`/mingw64/bin` on `PATH` so `cc1plus` can find its runtime. `CHERE_INVOKING=1`
keeps the current directory.

## Notes / troubleshooting

- **`cc1plus` exits 1 with no output.** You are not in the MINGW64 environment.
  Build from the MSYS2 MINGW64 shell (see the warning above).
- **Link fails with "Access is denied" on `rtl-eas.exe`.** A previous instance is
  still running and is locking the file. Close it, then rebuild.
- **`usb_open error -3` when opening the RTL-SDR.** Another program (or a
  previous `rtl-eas`/`rtl_test` that was force-killed) still holds the device.
  Close it and retry.
- **Incremental builds.** Re-run `ninja -C build`. Only changed files recompile.
  After editing `CMakeLists.txt`, re-run `cmake -S . -B build` first.
