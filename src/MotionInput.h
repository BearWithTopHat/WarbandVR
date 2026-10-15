// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// MotionInput (M5): OpenXR actions, controller->game input emulation, and the
// melee swing classifier.
//
// Two operating modes, both off unless the OpenXR build is active:
//   - Emulation ([Input] EmulateControllers=1, the default): controllers
//     reproduce the user's existing flat controller scheme via SendInput
//     (sticks -> WASD/mouse, triggers -> mouse buttons, face buttons ->
//     configurable keys). Menus, map, movement, camera: solved.
//   - Melee ([Input] MotionMelee=1): right-hand swing velocity is classified
//     into Warband's four attack directions and injected as the
//     mouse-direction + attack-click sequence the engine expects; right grip
//     is directional block. The emulation layer stays active for everything
//     else.
//
// All injection is single-player-only by repo invariant. Tunables live in
// [Input] / [InputMap] in WarbandVR.ini.

namespace MotionInput
{
    // Called by OpenXRBridge once the XrSession exists (OpenXR build only).
    bool Init();
    void Shutdown();

    // Called once per submitted frame (from OpenXRBridge::SubmitFrame).
    void OnFrame();
}
