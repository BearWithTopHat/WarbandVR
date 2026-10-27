// SPDX-License-Identifier: GPL-3.0-only
#include "MouseMap.h"
#include "Config.h"
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
}

void MouseMap::Uninstall()
{
    if (g_hook) UnhookWindowsHookEx(g_hook);
    g_hook = nullptr;
    g_hwnd = nullptr;
}
