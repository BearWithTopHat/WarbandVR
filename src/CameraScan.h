// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// CameraScan: M0 memory-reconnaissance helper (task M0-2).
//
// With [Debug] CameraScan=1 and ExpectedPos="x,y,z" (a position the human
// reads off a known in-game spot), scans a memory range for matching float
// triples, then re-reads candidates over time and scores those whose values
// MOVE as the player moves - the signature of a live camera position.
//
// Default scan range = the .data section of mb_warband.exe (engine globals).
// Override with [Debug] ScanBase / ScanSize (hex).
//
// Rate-limited to one pass every 500 ms and page-guarded via VirtualQuery,
// so it is safe to leave on while walking around in-game.

namespace CameraScan
{
    void Init();        // resolve scan range; call once at proxy startup
    void OnFrame();     // call every frame (from hkEndScene); self-throttles
    void Shutdown();
}
