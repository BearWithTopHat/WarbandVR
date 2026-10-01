#include "Logger.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace
{
    FILE* g_file = nullptr;
    int   g_level = Log::Info;
    CRITICAL_SECTION g_cs;
    bool g_csInit = false;

    const char* Tag(Log::Level l)
    {
        switch (l)
        {
        case Log::Error:   return "ERR";
        case Log::Warn:    return "WRN";
        case Log::Info:    return "INF";
        default:           return "VRB";
        }
    }
}

void Log::Init(int level)
{
    if (g_file) return;

    // Log lives next to OUR dll (the game directory), not the cwd.
    char path[MAX_PATH] = {};
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&Log::Init), &self);
    if (self)
    {
        GetModuleFileNameA(self, path, MAX_PATH);
        char* slash = strrchr(path, '\\');
        if (slash) slash[1] = '\0';
        strcat_s(path, "WarbandVR_log.txt");
    }
    else
    {
        strcpy_s(path, "WarbandVR_log.txt");
    }

    fopen_s(&g_file, path, "w");
    if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; }
    g_level = level;

    Write(Info, "=== WarbandVR log started ===");
}

void Log::Shutdown()
{
    if (!g_file) return;
    Write(Info, "=== WarbandVR log closed ===");
    EnterCriticalSection(&g_cs);
    fclose(g_file);
    g_file = nullptr;
    LeaveCriticalSection(&g_cs);
}

void Log::SetLevel(int level) { g_level = level; }

void Log::Write(Level level, const char* fmt, ...)
{
    if (!g_file || level > g_level) return;

    char msg[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    msg[sizeof(msg) - 1] = '\0';

    time_t now = time(nullptr);
    tm t{};
    localtime_s(&t, &now);

    EnterCriticalSection(&g_cs);
    fprintf(g_file, "[%02d:%02d:%02d][%s] %s\n",
            t.tm_hour, t.tm_min, t.tm_sec, Tag(level), msg);
    fflush(g_file); // a crash must never eat the last log line
    LeaveCriticalSection(&g_cs);
}
