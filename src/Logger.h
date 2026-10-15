// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Simple file logger. Writes WarbandVR_log.txt next to the proxy DLL.
// Thread-safe, tiny, no dependencies.

namespace Log
{
    enum Level { Error = 0, Warn = 1, Info = 2, Verbose = 3 };

    void Init(int level);        // call once from proxy init
    void Shutdown();
    void SetLevel(int level);

    // Crash logging (docs/15, BannerlordVR lesson): a vectored exception
    // handler that logs hard faults as module+RVA before the engine's own
    // handlers run. Installed by Init; never swallows the exception.
    void InstallCrashHandler();
    long CrashLogForTest(void* exceptionPointers); // handler body, test-visible

    void Write(Level level, const char* fmt, ...);
}

#define LOG_ERROR(...) Log::Write(Log::Error,   __VA_ARGS__)
#define LOG_WARN(...)  Log::Write(Log::Warn,    __VA_ARGS__)
#define LOG_INFO(...)  Log::Write(Log::Info,    __VA_ARGS__)
#define LOG_VERB(...)  Log::Write(Log::Verbose, __VA_ARGS__)
