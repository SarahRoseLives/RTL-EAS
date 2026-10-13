// Thread-safe logger. Verbose lines go to stderr; everything is also
// appended to log.txt so a long unattended run leaves a record behind.
#pragma once

#include <cstdio>

enum class LogLevel
{
    Info,
    Debug,
};

void logSetVerbose(bool on);
bool logVerbose();

void logWrite(LogLevel level, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
