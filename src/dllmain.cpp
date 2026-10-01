// ============================================================================
// WarbandVR - d3d9.dll proxy entry point and export forwarders.
//
// The game loads this file as "d3d9.dll" because it sits next to
// mb_warband.exe (DLL search order: application directory first). We load the
// REAL system d3d9.dll by absolute path, forward everything we don't care
// about, and wrap Direct3DCreate9 so device creation flows through us.
// ============================================================================

#include "D3D9Proxy.h"
#include "D3D9ExProxy.h"
#include "DeviceHooks.h"
#include "StereoRenderer.h"
#include "CameraScan.h"
#include "Config.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>

namespace
{
    HMODULE g_realD3D9 = nullptr;

    HMODULE RealD3D9()
    {
        if (!g_realD3D9)
        {
            char sysPath[MAX_PATH] = {};
            // In a 32-bit process GetSystemDirectory returns SysWOW64 on
            // 64-bit Windows - i.e. the 32-bit d3d9.dll, which is what we want.
            GetSystemDirectoryA(sysPath, MAX_PATH);
            strcat_s(sysPath, "\\d3d9.dll");
            g_realD3D9 = LoadLibraryA(sysPath);
            if (!g_realD3D9)
                LOG_ERROR("Failed to load real d3d9.dll from %s (err=%lu)", sysPath, GetLastError());
        }
        return g_realD3D9;
    }

    FARPROC RealProc(const char* name)
    {
        HMODULE m = RealD3D9();
        if (!m) return nullptr;
        FARPROC p = GetProcAddress(m, name);
        if (!p) LOG_WARN("Real d3d9.dll has no export '%s'", name);
        return p;
    }

    bool g_started = false;

    void Startup()
    {
        if (g_started) return;
        g_started = true;
        Log::Init(2);
        Config::Get().Load(); // Load() applies the ini's LogLevel
        LOG_INFO("WarbandVR proxy initialised");

        // Version tagging: every offset table in docs/04 is keyed to a game
        // build; log the exe's identity so M0 records are unambiguous.
        char exePath[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH))
        {
            HANDLE h = CreateFileA(exePath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE)
            {
                LARGE_INTEGER size{};
                FILETIME writeTime{};
                GetFileSizeEx(h, &size);
                GetFileTime(h, nullptr, nullptr, &writeTime);
                CloseHandle(h);
                LOG_INFO("Host exe: %s (%lld bytes, mtime %08lX%08lX)",
                         exePath, size.QuadPart, writeTime.dwHighDateTime, writeTime.dwLowDateTime);
            }
        }

        CameraScan::Init();
    }
}

// ---------------------------------------------------------------------------
// Exports we implement
// ---------------------------------------------------------------------------

// All exports are listed in WarbandVR.def; marking them __declspec(dllexport)
// here would clash with d3d9.h's own declarations (C2375), so we don't.
extern "C"
IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion)
{
    Startup();

    typedef IDirect3D9* (WINAPI *Fn)(UINT);
    static Fn real = reinterpret_cast<Fn>(RealProc("Direct3DCreate9"));
    if (!real) return nullptr;

    IDirect3D9* realObj = real(SDKVersion);
    if (!realObj)
    {
        LOG_ERROR("Real Direct3DCreate9 returned null");
        return nullptr;
    }

    LOG_INFO("Direct3DCreate9(SDK=%u) -> wrapping real object %p", SDKVersion, realObj);
    return new D3D9Proxy(realObj);
}

extern "C"
HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex** ppD3D)
{
    Startup();

    typedef HRESULT (WINAPI *Fn)(UINT, IDirect3D9Ex**);
    static Fn real = reinterpret_cast<Fn>(RealProc("Direct3DCreate9Ex"));
    if (!real) return E_FAIL;

    IDirect3D9Ex* realObj = nullptr;
    HRESULT hr = real(SDKVersion, &realObj);
    if (FAILED(hr) || !realObj)
    {
        LOG_ERROR("Real Direct3DCreate9Ex failed hr=0x%08X", hr);
        return hr;
    }

    LOG_INFO("Direct3DCreate9Ex(SDK=%u) -> wrapping real Ex object %p", SDKVersion, realObj);
    *ppD3D = new D3D9ExProxy(realObj);
    return S_OK;
}

// ---------------------------------------------------------------------------
// Forwarded exports (thin pass-throughs, resolved at runtime)
// ---------------------------------------------------------------------------

extern "C" void* WINAPI Direct3DShaderValidatorCreate9()
{
    typedef void* (WINAPI *Fn)();
    static Fn real = reinterpret_cast<Fn>(RealProc("Direct3DShaderValidatorCreate9"));
    return real ? real() : nullptr;
}

extern "C" int WINAPI D3DPERF_BeginEvent(D3DCOLOR color, LPCWSTR name)
{
    typedef int (WINAPI *Fn)(D3DCOLOR, LPCWSTR);
    static Fn real = reinterpret_cast<Fn>(RealProc("D3DPERF_BeginEvent"));
    return real ? real(color, name) : 0;
}

extern "C" int WINAPI D3DPERF_EndEvent()
{
    typedef int (WINAPI *Fn)();
    static Fn real = reinterpret_cast<Fn>(RealProc("D3DPERF_EndEvent"));
    return real ? real() : 0;
}

extern "C" void WINAPI D3DPERF_SetMarker(D3DCOLOR color, LPCWSTR name)
{
    typedef void (WINAPI *Fn)(D3DCOLOR, LPCWSTR);
    static Fn real = reinterpret_cast<Fn>(RealProc("D3DPERF_SetMarker"));
    if (real) real(color, name);
}

extern "C" void WINAPI D3DPERF_SetRegion(DWORD color, LPCWSTR name)
{
    typedef void (WINAPI *Fn)(DWORD, LPCWSTR);
    static Fn real = reinterpret_cast<Fn>(RealProc("D3DPERF_SetRegion"));
    if (real) real(color, name);
}

extern "C" BOOL WINAPI D3DPERF_QueryRepeatFrame()
{
    typedef BOOL (WINAPI *Fn)();
    static Fn real = reinterpret_cast<Fn>(RealProc("D3DPERF_QueryRepeatFrame"));
    return real ? real() : FALSE;
}

extern "C" void WINAPI D3DPERF_SetOptions(DWORD options)
{
    typedef void (WINAPI *Fn)(DWORD);
    static Fn real = reinterpret_cast<Fn>(RealProc("D3DPERF_SetOptions"));
    if (real) real(options);
}

extern "C" DWORD WINAPI D3DPERF_GetStatus()
{
    typedef DWORD (WINAPI *Fn)();
    static Fn real = reinterpret_cast<Fn>(RealProc("D3DPERF_GetStatus"));
    return real ? real() : 0;
}

extern "C" void WINAPI DebugSetMute()
{
    typedef void (WINAPI *Fn)();
    static Fn real = reinterpret_cast<Fn>(RealProc("DebugSetMute"));
    if (real) real();
}

extern "C" int WINAPI DebugSetLevel(DWORD level)
{
    typedef int (WINAPI *Fn)(DWORD);
    static Fn real = reinterpret_cast<Fn>(RealProc("DebugSetLevel"));
    return real ? real(level) : 0;
}

// ---------------------------------------------------------------------------
// DLL entry
// ---------------------------------------------------------------------------

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hInst);
        // Deliberately minimal: real startup happens on first Direct3DCreate9
        // so we never do heavy work inside the loader lock.
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        CameraScan::Shutdown();
        DeviceHooks::Get().Shutdown();
        StereoRenderer::Get().Shutdown();
        Log::Shutdown();
    }
    return TRUE;
}
