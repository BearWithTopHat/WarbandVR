# 09 — Risk register: game, mod, Steam, Steam Frame, Phase 2

Everything we could not preemptively fix in code, with the failure mode,
early symptom, and the testing path to confirm/resolve. Ordered roughly by
bring-up sequence.

## A. Warband itself

| # | Risk | Symptom | Mitigation / test path |
|---|---|---|---|
| A1 | **Frame pacing lock.** The XR frame loop runs inside the game's render thread (`xrWaitFrame` in `SubmitFrame`). If stereo rendering can't hold HMD refresh, the whole game slows down. | Game logic slow-motion, input mush. | Check `[PERF]` rollup log; lower resolution; reduce battle size (Warband's real cost driver); if chronic, implement the async submission thread (documented in docs/01). |
| A2 | **UI/heuristic edge cases.** `SBSMonoUI` classifies by triangle count. Mods (or main menus with animated banners) may produce batches that straddle the threshold. | A UI element appears twice/shifted, or world geometry drawn unshifted. | Tune `UIMaxTriangles`; `DumpFrames` on the exact screen; worst case add an exclusion list by texture/shader hash (new work). |
| A3 | **Z-prepass / multi-viewport scenes.** Warband uses a Z-prepass; solved in M2 via full per-eye targets. Mods with custom post-processing shaders may bypass the assumed paths. | Mod-specific double vision or missing effects. | Test with plain Native first; add mods one at a time. |
| A4 | **Anti-cheat.** Single-player only — VAC/EAC never sees this. If anyone runs it in multiplayer anyway: don't (AGENTS.md invariant). | — | Config/docs explicitly scope to single-player. |
| A5 | **Mouse-look while head-tracking.** Head pose composes onto the game's view matrix; mouse-look still rotates the base view. They combine fine, but some players may feel "swimming". | Disorientation when using stick-look + head-look together. | Tune `SmoothingMinCutoff/Beta`; optionally add a future "head-dominant" mode (documented, not built). |
| A6 | **Riding/couch lance feel.** Horse riding moves the camera strongly; 1:1 head tracking on top can cause discomfort. | Discomfort during charges. | Comfort pass is an explicit M4 exit criterion; consider `WorldScale`/tracking damping on horseback (future tunable). |

## B. The mod

| # | Risk | Symptom | Mitigation / test path |
|---|---|---|---|
| B1 | **OpenXR loader must be x86.** SDKs commonly ship x64 binaries; linking/running the x64 loader fails silently at load or link. | Proxy loads but XR never initialises; or link error. | docs/05 step 1; log shows "no OpenXR runtime". |
| B2 | **Slow-path cost.** GetRenderTargetData stalls the D3D9 pipeline (~ms-scale per eye at high res). | Low fps in HMD despite GPU headroom. | Log reports the active path; enable `SharedSurface=1` after slow path is verified. |
| B3 | **Fast-path sync.** Cross-API D3D9Ex→D3D11 shared copy has no fence; we flush per eye, but driver-specific tearing is possible. | Tearing/judder only with `SharedSurface=1`. | Flip back to slow path; if confirmed, add a keyed-mutex (IDXGIKeyedMutex) path (documented upgrade, not built). |
| B4 | **Resize/Reset.** Handled (swapchains rebuild on eye-size change), but a Reset *during* an in-flight XR frame could theoretically interleave badly. | One-off glitch on resolution change. | Test: change resolution in-game repeatedly; watch log for the rebuild line. |
| B5 | **Steam overlay + wrapper.** The DeviceProxy wrapper exists *because* the overlay swaps vtables. Any future code path that bypasses the proxy breaks when the overlay opens. | Crash when opening Steam overlay. | Never hook vtables; keep the wrapper (AGENTS.md). Test overlay open/close in-session. |
| B6 | **SendInput injection loss.** If the game uses raw input with a capture window, injected mouse deltas could quantize attack directions. | Swings occasionally pick the wrong direction. | M5-3 50-swing test; if systemic, switch injection to `SendInput` batches timed across frames or investigate the engine input struct (M0-C). |
| B7 | **Menu lock false positives.** Static camera in *gameplay* (e.g. spectating a siege from a wall) freezes tracking for that stretch. | Tracking appears to drop intermittently. | `LockInMenus=0` disables; consider a manual toggle key (future). |
| B8 | **Pose sign conventions.** Right-handed→left-handed mapping assumed; defaults ship `InvertPosZ=1`. | World moves opposite to head on one axis. | M4-2 procedure: fix from ini `Invert*` keys, no rebuild. |
| B9b | **Gesture zone misfires.** Body-relative grab boxes assume average proportions/stance; seated or very tall/short players may grab "air". | Grab switches the wrong weapon or nothing. | Zones are plain boxes in `ClassifyZone()` - tune; or add a calibration log (press a key while holding hand at each zone, read [GESTURE] positions) |
| B9d | **ReShade/SweetFX filename collision**: those injectors also want to be `d3d9.dll` in the game dir. | Only one loads; silent loss of the other. | Chain-load (rename one; our forwarding target can point at the renamed DLL). Untested - document any working combo |
| B9c | **Temporal post-processing** (ReShade/SweetFX with TAA-like effects, frame-gen) breaks stereo/eye coherence, same class of bug as D2R VR's no-DLSS/no-TAA rule. | Ghosting/smearing only with post-processing loaded. | Remove injectors; our vsync override (ForceNoVsync) already handles the pacing side |
| B9 | **Swapchain sized to game res, not runtime-recommended.** | Slightly soft image if the runtime upscales. | If soft, scale eye surfaces ×runtime-recommended/game-res (new config; straightforward change in `CreateSwapchains`). |

## C. Steam / streaming (Phase 1: PC host → Steam Frame)

| # | Risk | Symptom | Mitigation / test path |
|---|---|---|---|
| C1 | **Runtime choice.** SteamVR must be the *active* OpenXR runtime on the PC. If another runtime (Oculus, WMR) claims it, our session lands elsewhere. | XR session runs but the Frame shows nothing. | SteamVR → Settings → OpenXR → "Set as active runtime". First-run checklist item. |
| C2 | **Controller path.** Controllers paired to the Frame reach SteamVR only if Steam Link VR forwarding is active. | Head tracking works; controllers dead. | Verify controllers visible in SteamVR's binding UI on the host *before* judging MotionInput. |
| C3 | **Streaming adds latency outside our control.** Encode+network+decode (~tens of ms) sits on top of our <1ms injection path. | Motion-to-photon lag despite perfect mod timing. | Wired/5 GHz close-range streaming; this is inherent to Phase 1 architecture and a primary argument for Phase 2. |
| C4 | **Steam Link VR reprojection** on the Frame masks some frame drops (good) but can artifact with fast head turns (bad). | Wobble on fast turns even at good fps. | If persistent, try disabling Steam Link's reprojection; see A1 for fps. |
| C5 | **Desktop window focus.** SendInput targets the system; if focus leaves the game window, emulated keys go elsewhere. | Controls dead after alt-tab. | Refocus the game; consider future focus-check before injection. |

## D. Phase 2: hosted natively on the Steam Frame

A Native package/profile now exists and shares the same source tree as PCVR,
but the Phase 2 runtime path has not been executed on Steam Frame hardware.
Treat this section as a bring-up plan with explicit failure gates, not as a
verified port.

The Frame is ARM64 (Snapdragon) running SteamOS; Warband would run under
**Proton + FEX** (x86 emulation). Implications:

| # | Question | Assessment | Test path |
|---|---|---|---|
| D1 | Does the proxy DLL work under Proton? | **Plausibly yes.** Our `d3d9.dll` is a normal x86 Windows DLL; Proton's app-dir load order matches Windows. The "real" d3d9 we forward to is Wine's/DXVK's — same interface contract. | Phase 2 step 1: deploy the *SBS* build (no XR) under Proton on the Frame and check the log. This validates the proxy, wrapper, and stereo renderer before touching XR. |
| D2 | Does OpenXR work from inside Proton? | **The hard part.** Wine's OpenXR layer (wineopenxr) forwards to a Linux runtime (Monado). Status of `XR_KHR_D3D11_enable` through that stack is uncertain and version-dependent. | Phase 2 step 2: build Release-OpenXR, run, read the log — failure mode is explicit (`xrCreateInstance`/`xrGetD3D11GraphicsRequirementsKHR` errors). If the D3D11 binding is unsupported, options: (a) wait for/extend Wine support, (b) add a Vulkan interop path (large), (c) stay on streaming. |
| D3 | Performance under FEX? | Unknown. Warband is a 2010 CPU-bound DX9 title; FEX overhead is real but the game's requirements are modest. | Measure flat first: stock game via Proton on the Frame before adding stereo. |
| D4 | Controller pairing natively. | Should be native to SteamOS/Monado — likely *better* than streamed (C2/C3 disappear). | Validate during D2. |
| D5 | DXVK vs WineD3D. | Under Proton, d3d9 is DXVK (Vulkan). Our proxy forwards to it transparently. The D3D9Ex shared-surface path's behavior under DXVK is untested — **use the slow path or SBS for Phase 2 bring-up**. | Test matrix in phase-2 plan. |

**Phase 2 recommended order:** D3 (flat perf) → D1 (SBS proxy) → D2 (XR
session) → then fast-path/fast-fail tuning. Each stage has a log-defined
success line, so the first broken stage is identifiable immediately.

## E. Already-fixed-in-code (this pass)

For the record, so nobody re-investigates: bridge shutdown on
DLL_PROCESS_DETACH (stuck keys/leaked session), init-retry every 5 s (runtime
coming up after launch), swapchain rebuild on resolution change, projection
layer suppressed when pose locate fails (invalid zero-quaternion), MotionInput
self-retry, per-eye flush on the fast path.
