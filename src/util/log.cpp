#include "util/log.h"

#include <atomic>
#include <cstdarg>
#include <ctime>
#include <mutex>

namespace
{

std::atomic<bool> g_verbose{false};
std::mutex        g_mtx;
std::FILE*        g_file = nullptr;

void ensureOpen()
{
    if (!g_file)
    {
        g_file = std::fopen("log.txt", "ab");
    }
}

void stamp(char* buf, size_t n)
{
    std::time_t t = std::time(nullptr);
    std::tm     tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::snprintf(buf, n, "[%02d:%02d:%02d] ", tm.tm_hour, tm.tm_min, tm.tm_sec);
}

} // namespace

void logSetVerbose(bool on)
{
    g_verbose.store(on);
}

bool logVerbose()
{
    return g_verbose.load();
}

void logWrite(LogLevel level, const char* fmt, ...)
{
    if (level == LogLevel::Debug && !g_verbose.load())
    {
        return;
    }

    char prefix[32];
    stamp(prefix, sizeof(prefix));

    char body[2048];
    std::va_list args;
    va_start(args, fmt);
    std::vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);

    std::lock_guard<std::mutex> lk(g_mtx);
    std::fprintf(stderr, "%s%s\n", prefix, body);
    std::fflush(stderr);

    ensureOpen();
    if (g_file)
    {
        std::fprintf(g_file, "%s%s\n", prefix, body);
        std::fflush(g_file);
    }
}
