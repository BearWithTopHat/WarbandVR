// SPDX-License-Identifier: GPL-3.0-only
#include "Logger.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <share.h>

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

    // Keep the previous run's log (BannerlordVR lesson): logs are rewritten
    // each launch, and a crash-then-relaunch would otherwise destroy the
    // only evidence. Bug reports want both files.
    {
        char prev[MAX_PATH];
        strcpy_s(prev, path);
        char* dot = strstr(prev, ".txt");
        if (dot) *dot = '\0';
        strcat_s(prev, ".prev.txt");
        MoveFileExA(path, prev, MOVEFILE_REPLACE_EXISTING);
    }

    // Open with read sharing so the log can be tailed live while the game
    // runs (it is our only debugging tool in there).
    g_file = _fsopen(path, "w", _SH_DENYNO);
    if (!g_file) fopen_s(&g_file, path, "w");
    if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; }
    g_level = level;
    InstallCrashHandler();

    Write(Info, "=== WarbandVR log started ===");
}

// ---------------------------------------------------------------------------
// Crash logging (docs/15, BannerlordVR lesson).
//
// Warband ships no PDBs, so a bare fault offset says nothing - but
// module+RVA says whose code died (our proxy, the engine, the overlay, a
// driver). The handler logs the hard faults and ALWAYS continues the search:
// Steam's and the game's own handlers still run. File I/O from a crash
// context is technically unsound and deliberately accepted here - the log is
// the only debugging tool inside the game, and the write is a single
// best-effort line with no allocation.
// ---------------------------------------------------------------------------
namespace
{
    void* g_veh = nullptr;

    long __stdcall VehCrashLog(PEXCEPTION_POINTERS ep)
    {
        const DWORD code = ep->ExceptionRecord->ExceptionCode;
        const unsigned char* eip =
            static_cast<const unsigned char*>(ep->ExceptionRecord->ExceptionAddress);
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_STACK_OVERFLOW:
        {
            HMODULE hm = nullptr;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(eip), &hm);
            char mod[MAX_PATH] = "??";
            unsigned long rva = 0;
            if (hm)
            {
                GetModuleFileNameA(hm, mod, MAX_PATH);
                const char* slash = strrchr(mod, '\\');
                if (slash) memmove(mod, slash + 1, strlen(slash));
                rva = (unsigned long)(eip - (const unsigned char*)hm);
            }
            LOG_ERROR("CRASH code=0x%08lX at %s+0x%lX", code, mod, rva);
            break;
        }
        default:
            break;
        }
        return EXCEPTION_CONTINUE_SEARCH; // never swallow
    }
}

void Log::InstallCrashHandler()
{
    if (g_veh) return;
    g_veh = AddVectoredExceptionHandler(1, VehCrashLog);
}

long Log::CrashLogForTest(void* exceptionPointers)
{
    return VehCrashLog(static_cast<PEXCEPTION_POINTERS>(exceptionPointers));
}

void Log::Shutdown()
{
    if (g_veh) { RemoveVectoredExceptionHandler(g_veh); g_veh = nullptr; }
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
