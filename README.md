# rtl-eas

A **SDR-first NOAA SAME (Specific Area Message Encoding) weather-radio
decoder** written in C++17.

`rtl-eas` tunes an RTL-SDR (or replays an IQ/audio capture), demodulates the
narrowband FM weather-radio channel, and decodes the 520.83 baud SAME AFSK
header that precedes Emergency Alert System / NOAA Weather Radio messages. It
prints the parsed alert (event, area, purge time, issue time, station) and can
record or play the demodulated audio.

## Features

- **SDR-first**: drives an RTL-SDR directly via `librtlsdr`; no external
  `rtl_fm`/piping required.
- **Offline replay**: decode IQ WAV captures (`--wav`) or already-demodulated
  audio recordings (`--audio`).
- **Full DSP chain**: digital downconversion, FM quadrature discriminator,
  mark/space tone correlators, bit-clock recovery (phase loop), preamble +
  `ZCZC` sync, LSB-first byte framing.
- **SAME header parsing** with event-code, originator and FIPS-state lookup
  tables, plus 3x-repeat combining.
- **Auto channel offset** (`--auto-offset`) for off-tuned captures.
- **Audio output**: record the demodulated channel to WAV and/or play it live.
- Machine-readable `--json` output for scripting/forwarding.

## Build

See [COMPILE.md](COMPILE.md). Short version (MSYS2 MINGW64 shell):

```bash
cmake -S . -B build -G Ninja
ninja -C build
```

## Desktop app (GUI)

`rtl-eas-gui` is a focused desktop app for people who want weather alerts, not a
radio console. It has no spectrum or waterfall — just the controls you need and
the alerts you care about, in a resizable, dockable Dear ImGui layout (saved to
`rtleas.ini`).

- **Control panel** — pick your RTL-SDR (listed by name/serial), choose one of
  the seven NOAA Weather Radio channels, set sample rate, gain (auto/manual),
  PPM correction, and **bias tee** for an inline preamp. Start/Stop monitoring,
  **auto-play alerts** (hears the alert as soon as it arrives) plus continuous
  monitoring, a level meter, and **Record alerts to WAV** (saves each subscribed
  alert, with a few seconds of lead-in, as `alert_<event>_<timestamp>.wav`).
  Demodulated audio runs through a boost-only AGC, so quiet stations stay
  audible and loud ones do not clip.
- **Subscriptions panel** — tick the alert types you actually care about
  (grouped into Warning / Watch / Advisory / Statement / Test / Other, with
  All / None / Warnings / Watches shortcuts and a filter box). Only subscribed
  types are shown in the Alerts panel; settings persist in `rtleas.cfg`.
- **Alerts panel** — a table of decoded SAME headers (time, event, originator,
  areas, purge time, copies) with a detail view of the selected alert
  (full raw header, event/area names, issue time, station). Export to JSON.
- **Log panel** — device/status messages and a running decode log.

The **View** menu has *Save current layout as default* (stores your docked
arrangement to `rtleas.default.ini`) and *Reset layout to default*, so the
layout you build becomes the one the app opens with.

```bash
./build/rtl-eas-gui.exe
# power-user shortcuts: start immediately on a given channel
./build/rtl-eas-gui.exe --start --channel 6      # 162.550 MHz (WX-1)
./build/rtl-eas-gui.exe --start --channel 0 --bias
```

There is deliberately **no automatic channel selection** — you pick the channel
your local transmitter uses. Turn on **bias tee** only if your dongle is an
RTL-SDR Blog V3-class device and you have an inline preamp/antenna that needs
power; it is harmless on other dongles.

## Usage (command line)

```
rtl-eas [options]

Source:
  --device N        RTL-SDR device index (default 0)
  --freq MHZ        center frequency (default 162.550)
  --rate HZ         sample rate (default 2400000)
  --gain DB         tuner gain in dB, or 'auto' (default auto)
  --ppm N           frequency correction in ppm
  --offset HZ       channel offset from center (default 0)
  --auto-offset     auto-detect the strongest channel offset
  --wav FILE        decode an IQ WAV capture instead of the RTL-SDR
  --loop            loop the WAV capture
  --audio FILE      decode an already-demodulated audio WAV file

Output:
  --record FILE     write the demodulated audio to a WAV file
  --play            play the demodulated audio (Windows)
  --json            emit each message as a single JSON line
  --verbose         verbose logging
  --duration N      stop after N seconds (0 = run until Ctrl-C)
  --list            list RTL-SDR devices and exit
  -h, --help        show this help
```

NOAA weather radio channels (MHz): `162.400 162.425 162.450 162.475 162.500
162.525 162.550`.

### Live decoding

Tune the RTL-SDR to a local NOAA weather radio channel and let it run:

```bash
./build/rtl-eas.exe --freq 162.550 --gain 40
```

### Decoding a capture

The bundled baseband capture was recorded with its center between two NOAA
channels; the wanted signal sits at `+71777 Hz`:

```bash
./build/rtl-eas.exe --wav "Samples/baseband_162478469Hz_22-21-57_12-10-2026.wav" --offset 71777
```

```
ZCZC-WXR-RWT-390007+0030-0010000-SAMETX--
  Originator : WXR - Weather Radio / NOAA
  Event      : RWT - Required Weekly Test
  Locations  : 390007 (Ohio)
  Purge      : 30 min
  Issued     : day 001 00:00 UTC
  Callsign   : SAMETX
  Copies     : 3
```

An already-demodulated audio recording decodes the same way, at its own sample
rate:

```bash
./build/rtl-eas.exe --audio "Samples/audio.wav"
```

```
ZCZC-EAS-RWT-012057-012081-012101-012103-012115+0030-2780415-WTSP
  Originator : EAS - Emergency Alert System
  Event      : RWT - Required Weekly Test
  Locations  : 012057 (Florida), 012081 (Florida), 012101 (Florida), 012103 (Florida), 012115 (Florida)
  Purge      : 30 min
  Issued     : day 278 04:15 UTC
  Callsign   : WTSP
  Copies     : 3
```

## How it works

```
RTL-SDR / WAV IQ
      │  (float IQ @ Fs)
      ▼
  DDC  ── NCO mix + cascaded FIR decimation ──▶ complex channel @ ~24 kHz
      │
      ▼
FM demod ── quadrature discriminator + DC block + AGC ──▶ audio
      │
      ▼
SameDecoder
   ├─ band-pass 900–2700 Hz
   ├─ mark/space complex tone correlators (1-symbol window)
   ├─ first-order bit-clock phase loop (520.833 baud)
   ├─ 0xAB×8 preamble match → "ZCZC" sync match
   └─ LSB-first byte framing → parse → 3× repeat combine → emit
```

The SAME signal is 2-FSK on the audio: mark = 2083.3 Hz, space = 1562.5 Hz,
520.83 bit/s. Each header is sent three times, each preceded by a 16-byte
`0xAB` preamble. The header wire format is:

```
ZCZC-ORG-EEE-PSSCCC-PSSCCC...+TTTT-JJJHHMM-LLLLLLLL-
```

## Layout

```
src/
  main.cpp              CLI entry point and mode selection
  gui_main.cpp          GUI entry point (GLFW + OpenGL3 + ImGui host)
  app/                  options parsing, channel pipeline
  gui/                  GuiApp state + docked panels (Control/Alerts/Log/About)
  sdr/                  RTL-SDR + WAV IQ sources
  dsp/                  DDC, FM demod, small FFT
  eas/                  SAME decoder, message model, NOAA channel plan, tables
  audio/                waveform-audio playback
  util/                 logging, WAV read/write
third_party/imgui/      vendored Dear ImGui (docking branch)
```

Both executables link a shared static core library (`rtleascore`).

## Roadmap

- GUI (spectrum/waterfall) and live channel list.
- 1050 Hz attention-tone detection and automatic message alerting.
- Streaming output (UDP/JSON feed), logging to a message store.
- Multi-channel monitoring across a wideband capture.
