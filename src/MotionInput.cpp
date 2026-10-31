// SPDX-License-Identifier: GPL-3.0-only
// WarbandVR - GPL v3 - see LICENSE
//
// MotionInput (M5): OpenXR actions -> Warband input.
//
// Two layers, independent toggles:
//
//   [Input] EmulateControllers=1  (default)
//     Controllers emulate the flat kb/mouse scheme: left stick -> WASD,
//     right stick -> mouse look, face buttons -> [InputMap] keys,
//     right trigger/squeeze -> LMB/RMB. This alone keeps every menu, the
//     map, movement and camera fully playable without a kb/mouse.
//
//   [Input] MotionMelee=1
//     Right-hand motion drives attacks:
//       - Swing classifier: grip-space linear velocity is ring-buffered; a
//         swing is detected on threshold crossing and classified by the
//         dominant axis at peak velocity into Warband's 4 attack directions
//         (left / right / overhead / thrust).
//       - Injection: Warband picks the attack direction from mouse movement
//         at attack-press time, so an attack = mouse-delta + LMB press. The
//         press is HELD (not instantly released) and, with MotionSteer=1,
//         per-frame hand motion keeps injecting mouse deltas while held -
//         this uses Warband's native "mouse movement during a held attack
//         steers the swing" behaviour, so the swing follows the hand's arc
//         instead of being a fixed canned direction. LMB releases when the
//         hand slows or SteerMaxHoldMs elapses.
//       - Block: right grip held (> BlockHoldTime) -> RMB held.
//
// Action model: one action per control, with BOTH /user/hand subaction
// paths; reads specify the subaction path. This is the spec-correct model
// (actions with subactions + per-hand suggested bindings + per-hand action
// spaces); an earlier draft created per-hand actions without subactions,
// which made xrCreateActionSpace(subactionPath) illegal.
//
// Latency: swing -> SendInput is logged ([MELEE] lines); game-side latency
// is measured with high-speed video during M5 tuning (docs/03 M5-3).
//
// All SendInput happens on the render thread (called from
// OpenXRBridge::SubmitFrame -> OnFrame) - never from DllMain.

#include "MotionInput.h"
#include "OpenXRBridge.h"
#include "Config.h"
#include "Logger.h"
#include "MeleeCore.h"
#include "GestureZones.h"
#include "InputSink.h"
#include "PointerMath.h"
#include "MathUtil.h"
#include "ViewState.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// --- input sink (both build flavors) -----------------------------------------
// Null sink: drops everything. Production installs SendInputSink at Init;
// headless tests install a recording fake. See InputSink.h.
namespace
{
    class NullSink : public IInputSink
    {
    public:
        void Key(int, bool) override {}
        void MouseButton(bool, bool) override {}
        void MouseMove(int, int) override {}
        void MouseMoveAbsolute(int, int) override {}
    };
    NullSink   g_nullSink;
    IInputSink* g_sink = &g_nullSink;
}

void InputSink::Install(IInputSink* sink) { g_sink = sink ? sink : &g_nullSink; }
IInputSink& InputSink::Get() { return *g_sink; }

#ifdef WBVR_WITH_OPENXR

// OpenXR structs are brace-initialised with just their type tag; the rest is
// zero. That idiom trips -Wmissing-field-initializers on every XR struct.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>

namespace
{
    // --- action handles (subaction model) ------------------------------------
    XrActionSet g_set = XR_NULL_HANDLE;
    XrAction    g_pose      = XR_NULL_HANDLE; // grip pose (both hands)
    XrAction    g_trigger   = XR_NULL_HANDLE; // float
    XrAction    g_squeeze   = XR_NULL_HANDLE; // float
    XrAction    g_stick     = XR_NULL_HANDLE; // vector2f
    XrAction    g_primary   = XR_NULL_HANDLE; // bool (A / X)
    XrAction    g_secondary = XR_NULL_HANDLE; // bool (B / Y)
    XrAction    g_stickClick = XR_NULL_HANDLE; // bool (stick press; recenter chord)
    XrAction    g_squeezeClick = XR_NULL_HANDLE; // bool (grip click; runtime-timed holds)
    XrAction    g_haptic = XR_NULL_HANDLE;       // vibration output (per hand)
    XrSpace     g_handSpace[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
    XrPath      g_handPath[2]  = { XR_NULL_PATH, XR_NULL_PATH }; // left, right
    bool        g_ready = false;

    enum Hand { LEFT = 0, RIGHT = 1 };

    // --- emulation state (to send only transitions) --------------------------
    bool   g_keyDown[8]   = {};   // WASD + use + jump (+ spare)
    bool   g_mouseBtn[2]  = {};   // 0 = LMB, 1 = RMB
    bool   g_blockHeld    = false;
    unsigned long long g_blockSince = 0;
    long long g_blockSinceXr = 0;   // runtime timestamp (ns) of the press, 0 = tick path

    // --- swing / attack-hold state -------------------------------------------
    using Melee::Sample;
    Sample g_ring[32] = {};
    int    g_ringPos  = 0;
    bool   g_swinging = false;
    unsigned long long g_lastAttackMs = 0;
    LARGE_INTEGER g_qpcFreq = {};

    bool   g_attackActive = false;          // LMB currently held by a swing
    unsigned long long g_attackStartMs = 0;

    // --- gesture state --------------------------------------------------------
    bool   g_gripPrev[2]   = {};
    bool   g_snapReady     = true;   // edge trigger for snap turn
    bool   g_grabSuppressBlock = false; // a zone-grab press shouldn't also block
    // Reaching INTO a grab zone is fast hand motion that can trip the swing
    // detector; after any zone grab/holster, suppress swing starts briefly
    // and cancel an in-progress swing so the reach is never classified as
    // an attack while the user grabs gear.
    unsigned long long g_grabRefractoryUntilMs = 0;
    long long g_gripChangeXr[2] = { 0, 0 }; // runtime press timestamps per hand
    unsigned long long g_bothGripSince = 0;
    bool   g_twoHandSent = false;
    bool   g_triggerPrev = false;
    // Laser-pointer state (menus/map only).
    bool   g_pointerActive = false;
    int    g_pointerX = 0, g_pointerY = 0;
    // Recenter chord (left stick click hold). Cancelled by right-stick motion
    // so look input never accidentally recenters; fires once per press.
    unsigned long long g_l3Since = 0;
    bool   g_l3Cancelled = false;
    bool   g_l3Fired = false;
    // Frame-time state for dt-scaled injection (file scope so Shutdown can
    // reset it; function-static would be unreachable there).
    unsigned long long s_lastFrameMs = 0;

    using Melee::AttackDir;
    using Melee::DIR_LEFT; using Melee::DIR_RIGHT;
    using Melee::DIR_OVERHEAD; using Melee::DIR_THRUST;

    // Production sink: real OS injection via SendInput.
    class SendInputSink : public IInputSink
    {
    public:
        void Key(int vk, bool down) override
        {
            INPUT in{};
            in.type = INPUT_KEYBOARD;
            in.ki.wVk = static_cast<WORD>(vk);
            in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
            SendInput(1, &in, sizeof(INPUT));
        }
        void MouseButton(bool left, bool down) override
        {
            INPUT in{};
            in.type = INPUT_MOUSE;
            in.mi.dwFlags = left ? (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP)
                                 : (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
            SendInput(1, &in, sizeof(INPUT));
        }
        void MouseMove(int dx, int dy) override
        {
            if (!dx && !dy) return;
            INPUT in{};
            in.type = INPUT_MOUSE;
            in.mi.dwFlags = MOUSEEVENTF_MOVE;
            in.mi.dx = dx;
            in.mi.dy = dy;
            SendInput(1, &in, sizeof(INPUT));
        }
        void MouseMoveAbsolute(int x, int y) override
        {
            INPUT in{};
            in.type = INPUT_MOUSE;
            in.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
            in.mi.dx = x < 0 ? 0 : (x > 65535 ? 65535 : x);
            in.mi.dy = y < 0 ? 0 : (y > 65535 ? 65535 : y);
            SendInput(1, &in, sizeof(INPUT));
        }
    };
    SendInputSink g_sendInputSink;

    void SendKey(int vk, bool down)              { InputSink::Get().Key(vk, down); }
    void SendMouseButton(bool left, bool down)   { InputSink::Get().MouseButton(left, down); }
    void SendMouseMove(int dx, int dy)           { InputSink::Get().MouseMove(dx, dy); }

    // -------------------------------------------------------------------------
    // Action reads (per-hand via subaction path)
    // -------------------------------------------------------------------------
    XrSession Session() { return (XrSession)OpenXRBridge::XrSessionHandle(); }

    bool ReadFloat(XrAction a, Hand h, float* out)
    {
        XrActionStateGetInfo gi{ XR_TYPE_ACTION_STATE_GET_INFO };
        gi.action = a;
        gi.subactionPath = g_handPath[h];
        XrActionStateFloat st{ XR_TYPE_ACTION_STATE_FLOAT };
        if (XR_FAILED(xrGetActionStateFloat(Session(), &gi, &st)) || !st.isActive)
            return false;
        *out = st.currentState;
        return true;
    }

    bool ReadBool(XrAction a, Hand h, bool* out, XrTime* changeTime = nullptr)
    {
        XrActionStateGetInfo gi{ XR_TYPE_ACTION_STATE_GET_INFO };
        gi.action = a;
        gi.subactionPath = g_handPath[h];
        XrActionStateBoolean st{ XR_TYPE_ACTION_STATE_BOOLEAN };
        if (XR_FAILED(xrGetActionStateBoolean(Session(), &gi, &st)) || !st.isActive)
            return false;
        *out = st.currentState != XR_FALSE;
        if (changeTime) *changeTime = st.lastChangeTime;
        return true;
    }

    // ms elapsed since a runtime timestamp (XrTime, ns) on the bridge's
    // predicted-display-time clock. Returns 0 when the clock is unavailable
    // or the timestamp is in the future.
    unsigned long long XrElapsedMs(long long sinceNs)
    {
        const long long now = OpenXRBridge::PredictedDisplayTime();
        if (sinceNs == 0 || now <= sinceNs) return 0;
        return (unsigned long long)(now - sinceNs) / 1000000ULL;
    }

    // One haptic pulse on the given hand (amplitude 0..1, duration ns).
    // Best-effort: haptics are optional and must never break input flow.
    void PulseHaptics(Hand h, float amplitude, long long durationNs)
    {
        if (!Config::Get().haptics || !g_haptic || amplitude <= 0.0f) return;
        XrHapticVibration vib{ XR_TYPE_HAPTIC_VIBRATION };
        vib.amplitude = amplitude < 0.0f ? 0.0f : (amplitude > 1.0f ? 1.0f : amplitude);
        vib.duration = durationNs;
        vib.frequency = XR_FREQUENCY_UNSPECIFIED;
        XrHapticActionCreateInfo haci{ XR_TYPE_HAPTIC_ACTION_CREATE_INFO };
        haci.action = g_haptic;
        haci.subactionPath = g_handPath[h];
        xrApplyHapticFeedback(Session(), &haci,
                              reinterpret_cast<const XrHapticBaseHeader*>(&vib));
    }

    bool ReadVec2(XrAction a, Hand h, float* x, float* y)
    {
        XrActionStateGetInfo gi{ XR_TYPE_ACTION_STATE_GET_INFO };
        gi.action = a;
        gi.subactionPath = g_handPath[h];
        XrActionStateVector2f st{ XR_TYPE_ACTION_STATE_VECTOR2F };
        if (XR_FAILED(xrGetActionStateVector2f(Session(), &gi, &st)) || !st.isActive)
            return false;
        *x = st.currentState.x; *y = st.currentState.y;
        return true;
    }

    // One xrLocateSpace site for the hand grip, in application (LOCAL) space
    // at predicted display time. vel==nullptr reads position only; pos and
    // vel may independently be null. Returns false when the requested data
    // isn't valid (position when pos given, linear velocity when vel given).
    bool LocateHand(Hand h, float* pos, float* vel)
    {
        if (g_handSpace[h] == XR_NULL_HANDLE) return false;
        XrSpace appSpace = (XrSpace)OpenXRBridge::XrAppSpaceHandle();
        XrTime time = (XrTime)OpenXRBridge::PredictedDisplayTime();
        if (!appSpace || !time) return false;

        XrSpaceVelocity v2{ XR_TYPE_SPACE_VELOCITY };
        XrSpaceLocation loc{ XR_TYPE_SPACE_LOCATION, vel ? &v2 : nullptr };
        if (XR_FAILED(xrLocateSpace(g_handSpace[h], appSpace, time, &loc)))
            return false;
        if (pos)
        {
            if ((loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) == 0)
                return false;
            pos[0] = loc.pose.position.x;
            pos[1] = loc.pose.position.y;
            pos[2] = loc.pose.position.z;
        }
        if (vel)
        {
            if ((v2.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) == 0)
                return false;
            vel[0] = v2.linearVelocity.x;
            vel[1] = v2.linearVelocity.y;
            vel[2] = v2.linearVelocity.z;
        }
        return true;
    }

    bool ReadHandPose(Hand h, float* pos)     { return LocateHand(h, pos, nullptr); }
    bool ReadHandVelocity(Hand h, float* vel) { return LocateHand(h, nullptr, vel); }

    // Pose + orientation in one locate (pointer needs both).
    bool ReadHandPoseFull(Hand h, float* pos, float* quat)
    {
        if (g_handSpace[h] == XR_NULL_HANDLE) return false;
        XrSpace appSpace = (XrSpace)OpenXRBridge::XrAppSpaceHandle();
        XrTime time = (XrTime)OpenXRBridge::PredictedDisplayTime();
        if (!appSpace || !time) return false;
        XrSpaceLocation loc{ XR_TYPE_SPACE_LOCATION };
        if (XR_FAILED(xrLocateSpace(g_handSpace[h], appSpace, time, &loc)))
            return false;
        if ((loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) == 0)
            return false;
        pos[0] = loc.pose.position.x;
        pos[1] = loc.pose.position.y;
        pos[2] = loc.pose.position.z;
        quat[0] = loc.pose.orientation.x;
        quat[1] = loc.pose.orientation.y;
        quat[2] = loc.pose.orientation.z;
        quat[3] = loc.pose.orientation.w;
        return true;
    }

    // Grab-zone classification lives in GestureZones.h (pure, unit-tested);
    // zones are head-relative boxes in app space (+X right, +Y up, -Z fwd).
    // VelocityToView and the swing math live in MeleeCore.h (pure, tested).

    // -------------------------------------------------------------------------
    // Attack injection
    // -------------------------------------------------------------------------

    // Begin a held attack: initial direction delta + LMB down. The hold is
    // steered per-frame in MeleeSteer() and released there.
    void BeginAttack(AttackDir dir)
    {
        int dx = 0, dy = 0;
        Melee::BeginDelta(dir, &dx, &dy);
        const char* name = "?";
        switch (dir)
        {
        case DIR_LEFT:     name = "left";     break;
        case DIR_RIGHT:    name = "right";    break;
        case DIR_OVERHEAD: name = "overhead"; break;
        case DIR_THRUST:   name = "thrust";   break;
        default: break;
        }
        LARGE_INTEGER t0; QueryPerformanceCounter(&t0);
        SendMouseMove(dx, dy);
        SendMouseButton(true, true);
        PulseHaptics(RIGHT, 0.7f, 40000000LL); // 40 ms on swing start
        LARGE_INTEGER t1; QueryPerformanceCounter(&t1);
        double us = (double)(t1.QuadPart - t0.QuadPart) * 1e6 / g_qpcFreq.QuadPart;
        LOG_INFO("[MELEE] swing -> %s (inject %.0f us)", name, us);

        g_attackActive = true;
        g_attackStartMs = GetTickCount64();
        g_mouseBtn[0] = true;
    }

    void ReleaseAttack()
    {
        if (!g_attackActive) return;
        SendMouseButton(true, false);
        g_mouseBtn[0] = false;
        g_attackActive = false;
        // Steering samples collected while this attack was held must not leak
        // into the next swing's 600 ms peak-classification window.
        memset(g_ring, 0, sizeof(g_ring));
        g_ringPos = 0;
    }

    // While an attack is held, steer the swing with live hand motion
    // (Warband steers a held attack from mouse movement). With MotionSteer=0
    // this degrades to a plain click (release next frame).
    void SteerAttack(const Config& cfg, unsigned long long now, float dt60)
    {
        if (!g_attackActive) return;

        unsigned long long held = now - g_attackStartMs;

        float v[3];
        const bool velValid = ReadHandVelocity(RIGHT, v);
        const float speed = velValid ? Melee::Speed(v[0], v[1], v[2]) : 0.0f;

        if (Melee::SteerRelease(cfg.motionSteer, held, cfg.steerMaxHoldMs,
                                velValid, speed, cfg.swingThreshold) != Melee::RELEASE_NONE)
        {
            ReleaseAttack();
            return;
        }

        float ypr[3], pos[3];
        float headYaw = 0.0f;
        if (OpenXRBridge::GetHeadPoseDelta(ypr, pos)) headYaw = ypr[0];

        float vRight = 0, vUp = 0;
        Melee::VelocityToView(v, headYaw, &vRight, &vUp);

        int dx = 0, dy = 0;
        // dt60 scales steering rate to a 60 fps reference so held-attack
        // steering feels identical at any frame rate (WSE2-fixed class).
        Melee::SteerDelta(vRight, vUp, cfg.steerScale * dt60, cfg.steerMaxDeltaPx, &dx, &dy);
        SendMouseMove(dx, dy);
    }

    // -------------------------------------------------------------------------
    // Layer 1: controller -> kb/mouse emulation
    // -------------------------------------------------------------------------
    void Emulate(const Config& cfg, float dt60)
    {
        // Left stick -> WASD
        float lx = 0, ly = 0;
        if (ReadVec2(g_stick, LEFT, &lx, &ly))
        {
            const float dz = cfg.stickDeadzone;
            const int  keys[4] = { cfg.keyForward, cfg.keyBack, cfg.keyLeft, cfg.keyRight };
            const bool want[4] = { ly > dz, ly < -dz, lx < -dz, lx > dz };
            for (int i = 0; i < 4; ++i)
                if (want[i] != g_keyDown[i]) { SendKey(keys[i], want[i]); g_keyDown[i] = want[i]; }
        }

        // Right stick -> snap turn (h2-mod-vr style comfort option) or
        // smooth mouse look, per config. Snap is edge-triggered with a
        // return-to-centre re-arm. Look deltas are scaled by frame time
        // (dt60 = dt * 60): Warband's input is frame-rate dependent (WSE2
        // fixed the same class), and the VR cadence differs from vanilla.
        const float lookScale = cfg.mouseSpeed * dt60;
        float rx = 0, ry = 0;
        if (ReadVec2(g_stick, RIGHT, &rx, &ry))
        {
            const float dz = cfg.stickDeadzone;
            if (cfg.snapTurnDegrees > 0.0f)
            {
                const float th = 0.7f;
                if (fabsf(rx) < dz) g_snapReady = true;
                else if (g_snapReady && fabsf(rx) > th)
                {
                    OpenXRBridge::SnapTurn(rx > 0 ? cfg.snapTurnDegrees : -cfg.snapTurnDegrees);
                    g_snapReady = false;
                }
                // stick Y still does smooth pitch
                if (fabsf(ry) > dz)
                    SendMouseMove(0, (int)(ry * lookScale));
            }
            else if (fabsf(rx) > dz || fabsf(ry) > dz)
            {
                SendMouseMove((int)(rx * lookScale), (int)(ry * lookScale));
            }
        }

        // Face buttons -> configurable keys
        bool primary = false, secondary = false;
        if (ReadBool(g_primary, RIGHT, &primary) && primary != g_keyDown[4])
        {
            SendKey(cfg.keyUse, primary); g_keyDown[4] = primary;   // A (right hand)
        }
        if (ReadBool(g_secondary, RIGHT, &secondary) && secondary != g_keyDown[5])
        {
            SendKey(cfg.keyJump, secondary); g_keyDown[5] = secondary; // B
        }

        // In pure emulation mode the triggers are plain mouse buttons -
        // unless the bow-draw gesture owns the right trigger.
        if (!cfg.motionMelee)
        {
            float trig = 0;
            if (!(cfg.gestureGrab && cfg.attackHoldTrigger) &&
                ReadFloat(g_trigger, RIGHT, &trig))
            {
                bool down = trig > 0.5f;
                if (down != g_mouseBtn[0]) { SendMouseButton(true, down); g_mouseBtn[0] = down; }
            }
            float sq = 0;
            if (ReadFloat(g_squeeze, RIGHT, &sq))
            {
                bool down = sq > 0.5f;
                if (down != g_mouseBtn[1]) { SendMouseButton(false, down); g_mouseBtn[1] = down; }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Layer 2: swing classification + held-attack steering
    // -------------------------------------------------------------------------
    void MeleeTick(const Config& cfg, unsigned long long now, float dt60)
    {
        SteerAttack(cfg, now, dt60);

        float v[3] = {};
        if (ReadHandVelocity(RIGHT, v))
        {
            Sample& s = g_ring[g_ringPos];
            s.vx = v[0]; s.vy = v[1]; s.vz = v[2];
            s.tMs = now;
            g_ringPos = (g_ringPos + 1) % 32;

            float speed = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            const bool cooldown = (now - g_lastAttackMs) < (unsigned long long)cfg.attackCooldownMs;

            // Suppressed while the bow-draw trigger holds LMB (LMB
            // single-owner rule) and during the post-grab refractory.
            if (!g_swinging && !cooldown && !g_attackActive && !g_triggerPrev &&
                now >= g_grabRefractoryUntilMs &&
                speed > cfg.swingThreshold)
            {
                g_swinging = true;
            }
            else if (g_swinging && speed < cfg.swingThreshold * 0.6f)
            {
                // Swing ended; classify from the peak-velocity sample in the window.
                Sample peak{};
                const bool havePeak = Melee::FindPeak(g_ring, 32, now,
                                                      (unsigned long long)cfg.swingWindowMs,
                                                      cfg.swingThreshold, &peak);
                g_swinging = false;
                if (havePeak)
                {
                    AttackDir dir = Melee::ClassifySwing(peak.vx, peak.vy, peak.vz);

                    // Near-tie assist: when two axes are close, a mostly
                    // straight trajectory (low curvature) is a thrust, not
                    // a side slash. The four-direction ceiling is unchanged.
                    if (cfg.swingCurvatureAssist && dir != Melee::DIR_NONE)
                    {
                        const float ax = fabsf(peak.vx), ay = fabsf(peak.vy), az = fabsf(peak.vz);
                        const float hi = ax > ay ? (ax > az ? ax : az) : (ay > az ? ay : az);
                        const float lo = ax < ay ? (ax < az ? ax : az) : (ay < az ? ay : az);
                        const float mid = ax + ay + az - hi - lo;
                        if (hi > 1e-3f && mid / hi > 0.8f)
                        {
                            const float curv = Melee::TrajectoryCurvature(
                                g_ring, 32, now, (unsigned long long)cfg.swingWindowMs);
                            if (curv < 0.5f && az > 0.4f * ax)
                                dir = DIR_THRUST;
                        }
                    }

                    if (dir != Melee::DIR_NONE)
                    {
                        BeginAttack(dir);
                        g_lastAttackMs = now;
                    }
                    // upward-dominant motion: no matching attack; ignore
                }
            }
        }

        // Block: right grip held (hold-time so taps don't flick it).
        // Suppressed when this grip press was a zone grab (arrow/weapon grab
        // shouldn't also raise a block).
        if (g_grabSuppressBlock) return;

        // Prefer the boolean squeeze-click action where the profile has one:
        // its lastChangeTime is the RUNTIME's timestamp of the physical
        // press, so the hold engages exactly BlockHoldTime after the press
        // even when our polling cadence dips (frame drops can't delay it).
        // Fallback (profiles with only an analog path, e.g. Oculus Touch):
        // float threshold + poll tick. 0.6 (not a hair trigger): squeeze
        // sensors report small resting values on several controllers.
        bool want = false;
        bool haveRuntime = false;
        XrTime changeTime = 0;
        bool down = false;
        if (ReadBool(g_squeezeClick, RIGHT, &down, &changeTime))
        {
            want = down;
            haveRuntime = (changeTime != 0) && (OpenXRBridge::PredictedDisplayTime() != 0);
        }
        else
        {
            float sq = 0;
            if (ReadFloat(g_squeeze, RIGHT, &sq))
                want = sq > 0.6f;
        }

        // Physical forearm block (position-based hold): left forearm raised
        // in front of the face, independent of grip state.
        bool forearmBlock = false;
        if (cfg.blockForearm)
        {
            float head[3] = {};
            if (OpenXRBridge::GetHeadPoseAbsolute(head))
            {
                float hp[3];
                if (ReadHandPose(LEFT, hp) &&
                    ClassifyZone(LEFT, hp, head) == ZONE_BLOCK_FOREARM)
                {
                    want = true;
                    forearmBlock = true;
                }
            }
        }

        if (want && !g_blockHeld)
        {
            // Start the timer on the edge. heldMs must not be measured
            // against a stale/zero timestamp, or every grip press would
            // become an instant block once the process uptime exceeds the
            // configured hold time (regression covered headlessly).
            g_blockSince = now;
            g_blockSinceXr = haveRuntime ? changeTime : 0;
            g_blockHeld = true;
        }
        else if (want && g_blockHeld && !g_mouseBtn[1])
        {
            const unsigned long long holdMs =
                (unsigned long long)(cfg.blockHoldTime * 1000.0f);
            const unsigned long long heldMs = (g_blockSinceXr != 0)
                ? XrElapsedMs(g_blockSinceXr)
                : (now - g_blockSince);
            if (heldMs >= holdMs)
            {
                SendMouseButton(false, true);  // RMB down = hold block
                g_mouseBtn[1] = true;
                PulseHaptics(forearmBlock ? LEFT : RIGHT, 0.6f, 40000000LL); // 40 ms
            }
        }
        if (!want && g_blockHeld)
        {
            g_blockHeld = false;
            if (g_mouseBtn[1])
            {
                SendMouseButton(false, false);
                g_mouseBtn[1] = false;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Layer 3: grab/stock gestures -> synthetic keys (companion mod channel)
    //
    // The DLL cannot know Warband's equipment state (closed engine), so
    // gestures are delivered as key presses that a companion module-system mod
    // maps to agent_equip_item etc. Without a companion mod, bind these keys
    // to Warband's native weapon switching. See docs/10_VR_Combat.md.
    // -------------------------------------------------------------------------
    void Gestures(const Config& cfg, unsigned long long now)
    {
        // Zone classification needs the absolute head pose, but two-handed
        // stance and bow draw do not. A transient head-pose failure must not
        // disable those body-independent controls.
        float head[3] = {};
        const bool haveHead = OpenXRBridge::GetHeadPoseAbsolute(head);

        // Grip level: prefer the boolean squeeze-click action per hand (the
        // runtime timestamps its edges; g_gripChangeXr feeds the both-grips
        // hold below). Profiles without a click path fall back to the analog
        // threshold. Same 0.6 reasoning as the block path.
        bool grip[2] = { false, false };
        for (int h = 0; h < 2; ++h)
        {
            XrTime ct = 0;
            bool down = false;
            if (ReadBool(g_squeezeClick, (Hand)h, &down, &ct))
            {
                grip[h] = down;
                if (down && !g_gripPrev[h] && ct != 0 &&
                    OpenXRBridge::PredictedDisplayTime() != 0)
                    g_gripChangeXr[h] = ct;
                else if (!down)
                    g_gripChangeXr[h] = 0;
            }
            else
            {
                float sq = 0;
                if (ReadFloat(g_squeeze, (Hand)h, &sq))
                    grip[h] = sq > 0.6f;
                g_gripChangeXr[h] = 0; // analog path: no runtime timestamp
            }
        }

        // --- zone grabs: grip PRESS while the hand is inside a zone ----------
        if (haveHead)
        {
        for (int h = 0; h < 2; ++h)
        {
            bool pressed  = grip[h] && !g_gripPrev[h];
            bool released = !grip[h] && g_gripPrev[h];
            if (released && cfg.keyHolster)
            {
                // h2-mod-vr pattern: release at a body zone = holster/stow.
                float hp[3];
                if (ReadHandPose((Hand)h, hp))
                {
                    GrabZone z = ClassifyZone(h, hp, head);
                    if (z != ZONE_NONE)
                    {
                        SendKey(cfg.keyHolster, true); SendKey(cfg.keyHolster, false);
                        LOG_INFO("[GESTURE] holster (hand=%d zone=%d) -> vk 0x%02X", h, (int)z, cfg.keyHolster);
                        g_grabRefractoryUntilMs = now + (unsigned long long)cfg.grabRefractoryMs;
                        g_swinging = false; // a reach is not an attack
                        if (h == RIGHT) g_grabSuppressBlock = true;
                    }
                }
            }
            if (pressed)
            {
                float hp[3];
                if (ReadHandPose((Hand)h, hp))
                {
                    GrabZone z = ClassifyZone(h, hp, head);
                    int vk = 0;
                    const char* zn = nullptr;
                    switch (z)
                    {
                    case ZONE_HIP_L:   vk = cfg.keyGrabHipL;    zn = "hip-left";   break;
                    case ZONE_HIP_R:   vk = cfg.keyGrabHipR;    zn = "hip-right";  break;
                    case ZONE_SHOULDER:vk = cfg.keyGrabShoulder;zn = "shoulder";   break;
                    case ZONE_QUIVER:  vk = cfg.keyGrabQuiver;  zn = "quiver";     break;
                    default: break;
                    }
                    if (vk)
                    {
                        SendKey(vk, true); SendKey(vk, false); // tap
                        LOG_INFO("[GESTURE] grab %s (hand=%d) -> vk 0x%02X", zn, h, vk);
                        g_grabRefractoryUntilMs = now + (unsigned long long)cfg.grabRefractoryMs;
                        g_swinging = false; // a reach is not an attack
                        if (h == RIGHT) g_grabSuppressBlock = true; // grab != block
                    }
                }
            }
        }
        }
        g_gripPrev[LEFT] = grip[LEFT];
        g_gripPrev[RIGHT] = grip[RIGHT];
        if (!grip[RIGHT]) g_grabSuppressBlock = false;

        // --- two-handed stance: both grips held together -> stance key ------
        // Hold measured from the LATER of the two physical presses when the
        // runtime timestamps are available; otherwise from the first poll
        // that observed both down (tick path).
        if (grip[LEFT] && grip[RIGHT])
        {
            if (g_bothGripSince == 0) g_bothGripSince = now;
            const bool runtimeTimed = g_gripChangeXr[LEFT] != 0 && g_gripChangeXr[RIGHT] != 0;
            const unsigned long long heldMs = runtimeTimed
                ? XrElapsedMs(g_gripChangeXr[LEFT] > g_gripChangeXr[RIGHT]
                              ? g_gripChangeXr[LEFT] : g_gripChangeXr[RIGHT])
                : (now - g_bothGripSince);
            if (!g_twoHandSent && heldMs > 500)
            {
                if (cfg.keyTwoHand)
                {
                    SendKey(cfg.keyTwoHand, true); SendKey(cfg.keyTwoHand, false);
                    LOG_INFO("[GESTURE] two-handed stance -> vk 0x%02X", cfg.keyTwoHand);
                }
                g_twoHandSent = true;
            }
        }
        else
        {
            g_bothGripSince = 0;
            g_twoHandSent = false;
        }

        // --- pointer click in menus (laser pointer owns the trigger) --------
        if (g_pointerActive)
        {
            float trig = 0;
            if (ReadFloat(g_trigger, RIGHT, &trig))
            {
                const bool down = trig > 0.5f;
                if (down != g_triggerPrev)
                {
                    SendMouseButton(true, down);
                    g_mouseBtn[0] = down;
                    g_triggerPrev = down;
                    LOG_INFO("[GESTURE] pointer click %s", down ? "down" : "up");
                }
            }
        }
        // --- bow draw: trigger hold = LMB hold (draw), release = loose -------
        // Lets bows/crossbows work by physically holding the draw. Melee
        // swings still use the classifier on the right hand.
        else if (cfg.attackHoldTrigger)
        {
            float trig = 0;
            if (ReadFloat(g_trigger, RIGHT, &trig))
            {
                // Deliberately higher than emulation mode's 0.5: a light
                // incidental press while gripping must not start a draw.
                bool down = trig > 0.7f;

                // Physical two-hand draw: with the trigger held, the pull
                // distance decides the draw state with hysteresis (engage
                // past 0.35 m, loose below 0.25 m). Trigger-only fallback
                // when either pose is unavailable or BowTwoHand=0.
                if (down && cfg.bowTwoHand)
                {
                    float lp[3], rp[3];
                    const bool haveBoth = ReadHandPose(LEFT, lp) && ReadHandPose(RIGHT, rp);
                    if (haveBoth)
                    {
                        const float dist = Melee::HandDistance(lp, rp);
                        if (dist > 0.35f)      down = true;
                        else if (dist < 0.25f) down = false;
                        else                   down = g_triggerPrev; // hysteresis
                    }
                }

                if (down && g_attackActive) return; // swing owns LMB; wait
                if (down != g_triggerPrev)
                {
                    SendMouseButton(true, down);
                    g_mouseBtn[0] = down;
                    g_triggerPrev = down;
                    if (!down) PulseHaptics(RIGHT, 0.5f, 30000000LL); // bow release
                    LOG_INFO("[GESTURE] bow draw %s", down ? "hold" : "release");
                }
            }
        }
    }

    // --- binding tables -------------------------------------------------------
    struct BindingMap { const char* profile; const char* posePath; const char* triggerPath;
                        const char* squeezePath; const char* stickPath;
                        const char* primaryPath; const char* secondaryPath;
                        const char* stickClickPath; const char* squeezeClickPath;
                        const char* hapticPath; };
    const BindingMap kMaps[] = {
        { "/interaction_profiles/oculus_touch_controller",
          "/input/grip/pose", "/input/trigger/value", "/input/squeeze/value", "/input/thumbstick",
          "/input/a/click", "/input/b/click", "/input/thumbstick/click",
          nullptr, "/output/haptic" },
        { "/interaction_profiles/valve_index_controller",
          "/input/grip/pose", "/input/trigger/value", "/input/squeeze/value", "/input/thumbstick",
          "/input/a/click", "/input/b/click", "/input/thumbstick/click",
          "/input/squeeze/click", "/output/haptic" },
        { "/interaction_profiles/htc_vive_controller",
          "/input/grip/pose", "/input/trigger/value", "/input/squeeze/value", "/input/trackpad",
          "/input/trackpad/click", "/input/menu/click", nullptr, // no spare click
          "/input/squeeze/click", "/output/haptic" },
        { "/interaction_profiles/microsoft_motion_controller",
          "/input/grip/pose", "/input/trigger/value", "/input/squeeze/click", "/input/thumbstick",
          "/input/menu/click", "/input/trackpad/click", "/input/thumbstick/click",
          "/input/squeeze/click", "/output/haptic" },
        { "/interaction_profiles/khr/simple_controller",
          "/input/grip/pose", "/input/select/click", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr },
    };

    XrPath StrToPath(const char* s)
    {
        XrPath p = XR_NULL_PATH;
        if (s) xrStringToPath((XrInstance)OpenXRBridge::XrInstanceHandle(), s, &p);
        return p;
    }
}

bool MotionInput::Init()
{
    if (g_ready) return true;
    InputSink::Install(&g_sendInputSink); // production injection via SendInput
    XrSession session = (XrSession)OpenXRBridge::XrSessionHandle();
    if (!session) return false;

    g_handPath[LEFT]  = StrToPath("/user/hand/left");
    g_handPath[RIGHT] = StrToPath("/user/hand/right");

    XrActionSetCreateInfo asci{ XR_TYPE_ACTION_SET_CREATE_INFO };
    strcpy_s(asci.actionSetName, "warbandvr");
    strcpy_s(asci.localizedActionSetName, "WarbandVR");
    if (XR_FAILED(xrCreateActionSet((XrInstance)OpenXRBridge::XrInstanceHandle(), &asci, &g_set)))
        return false;

    auto makeAction = [&](XrAction* out, const char* name, const char* label, XrActionType type) {
        XrActionCreateInfo aci{ XR_TYPE_ACTION_CREATE_INFO };
        strcpy_s(aci.actionName, name);
        strcpy_s(aci.localizedActionName, label);
        aci.actionType = type;
        aci.countSubactionPaths = 2;
        aci.subactionPaths = g_handPath;
        return XR_SUCCEEDED(xrCreateAction(g_set, &aci, out));
    };

    bool ok = true;
    ok &= makeAction(&g_pose,      "pose",      "Grip Pose",  XR_ACTION_TYPE_POSE_INPUT);
    ok &= makeAction(&g_trigger,   "trigger",   "Trigger",    XR_ACTION_TYPE_FLOAT_INPUT);
    ok &= makeAction(&g_squeeze,   "squeeze",   "Squeeze",    XR_ACTION_TYPE_FLOAT_INPUT);
    ok &= makeAction(&g_stick,     "stick",     "Thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT);
    ok &= makeAction(&g_primary,   "primary",   "Primary",    XR_ACTION_TYPE_BOOLEAN_INPUT);
    ok &= makeAction(&g_secondary, "secondary", "Secondary",  XR_ACTION_TYPE_BOOLEAN_INPUT);
    ok &= makeAction(&g_stickClick, "stickclick", "Stick Click", XR_ACTION_TYPE_BOOLEAN_INPUT);
    ok &= makeAction(&g_squeezeClick, "squeezeclick", "Squeeze Click", XR_ACTION_TYPE_BOOLEAN_INPUT);
    ok &= makeAction(&g_haptic, "haptic", "Haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT);
    if (!ok)
    {
        LOG_ERROR("MotionInput: action creation failed");
        return false;
    }

    // Suggested bindings per known profile. Missing component paths are
    // skipped (partial bindings are legal - unbound actions stay inactive).
    const char* sub[2] = { "/user/hand/left", "/user/hand/right" };
    for (const BindingMap& m : kMaps)
    {
        std::vector<XrActionSuggestedBinding> binds;
        auto add = [&](XrAction a, const char* comp, int hand) {
            if (!a || !comp) return;
            char path[128];
            snprintf(path, sizeof(path), "%s%s", sub[hand], comp);
            XrActionSuggestedBinding b{ a, StrToPath(path) };
            if (b.binding != XR_NULL_PATH) binds.push_back(b);
        };
        for (int h = 0; h < 2; ++h)
        {
            add(g_pose,      m.posePath,      h);
            add(g_trigger,   m.triggerPath,   h);
            add(g_squeeze,   m.squeezePath,   h);
            add(g_stick,     m.stickPath,     h);
            add(g_primary,   m.primaryPath,   h);
            add(g_secondary, m.secondaryPath, h);
            add(g_stickClick, m.stickClickPath, h);
            add(g_squeezeClick, m.squeezeClickPath, h);
            add(g_haptic, m.hapticPath, h);
        }
        if (binds.empty()) continue;
        XrInteractionProfileSuggestedBinding sb{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
        sb.interactionProfile = StrToPath(m.profile);
        sb.suggestedBindings = binds.data();
        sb.countSuggestedBindings = (uint32_t)binds.size();
        xrSuggestInteractionProfileBindings((XrInstance)OpenXRBridge::XrInstanceHandle(), &sb);
    }

    XrSessionActionSetsAttachInfo attach{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
    attach.countActionSets = 1;
    attach.actionSets = &g_set;
    if (XR_FAILED(xrAttachSessionActionSets(session, &attach)))
    {
        LOG_ERROR("MotionInput: xrAttachSessionActionSets failed");
        return false;
    }

    for (int h = 0; h < 2; ++h)
    {
        XrActionSpaceCreateInfo sci{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
        sci.action = g_pose;
        sci.poseInActionSpace.orientation.w = 1.0f;
        sci.subactionPath = g_handPath[h]; // legal: g_pose has these subactions
        if (XR_FAILED(xrCreateActionSpace(session, &sci, &g_handSpace[h])))
        {
            LOG_ERROR("MotionInput: xrCreateActionSpace hand=%d failed", h);
            return false;
        }
    }

    if (g_qpcFreq.QuadPart == 0) QueryPerformanceFrequency(&g_qpcFreq);
    g_ready = true;
    LOG_INFO("MotionInput: actions ready (emulation=%d melee=%d steer=%d)",
             Config::Get().emulateControllers ? 1 : 0,
             Config::Get().motionMelee ? 1 : 0,
             Config::Get().motionSteer ? 1 : 0);
    return true;
}

void MotionInput::Shutdown()
{
    // Release any held emulated inputs so the game isn't left with stuck keys.
    // LMB may be owned by either a steered swing or the bow-draw trigger, so
    // explicitly clear both paths rather than relying on ReleaseAttack alone.
    ReleaseAttack();
    const int keys[6] = { Config::Get().keyForward, Config::Get().keyBack,
                          Config::Get().keyLeft, Config::Get().keyRight,
                          Config::Get().keyUse, Config::Get().keyJump };
    for (int i = 0; i < 6; ++i)
        if (g_keyDown[i]) SendKey(keys[i], false);
    if (g_mouseBtn[0]) SendMouseButton(true, false);
    if (g_mouseBtn[1]) SendMouseButton(false, false);
    memset(g_keyDown, 0, sizeof(g_keyDown));
    memset(g_mouseBtn, 0, sizeof(g_mouseBtn));

    // Full lifecycle reset: Shutdown/Init must leave no stale edges, held
    // timers, ring samples, or ownership flags that can leak into a session
    // restart. (Regression-tested headlessly.)
    g_blockHeld = false;
    g_blockSince = 0;
    g_blockSinceXr = 0;
    memset(g_ring, 0, sizeof(g_ring));
    g_ringPos = 0;
    g_swinging = false;
    g_lastAttackMs = 0;
    g_attackActive = false;
    g_attackStartMs = 0;
    memset(g_gripPrev, 0, sizeof(g_gripPrev));
    g_snapReady = true;
    g_grabSuppressBlock = false;
    g_grabRefractoryUntilMs = 0;
    g_gripChangeXr[0] = g_gripChangeXr[1] = 0;
    g_bothGripSince = 0;
    g_twoHandSent = false;
    g_triggerPrev = false;
    g_l3Since = 0;
    g_l3Cancelled = false;
    g_l3Fired = false;
    s_lastFrameMs = 0; // dt state must not leak across session restarts
    g_pointerActive = false;
    g_pointerX = g_pointerY = 0;

    for (int h = 0; h < 2; ++h)
        if (g_handSpace[h]) xrDestroySpace(g_handSpace[h]);
    if (g_set) xrDestroyActionSet(g_set);
    g_set = XR_NULL_HANDLE;
    g_pose = XR_NULL_HANDLE;
    g_trigger = XR_NULL_HANDLE;
    g_squeeze = XR_NULL_HANDLE;
    g_stick = XR_NULL_HANDLE;
    g_primary = XR_NULL_HANDLE;
    g_secondary = XR_NULL_HANDLE;
    g_stickClick = XR_NULL_HANDLE;
    g_squeezeClick = XR_NULL_HANDLE;
    g_haptic = XR_NULL_HANDLE;
    memset(g_handSpace, 0, sizeof(g_handSpace));
    memset(g_handPath, 0, sizeof(g_handPath));
    g_ready = false;
}

void MotionInput::OnFrame()
{
    // Self-retry: if Init failed at bridge startup (e.g. actions not yet
    // bindable), try again periodically while the session is running.
    if (!g_ready)
    {
        static ULONGLONG s_lastRetry = 0;
        ULONGLONG now = GetTickCount64();
        if (now - s_lastRetry > 3000)
        {
            s_lastRetry = now;
            if (OpenXRBridge::SessionRunning()) Init();
        }
        if (!g_ready) return;
    }
    if (!OpenXRBridge::SessionRunning()) return;
    XrSession session = Session();

    XrActiveActionSet active{ g_set, XR_NULL_PATH };
    XrActionsSyncInfo sync{ XR_TYPE_ACTIONS_SYNC_INFO };
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &active;
    if (XR_FAILED(xrSyncActions(session, &sync))) return;

    const Config& cfg = Config::Get();
    unsigned long long nowMs = GetTickCount64();

    // Laser pointer (menus/map only): compute before any layer so click
    // handling below sees the current state. View-static is the menu
    // heuristic already used for tracking lock.
    g_pointerActive = false;
    if (cfg.pointerMenus && cfg.outputMode == 0 && cfg.enableStereo &&
        (GetTickCount64() - ViewState::g_lastViewChangeTick > 250))
    {
        float pos[3], quat[4];
        if (ReadHandPoseFull(RIGHT, pos, quat))
        {
            if (ViewState::g_hasView && ViewState::g_hasProj)
            {
                const int eyeW = (int)ViewState::g_eyeW;
                const int eyeH = (int)ViewState::g_eyeH;
                int lx = 0, ly = 0;
                if (PointerMath::ComputePointerLogical(pos, quat,
                                                       MatFromD3D(ViewState::g_view),
                                                       MatFromD3D(ViewState::g_proj),
                                                       eyeW, eyeH, &lx, &ly)
                    && eyeW > 0 && eyeH > 0)
                {
                    g_pointerActive = true;
                    g_pointerX = lx;
                    g_pointerY = ly;
                    InputSink::Get().MouseMoveAbsolute(lx * 65535 / eyeW,
                                                       ly * 65535 / eyeH);
                }
            }
        }
    }

    // Frame-time factor for all rate-based injection (dt * 60, clamped).
    float dt = 1.0f / 60.0f;
    if (s_lastFrameMs != 0)
    {
        const unsigned long long deltaMs = nowMs - s_lastFrameMs;
        dt = deltaMs / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;          // hitch guard: cap a long frame
        if (dt < 1.0f / 240.0f) dt = 1.0f / 240.0f;
    }
    s_lastFrameMs = nowMs;
    const float dt60 = dt * 60.0f;
    if (cfg.emulateControllers) Emulate(cfg, dt60);
    if (cfg.motionMelee)        MeleeTick(cfg, nowMs, dt60);
    if (cfg.gestureGrab)        Gestures(cfg, nowMs);

    // Recenter chord (BannerlordVR lesson): no keyboard in a headset, so
    // holding the left stick press for RecenterHoldSeconds with the right
    // stick centered recenters the view. Right-stick motion cancels that
    // press so look input never accidentally recenters. System-level: runs
    // regardless of which emulation/melee/gesture layers are enabled. 0
    // disables; profiles without a stick-click path (Vive wand) leave the
    // action unbound and the chord inert.
    if (cfg.recenterHoldSeconds > 0.0f)
    {
        bool l3 = false;
        XrTime l3Change = 0;
        ReadBool(g_stickClick, LEFT, &l3, &l3Change);
        if (l3 && g_l3Since == 0)
        {
            g_l3Since = nowMs;
            g_l3Cancelled = false;
            g_l3Fired = false;
        }
        else if (!l3)
            g_l3Since = 0;

        if (l3 && g_l3Since != 0 && !g_l3Fired && !g_l3Cancelled)
        {
            float rx = 0, ry = 0;
            if (ReadVec2(g_stick, RIGHT, &rx, &ry) &&
                (fabsf(rx) > cfg.stickDeadzone || fabsf(ry) > cfg.stickDeadzone))
                g_l3Cancelled = true;
            else
            {
                // Prefer the runtime's edge timestamp: the hold completes
                // RecenterHoldSeconds after the physical press even if a
                // frame dip delayed the first poll that saw it.
                const unsigned long long holdMs =
                    (unsigned long long)(cfg.recenterHoldSeconds * 1000.0f);
                const unsigned long long heldMs =
                    (l3Change != 0 && OpenXRBridge::PredictedDisplayTime() != 0)
                        ? XrElapsedMs(l3Change)
                        : (nowMs - g_l3Since);
                if (heldMs >= holdMs)
                {
                    OpenXRBridge::Recenter();
                    g_l3Fired = true;
                    LOG_INFO("[GESTURE] recenter (stick-click hold)");
                }
            }
        }
    }
}

bool MotionInput::GetPointerLogical(int* x, int* y)
{
    if (!g_pointerActive) return false;
    if (x) *x = g_pointerX;
    if (y) *y = g_pointerY;
    return true;
}

#else
// --- stubs (non-OpenXR build) ------------------------------------------------
bool MotionInput::Init() { return false; }
void MotionInput::Shutdown() {}
void MotionInput::OnFrame() {}
bool MotionInput::GetPointerLogical(int*, int*) { return false; }
#endif
