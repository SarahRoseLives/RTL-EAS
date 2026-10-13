// NOAA Weather Radio (NWR) channel plan. Shared by the CLI and the GUI so the
// channel list lives in one place. The seven NWR channels are the only
// frequencies on which SAME headers are broadcast.
#pragma once

#include <cstddef>

struct NoaaChannel
{
    double      freqMHz;
    const char* label; // "WX1".."WX7"
    const char* name;  // channel number / mnemonic
};

inline const NoaaChannel kNoaaChannels[] = {
    {162.400, "162.400 MHz", "WX-2"},
    {162.425, "162.425 MHz", "WX-4"},
    {162.450, "162.450 MHz", "WX-5"},
    {162.475, "162.475 MHz", "WX-3"},
    {162.500, "162.500 MHz", "WX-6"},
    {162.525, "162.525 MHz", "WX-7"},
    {162.550, "162.550 MHz", "WX-1"},
};
inline constexpr int kNoaaChannelCount = (int)(sizeof(kNoaaChannels) / sizeof(kNoaaChannels[0]));

// RTL-SDR sample rates offered by the GUI (Hz) and their labels. Even the
// lowest covers the whole 162.400-162.550 NWR band.
inline const double kRtlRates[] = {0.25e6, 1.024e6, 1.2e6, 1.536e6, 1.8e6, 2.048e6, 2.4e6, 2.56e6, 2.88e6, 3.2e6};
inline const char* const kRtlRateLabels[] = {"0.25", "1.024", "1.2", "1.536", "1.8", "2.048", "2.4", "2.56", "2.88", "3.2"};
inline constexpr int kRtlRateCount = (int)(sizeof(kRtlRates) / sizeof(kRtlRates[0]));
