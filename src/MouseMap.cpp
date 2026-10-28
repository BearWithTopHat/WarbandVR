// SPDX-License-Identifier: GPL-3.0-only
#include "MouseMap.h"
#include "Config.h"
#include "Logger.h"
#include "StereoRenderer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

namespace
{
    HHOOK g_hook = nullptr;
    HWND g_hwnd = nullptr;

    LRESULT CALLBACK RemapProc(int code, WPARAM wp, LPARAM lp)
    {
        if (code == HC_ACTION)
        {
            MSG* msg = reinterpret_cast<MSG*>(lp);
            const bool mouseMsg =
                msg->message == WM_MOUSEMOVE ||
                (msg->message >= WM_LBUTTONDOWN && msg->message <= WM_MBUTTONDBLCLK);
            if (mouseMsg && g_hwnd)
            {
                const Config& cfg = Config::Get();
                const UINT eyeW = StereoRenderer::Get().EyeWidth();
                const UINT eyeH = StereoRenderer::Get().EyeHeight();
                if (cfg.enableStereo && cfg.outputMode == 0 && cfg.sbsMouseRemap &&
                    eyeW != 0 && eyeH != 0)
                {
                    RECT rc{};
                    GetClientRect(g_hwnd, &rc);
                    const long halfW = rc.right / 2;
                    const long bbH = rc.bottom;
                    const long x = GET_X_LPARAM(msg->lParam);
                    const long y = GET_Y_LPARAM(msg->lParam);
                    const int eye = (x >= halfW) ? 1 : 0;
                    long lx = x, ly = y;
                    MouseMap::ClientToLogical(eye, x, y, halfW, bbH,
                                              static_cast<long>(eyeW),
                                              static_cast<long>(eyeH),
                                              cfg.sbsAspectCorrect, &lx, &ly);
                    msg->lParam = static_cast<LPARAM>(
                        (static_cast<DWORD>(lx) & 0xFFFFu) |
                        ((static_cast<DWORD>(ly) & 0xFFFFu) << 16));
                }
            }
        }
        return CallNextHookEx(g_hook, code, wp, lp);
    }
}

void MouseMap::Install(void* gameWindow)
{
    if (g_hook) return;
    g_hwnd = static_cast<HWND>(gameWindow);
    if (!g_hwnd) return;
    const DWORD tid = GetWindowThreadProcessId(g_hwnd, nullptr);
    if (!tid) { g_hwnd = nullptr; return; }
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&RemapProc), &self);
    g_hook = SetWindowsHookExA(WH_GETMESSAGE, RemapProc, self, tid);
    LOG_INFO("MouseMap: message hook %s (hwnd=%p tid=%lu)",
             g_hook ? "installed" : "FAILED", static_cast<void*>(g_hwnd), tid);
    InstallPollingHooks();
}

void MouseMap::Uninstall()
{
    if (g_hook) UnhookWindowsHookEx(g_hook);
    g_hook = nullptr;
    g_hwnd = nullptr;
}

// ---------------------------------------------------------------------------
// IAT hook: user32!ScreenToClient
//
// Warband does NOT hit-test from WM_MOUSEMOVE lParams - it polls the cursor
// every frame with GetCursorPos + ScreenToClient (see the exe's import
// table). The WH_GETMESSAGE remap above therefore never reaches its menu
// logic. Patching the exe's IAT entry for ScreenToClient puts the remap on
// the value the game actually reads.
// ---------------------------------------------------------------------------

namespace
{
    typedef BOOL(WINAPI* ScreenToClient_t)(HWND, LPPOINT);
    ScreenToClient_t g_origScreenToClient = nullptr;
    int g_loggedS2C = 0;

    // TEMP DIAG: observe raw GetCursorPos polling (tooltip-layout probe).
    typedef BOOL(WINAPI* GetCursorPos_t)(LPPOINT);
    GetCursorPos_t g_origGetCursorPos = nullptr;
    int g_loggedGCP = 0;

    BOOL WINAPI GetCursorPos_Hook(LPPOINT pt)
    {
        const BOOL ok = g_origGetCursorPos(pt);
        if (ok && pt && Config::Get().enableStereo && StereoRenderer::Get().RbLogActive() && g_loggedGCP < 40)
        {
            ++g_loggedGCP;
            LOG_INFO("MouseMap: GetCursorPos -> (%ld,%ld)", pt->x, pt->y);
        }
        return ok;
    }

    BOOL RemapActive(UINT* eyeW, UINT* eyeH)
    {
        const Config& cfg = Config::Get();
        *eyeW = StereoRenderer::Get().EyeWidth();
        *eyeH = StereoRenderer::Get().EyeHeight();
        return cfg.enableStereo && cfg.outputMode == 0 && cfg.sbsMouseRemap &&
               *eyeW != 0 && *eyeH != 0;
    }

    BOOL WINAPI ScreenToClient_Hook(HWND hwnd, LPPOINT pt)
    {
        const BOOL ok = g_origScreenToClient(hwnd, pt);
        if (!ok || !pt || hwnd != g_hwnd) return ok;

        UINT eyeW = 0, eyeH = 0;
        if (!RemapActive(&eyeW, &eyeH)) return ok;

        // Only remap when the converted point IS the live cursor position.
        // The game also calls ScreenToClient for layout/clamping math (the
        // map tooltip does); remapping those displaces the drawn content.
        POINT cur{};
        if (!GetCursorPos(&cur)) return ok;
        POINT clientCur = cur;
        if (!g_origScreenToClient(hwnd, &clientCur)) return ok;
        if (labs(pt->x - clientCur.x) > 2 || labs(pt->y - clientCur.y) > 2)
        {
            if (g_loggedS2C < 20)
            {
                ++g_loggedS2C;
                LOG_INFO("MouseMap: ScreenToClient passthrough (non-cursor point %ld,%ld)", pt->x, pt->y);
            }
            return ok;
        }

        RECT rc{};
        GetClientRect(hwnd, &rc);
        const long halfW = rc.right / 2;
        const int eye = (pt->x >= halfW) ? 1 : 0;
        long lx = pt->x, ly = pt->y;
        MouseMap::ClientToLogical(eye, pt->x, pt->y, halfW, rc.bottom,
                                  static_cast<long>(eyeW), static_cast<long>(eyeH),
                                  Config::Get().sbsAspectCorrect, &lx, &ly);
        if (g_loggedS2C < 20)
        {
            ++g_loggedS2C;
            LOG_INFO("MouseMap: ScreenToClient (%ld,%ld) eye=%d -> (%ld,%ld)",
                     pt->x, pt->y, eye, lx, ly);
        }
        pt->x = lx;
        pt->y = ly;
        return ok;
    }

    // Replace the exe's IAT entries for dllName!fnName with newFn. The exe is
    // a 32-bit PE; we are in-process, so this is a plain memory patch.
    int PatchIat(const char* dllName, const char* fnName, void* newFn, void** oldFn)
    {
        HMODULE exe = GetModuleHandleA(nullptr);
        if (!exe) return 0;
        const auto base = reinterpret_cast<BYTE*>(exe);
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
        const auto dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dir.VirtualAddress) return 0;

        int patched = 0;
        for (auto* d = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
             d->Name; ++d)
        {
            const char* dll = reinterpret_cast<const char*>(base + d->Name);
            if (_stricmp(dll, dllName) != 0) continue;

            const auto nameThunks = d->OriginalFirstThunk
                ? reinterpret_cast<const IMAGE_THUNK_DATA32*>(base + d->OriginalFirstThunk)
                : reinterpret_cast<const IMAGE_THUNK_DATA32*>(base + d->FirstThunk);
            auto* iat = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + d->FirstThunk);
            for (auto* t = nameThunks; t->u1.AddressOfData; ++t, ++iat)
            {
                if (t->u1.Ordinal & IMAGE_ORDINAL_FLAG32) continue;
                const char* name = reinterpret_cast<const char*>(
                    base + t->u1.AddressOfData) + 2; // skip hint
                if (strcmp(name, fnName) != 0) continue;

                DWORD oldProt = 0;
                if (!VirtualProtect(&iat->u1.Function, sizeof(DWORD), PAGE_READWRITE, &oldProt))
                    continue;
                if (oldFn && !*oldFn)
                    *oldFn = reinterpret_cast<void*>(iat->u1.Function);
                iat->u1.Function = reinterpret_cast<DWORD>(newFn);
                VirtualProtect(&iat->u1.Function, sizeof(DWORD), oldProt, &oldProt);
                ++patched;
            }
        }
        return patched;
    }
}

void MouseMap::InstallPollingHooks()
{
    const int patched = PatchIat("user32.dll", "ScreenToClient",
                                 reinterpret_cast<void*>(&ScreenToClient_Hook),
                                 reinterpret_cast<void**>(&g_origScreenToClient));
    if (patched > 0)
        LOG_INFO("MouseMap: ScreenToClient IAT hooked (%d entr%s)", patched,
                 patched == 1 ? "y" : "ies");
    else
        LOG_WARN("MouseMap: ScreenToClient IAT entry not found - menu clicks will stay unmapped");

    // Observation-only GetCursorPos hook (does not remap).
    const int gcp = PatchIat("user32.dll", "GetCursorPos",
                             reinterpret_cast<void*>(&GetCursorPos_Hook),
                             reinterpret_cast<void**>(&g_origGetCursorPos));
    LOG_INFO("MouseMap: GetCursorPos IAT %s (%d)", gcp > 0 ? "hooked" : "not found", gcp);
}
