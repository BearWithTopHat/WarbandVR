// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// MouseMap: split-panel cursor remapping for SBS output.
//
// The game hit-tests UI in FULL-WINDOW logical coordinates, but the user
// moves the cursor in SPLIT-PANEL coordinates. Without remapping, a hover
// over empty space can light up a button (the "Options highlighted while
// outside the menu" bug). The mapping below is the exact inverse of the
// SBS composite transform, so the game receives the logical position the
// user actually points at. Pure math is host-testable; the Windows message
// hook lives in MouseMap.cpp.

namespace MouseMap
{
    struct Panel { float scaleX, scaleY; long x0, y0; };

    // Composite transform for one eye panel. Kept in sync with
    // StereoRenderer::CompositeToBackbuffer.
    inline Panel PanelForEye(int eye, long halfW, long bbH, long eyeW, long eyeH,
                             bool aspectCorrect)
    {
        if (!aspectCorrect)
        {
            // Old stretch-to-fill: X is squeezed into the panel, Y fills the
            // window height (independent scales).
            return { halfW / static_cast<float>(eyeW),
                     bbH / static_cast<float>(eyeH),
                     eye * halfW, 0L };
        }

        const float sx = halfW / static_cast<float>(eyeW);
        const float sy = bbH / static_cast<float>(eyeH);
        const float s = sx < sy ? sx : sy;
        const long w = static_cast<long>(eyeW * s + 0.5f);
        const long h = static_cast<long>(eyeH * s + 0.5f);
        return { s, s, eye * halfW + (halfW - w) / 2, (bbH - h) / 2 };
    }

    // Client (window) coordinates -> the game's full-window logical
    // coordinates, for the panel under the cursor.
    inline void ClientToLogical(int eye, long x, long y, long halfW, long bbH,
                                long eyeW, long eyeH, bool aspectCorrect,
                                long* outX, long* outY)
    {
        const Panel p = PanelForEye(eye, halfW, bbH, eyeW, eyeH, aspectCorrect);
        *outX = static_cast<long>((x - p.x0) / p.scaleX);
        *outY = static_cast<long>((y - p.y0) / p.scaleY);
    }

    // Installs the WH_GETMESSAGE remap on the window's thread. Idempotent.
    void Install(void* gameWindow);
    // IAT-hooks the polling path (user32!ScreenToClient) - Warband reads the
    // cursor position by polling, not from WM_* messages.
    void InstallPollingHooks();
    void Uninstall();
}
