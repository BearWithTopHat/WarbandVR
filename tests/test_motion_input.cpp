// SPDX-License-Identifier: GPL-3.0-only
// Headless OpenXR action-space tests: real MotionInput compiled with
// WBVR_WITH_OPENXR, driven by a scriptable XR fake and recording input sink.

#include "windows.h"
#include "d3d9.h"
#include "Config.h"
#include "InputSink.h"
#include "MotionInput.h"
#include "OpenXRBridge.h"
#include "xr_fake.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_failures = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { \
    ++g_failures; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

namespace
{
    struct Recorder : public IInputSink
    {
        std::vector<std::string> events;
        void Key(int vk, bool down) override
        {
            char b[64]; snprintf(b, sizeof(b), "K%d:%d", vk, down ? 1 : 0); events.push_back(b);
        }
        void MouseButton(bool left, bool down) override
        {
            char b[64]; snprintf(b, sizeof(b), "B%c:%d", left ? 'L' : 'R', down ? 1 : 0); events.push_back(b);
        }
        void MouseMove(int dx, int dy) override
        {
            char b[64]; snprintf(b, sizeof(b), "M%d,%d", dx, dy); events.push_back(b);
        }
        int Count(const char* exact) const
        {
            int n = 0; for (const std::string& e : events) if (e == exact) ++n; return n;
        }
        bool Has(const char* exact) const { return Count(exact) > 0; }
        bool LastIs(const char* exact) const { return !events.empty() && events.back() == exact; }
        void Clear() { events.clear(); }
    } g_rec;

    bool g_sessionRunning = true;
    bool g_haveHeadDelta = false;
    float g_headYpr[3] = {};
    bool g_haveHeadAbs = false;
    float g_headAbs[3] = {};
    std::vector<float> g_snapTurns;
    int g_recenterCount = 0;

    void ResetBridge()
    {
        g_sessionRunning = true;
        g_haveHeadDelta = false;
        g_headYpr[0] = g_headYpr[1] = g_headYpr[2] = 0;
        g_haveHeadAbs = false;
        g_headAbs[0] = g_headAbs[1] = g_headAbs[2] = 0;
        g_snapTurns.clear();
        g_recenterCount = 0;
    }

    void ConfigureDefaults()
    {
        Config& c = Config::Get();
        c.emulateControllers = true;
        c.motionMelee = false;
        c.gestureGrab = false;
        c.attackHoldTrigger = true;
        c.stickDeadzone = 0.2f;
        c.mouseSpeed = 6.0f;
        c.snapTurnDegrees = 0.0f;
    c.recenterHoldSeconds = 3.0f;
        c.swingThreshold = 1.8f;
        c.attackCooldownMs = 400;
        c.motionSteer = true;
        c.steerScale = 40.0f;
        c.steerMaxHoldMs = 600;
        c.blockHoldTime = 0.15f;
        c.keyForward = 'W'; c.keyBack = 'S'; c.keyLeft = 'A'; c.keyRight = 'D';
        c.keyUse = 'F'; c.keyJump = 0x20;
        c.keyGrabHipL = 0x70; c.keyGrabHipR = 0x71; c.keyGrabShoulder = 0x72;
        c.keyGrabQuiver = 0x73; c.keyTwoHand = 0x74; c.keyHolster = 0x75;
    }

    void Restart()
    {
        MotionInput::Shutdown();
        XrFake::Reset();
        ResetBridge();
        ConfigureDefaults();
        WinShimSetTickCount64(1000);
        CHECK(MotionInput::Init());
        InputSink::Install(&g_rec);
        g_rec.Clear();
    }

    void Frame(unsigned long long t)
    {
        WinShimSetTickCount64(t);
        MotionInput::OnFrame();
    }

    bool LastTwoAre(const char* a, const char* b)
    {
        size_t n = g_rec.events.size();
        return n >= 2 && g_rec.events[n - 2] == a && g_rec.events[n - 1] == b;
    }
}

// Fake bridge endpoint (OpenXRBridge.cpp intentionally not linked here).
namespace OpenXRBridge
{
    bool Init(IDirect3DDevice9*) { return true; }
    void SubmitFrame(IDirect3DDevice9*, IDirect3DSurface9*, IDirect3DSurface9*) {}
    void Shutdown() {}
    bool GetHeadPoseDelta(float* ypr, float* xyz)
    {
        if (!g_haveHeadDelta) return false;
        memcpy(ypr, g_headYpr, sizeof(g_headYpr));
        xyz[0] = xyz[1] = xyz[2] = 0;
        return true;
    }
    void Recenter() { ++g_recenterCount; }
    void SnapTurn(float degrees) { g_snapTurns.push_back(degrees); }
    bool GetHeadPoseAbsolute(float* outXYZ)
    {
        if (!g_haveHeadAbs) return false;
        memcpy(outXYZ, g_headAbs, sizeof(g_headAbs));
        return true;
    }
    bool SessionRunning() { return g_sessionRunning; }
    void* XrInstanceHandle() { return (void*)0x11; }
    void* XrSessionHandle() { return g_sessionRunning ? (void*)0x22 : nullptr; }
    void* XrAppSpaceHandle() { return (void*)0x33; }
    // Runtime clock in XrTime (ns), driven by the same deterministic shim
    // clock the tests set: lets runtime-timestamp hold timing be asserted at
    // exact millisecond boundaries.
    long long PredictedDisplayTime() { return (long long)GetTickCount64() * 1000000LL; }
}

int main()
{
    // --- action-set construction uses the subaction model -------------------
    Restart();
    CHECK(XrFake::HasAction("pose"));
    CHECK(XrFake::HasAction("trigger"));
    CHECK(XrFake::HasAction("squeeze"));
    CHECK(XrFake::HasAction("stick"));
    CHECK(XrFake::HasAction("primary"));
    CHECK(XrFake::HasAction("secondary"));
    CHECK(XrFake::HasAction("stickclick"));
    CHECK(XrFake::HasAction("squeezeclick"));
    CHECK(XrFake::ActionsCreated() == "pose,trigger,squeeze,stick,primary,secondary,stickclick,squeezeclick");
    CHECK(XrFake::AttachedSets() == 1);
    CHECK(XrFake::SuggestedBindings() > 0);

    // --- left stick -> WASD, transitions only -------------------------------
    XrFake::SetVec2("stick", XrFake::Left, 0.0f, 0.8f);
    Frame(1010);
    CHECK(g_rec.Has("K87:1"));       // W down
    CHECK(g_rec.Count("K87:1") == 1);
    XrFake::SetVec2("stick", XrFake::Left, 0.0f, -0.8f);
    Frame(1020);
    CHECK(g_rec.Has("K87:0"));
    CHECK(g_rec.Has("K83:1"));       // S down
    XrFake::SetVec2("stick", XrFake::Left, 0.0f, 0.0f);
    Frame(1030);
    CHECK(g_rec.Has("K83:0"));

    // --- right stick smooth look and face buttons ---------------------------
    XrFake::SetVec2("stick", XrFake::Right, 0.5f, 0.0f);
    Frame(1040);
    CHECK(g_rec.Has("M1,0")); // 0.5 * 6 * 0.6 = 1.8 -> 1
    XrFake::SetVec2("stick", XrFake::Right, 0.0f, 0.0f);
    XrFake::SetBool("primary", XrFake::Right, true);
    Frame(1050);
    CHECK(g_rec.Has("K70:1"));       // F
    XrFake::SetBool("primary", XrFake::Right, false);
    XrFake::SetBool("secondary", XrFake::Right, true);
    Frame(1060);
    CHECK(g_rec.Has("K70:0"));
    CHECK(g_rec.Has("K32:1"));       // SPACE
    XrFake::SetBool("secondary", XrFake::Right, false);
    Frame(1070);
    CHECK(g_rec.Has("K32:0"));

    // --- pure emulation triggers/squeeze -> mouse buttons -------------------
    XrFake::SetFloat("trigger", XrFake::Right, 1.0f);
    XrFake::SetFloat("squeeze", XrFake::Right, 1.0f);
    Frame(1080);
    CHECK(g_rec.Has("BL:1"));
    CHECK(g_rec.Has("BR:1"));
    XrFake::SetFloat("trigger", XrFake::Right, 0.0f);
    XrFake::SetFloat("squeeze", XrFake::Right, 0.0f);
    Frame(1090);
    CHECK(g_rec.Has("BL:0"));
    CHECK(g_rec.Has("BR:0"));

    // --- snap turn: edge-triggered with centre re-arm -----------------------
    Restart();
    Config::Get().snapTurnDegrees = 45.0f;
    XrFake::SetVec2("stick", XrFake::Right, 1.0f, 0.0f);
    Frame(1010);
    Frame(1020);
    CHECK(g_snapTurns.size() == 1 && g_snapTurns[0] == 45.0f);
    XrFake::SetVec2("stick", XrFake::Right, 0.0f, 0.0f);
    Frame(1030);
    XrFake::SetVec2("stick", XrFake::Right, -1.0f, 0.0f);
    Frame(1040);
    CHECK(g_snapTurns.size() == 2 && g_snapTurns[1] == -45.0f);

    // --- recenter chord: left stick-click hold, right-stick motion cancels --
    Restart();
    XrFake::SetBool("stickclick", XrFake::Left, true);
    Frame(1000);
    Frame(3999);                          // 2999 ms held < 3000 ms threshold
    CHECK(g_recenterCount == 0);
    Frame(4000);
    CHECK(g_recenterCount == 1);          // fired once at the threshold
    Frame(5000);
    CHECK(g_recenterCount == 1);          // no repeat while still held
    XrFake::SetBool("stickclick", XrFake::Left, false);
    Frame(5010);
    XrFake::SetBool("stickclick", XrFake::Left, true);
    XrFake::SetVec2("stick", XrFake::Right, 1.0f, 0.0f);
    Frame(5020);
    Frame(8100);                          // well past the threshold
    CHECK(g_recenterCount == 1);          // right-stick motion cancelled it
    XrFake::SetVec2("stick", XrFake::Right, 0.0f, 0.0f);
    XrFake::SetBool("stickclick", XrFake::Left, false);
    Frame(8110);
    XrFake::SetBool("stickclick", XrFake::Left, true);
    Frame(8120);
    Frame(11120);                         // fresh press, stick centered
    CHECK(g_recenterCount == 2);          // fires again

    // --- recenter chord timing uses the runtime edge timestamp -------------
    Restart();
    WinShimSetTickCount64(1005);
    XrFake::SetBool("stickclick", XrFake::Left, true); // pressed at 1005 ms
    Frame(1010);                          // first poll observing the press
    Frame(4004);                          // runtime says 2999 ms held
    CHECK(g_recenterCount == 0);
    Frame(4005);                          // runtime says 3000 ms held: fire
    CHECK(g_recenterCount == 1);

    // --- both-grips stance: hold timed from the LATER physical press -------
    Restart();
    Config::Get().motionMelee = true; Config::Get().gestureGrab = true;
    g_haveHeadAbs = true; g_headAbs[0] = 0; g_headAbs[1] = 1.6f; g_headAbs[2] = 0;
    XrFake::SetPose(XrFake::Left, 0.0f, 0.0f, 0.0f, true);
    XrFake::SetPose(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    WinShimSetTickCount64(1000);
    XrFake::SetBool("squeezeclick", XrFake::Left, true);   // left at 1000
    WinShimSetTickCount64(1200);
    XrFake::SetBool("squeezeclick", XrFake::Right, true);  // right at 1200
    Frame(1210);
    Frame(1700);                          // 500 ms after the later press
    CHECK(!g_rec.Has("K116:1"));          // strict: fires only past 500 ms
    Frame(1701);
    CHECK(g_rec.Has("K116:1") && g_rec.Has("K116:0"));

    // --- swing -> right attack, then per-frame steer and stop-release -------
    Restart();
    Config& c = Config::Get();
    c.motionMelee = true;
    c.gestureGrab = true;
    XrFake::SetVelocity(XrFake::Right, 2.0f, 0.0f, 0.0f, true);
    Frame(1000);
    CHECK(!g_rec.Has("BL:1"));
    XrFake::SetVelocity(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    Frame(1010);
    CHECK(g_rec.Has("M80,0"));
    CHECK(g_rec.Has("BL:1"));
    XrFake::SetVelocity(XrFake::Right, 2.0f, 0.0f, 0.0f, true);
    Frame(1020);
    CHECK(g_rec.Count("M80,0") == 1); // initial direction delta
    CHECK(g_rec.Count("M47,0") == 1); // dt=10ms: 2*40*0.6 = 48.0f -> 47 (truncation)
    XrFake::SetVelocity(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    Frame(1141);                     // hand stopped, minimum hold elapsed
    CHECK(g_rec.Has("BL:0"));

    // overhead and thrust classification through the same real path
    XrFake::SetVelocity(XrFake::Right, 0.0f, -2.0f, 0.0f, true);
    Frame(1500);
    XrFake::SetVelocity(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    Frame(1510);
    CHECK(g_rec.Has("M0,-80"));
    Frame(1631);
    CHECK(g_rec.Count("BL:0") >= 2);
    XrFake::SetVelocity(XrFake::Right, 0.0f, 0.0f, -2.0f, true);
    Frame(2000);
    XrFake::SetVelocity(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    Frame(2010);
    CHECK(g_rec.Has("M0,80"));
    Frame(2131);
    CHECK(g_rec.Count("BL:0") >= 3);

    // --- dt-scaled look: same px/sec at any frame rate ----------------------
    Restart();
    XrFake::SetVec2("stick", XrFake::Right, 1.0f, 0.0f);
    Frame(1000);                       // first frame: dt defaults to 1/60
    Frame(1016);                       // 16 ms: dt60 ~= 1.0 -> 6 px
    CHECK(g_rec.Has("M6,0"));
    g_rec.Clear();
    Frame(1023);                       // 7 ms: dt60 = 0.42 -> 2 px (int)
    CHECK(g_rec.Has("M2,0"));
    g_rec.Clear();
    Frame(1056);                       // 33 ms: dt60 = 1.98 -> 11 px (int)
    CHECK(g_rec.Has("M11,0"));

    // --- dt-scaled held-attack steering ------------------------------------
    Restart();
    Config::Get().motionMelee = true;
    XrFake::SetVelocity(XrFake::Right, 2.0f, 0.0f, 0.0f, true);
    Frame(1000);
    XrFake::SetVelocity(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    Frame(1010);
    CHECK(g_rec.Has("BL:1"));
    XrFake::SetVelocity(XrFake::Right, 2.0f, 0.0f, 0.0f, true);
    Frame(1020);                       // dt = 10 ms: raw = 2*40*0.6 = 48
    CHECK(g_rec.Has("M47,0")); // dt=10ms: 2*40*0.6 = 48.0f -> 47 (truncation)
    XrFake::SetVelocity(XrFake::Right, 2.0f, 0.0f, 0.0f, true);
    Frame(1027);                       // dt = 7 ms: raw = 2*40*0.42 = 33 (not clamped)
    CHECK(g_rec.Has("M33,0"));

    // --- grip block hold-time ----------------------------------------------
    Restart();
    Config::Get().motionMelee = true;
    g_haveHeadAbs = true; g_headAbs[0] = 0; g_headAbs[1] = 1.6f; g_headAbs[2] = 0;
    XrFake::SetPose(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    XrFake::SetFloat("squeeze", XrFake::Right, 1.0f);
    Frame(1000);
    CHECK(!g_rec.Has("BR:1"));
    Frame(1149);
    CHECK(!g_rec.Has("BR:1"));
    Frame(1150);
    CHECK(g_rec.Has("BR:1"));
    XrFake::SetFloat("squeeze", XrFake::Right, 0.0f);
    Frame(1160);
    CHECK(g_rec.Has("BR:0"));

    // --- block via squeeze click: hold timed by the RUNTIME timestamp ------
    // Press between polls: the engage time is BlockHoldTime after the
    // physical press, not after the first poll that observed it.
    Restart();
    Config::Get().motionMelee = true;
    WinShimSetTickCount64(1005);
    XrFake::SetBool("squeezeclick", XrFake::Right, true); // pressed at 1005 ms
    Frame(1010);                     // first poll observing the press
    Frame(1154);                     // runtime says 149 ms held
    CHECK(!g_rec.Has("BR:1"));
    Frame(1155);                     // runtime says 150 ms held: engage
    CHECK(g_rec.Has("BR:1"));
    XrFake::SetBool("squeezeclick", XrFake::Right, false);
    Frame(1160);
    CHECK(g_rec.Has("BR:0"));
    // analog fallback unchanged: no click action state -> float + tick path
    Restart();
    Config::Get().motionMelee = true;
    g_haveHeadAbs = true; g_headAbs[0] = 0; g_headAbs[1] = 1.6f; g_headAbs[2] = 0;
    XrFake::SetPose(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    XrFake::SetFloat("squeeze", XrFake::Right, 1.0f);
    Frame(1000);
    CHECK(!g_rec.Has("BR:1"));
    Frame(1150);
    CHECK(g_rec.Has("BR:1"));

    // --- grab zones -> companion-mod virtual-slot keys ----------------------
    Restart();
    Config::Get().motionMelee = true;
    Config::Get().gestureGrab = true;
    g_haveHeadAbs = true; g_headAbs[0] = 0; g_headAbs[1] = 1.6f; g_headAbs[2] = 0;
    XrFake::SetPose(XrFake::Right, 0.2f, 1.3f, 0.0f, true); // right hip
    XrFake::SetFloat("squeeze", XrFake::Right, 1.0f);
    Frame(1000);
    CHECK(g_rec.Has("K113:1") && g_rec.Has("K113:0")); // F2
    XrFake::SetFloat("squeeze", XrFake::Right, 0.0f);
    Frame(1010);
    CHECK(g_rec.Has("K117:1") && g_rec.Has("K117:0")); // F6 holster

    Restart();
    Config::Get().motionMelee = true; Config::Get().gestureGrab = true;
    g_haveHeadAbs = true; g_headAbs[0] = 0; g_headAbs[1] = 1.6f; g_headAbs[2] = 0;
    XrFake::SetPose(XrFake::Right, 0.45f, 1.4f, 0.2f, true); // quiver
    XrFake::SetFloat("squeeze", XrFake::Right, 1.0f);
    Frame(1000);
    CHECK(g_rec.Has("K115:1") && g_rec.Has("K115:0")); // F4

    // --- post-grab refractory: a fast reach must not classify as a swing ----
    Restart();
    Config::Get().motionMelee = true; Config::Get().gestureGrab = true;
    g_haveHeadAbs = true; g_headAbs[0] = 0; g_headAbs[1] = 1.6f; g_headAbs[2] = 0;
    XrFake::SetPose(XrFake::Right, 0.2f, 1.3f, 0.0f, true); // right hip
    XrFake::SetFloat("squeeze", XrFake::Right, 1.0f);
    Frame(1000);
    CHECK(g_rec.Has("K113:1"));                      // grab fired (F2)
    XrFake::SetVelocity(XrFake::Right, 2.0f, 0.0f, 0.0f, true);
    Frame(1010);
    XrFake::SetVelocity(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    Frame(1020);
    CHECK(!g_rec.Has("BL:1"));                       // no swing in the refractory
    XrFake::SetVelocity(XrFake::Right, 2.0f, 0.0f, 0.0f, true);
    Frame(1410);                                     // refractory (400 ms) over
    XrFake::SetVelocity(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    Frame(1420);
    CHECK(g_rec.Has("BL:1"));                        // swing detection works again

    Restart();
    Config::Get().motionMelee = true; Config::Get().gestureGrab = true;
    g_haveHeadAbs = true; g_headAbs[0] = 0; g_headAbs[1] = 1.6f; g_headAbs[2] = 0;
    XrFake::SetPose(XrFake::Left, -0.2f, 1.3f, 0.0f, true);
    XrFake::SetFloat("squeeze", XrFake::Left, 1.0f);
    Frame(1000);
    CHECK(g_rec.Has("K112:1") && g_rec.Has("K112:0")); // F1

    Restart();
    Config::Get().motionMelee = true; Config::Get().gestureGrab = true;
    g_haveHeadAbs = true; g_headAbs[0] = 0; g_headAbs[1] = 1.6f; g_headAbs[2] = 0;
    XrFake::SetPose(XrFake::Left, 0.0f, 0.0f, 0.0f, true);
    XrFake::SetPose(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    XrFake::SetFloat("squeeze", XrFake::Left, 1.0f);
    XrFake::SetFloat("squeeze", XrFake::Right, 1.0f);
    Frame(1000);
    CHECK(!g_rec.Has("K116:1"));
    Frame(1601);
    CHECK(g_rec.Has("K116:1") && g_rec.Has("K116:0")); // F5 two-hand stance

    // --- bow draw owns LMB; swing path cannot double-own it -----------------
    Restart();
    Config::Get().motionMelee = true;
    Config::Get().gestureGrab = true;
    Config::Get().attackHoldTrigger = true;
    XrFake::SetVelocity(XrFake::Right, 2.0f, 0.0f, 0.0f, true);
    Frame(1000);
    XrFake::SetVelocity(XrFake::Right, 0.0f, 0.0f, 0.0f, true);
    Frame(1010);
    CHECK(g_rec.Count("BL:1") == 1);
    XrFake::SetFloat("trigger", XrFake::Right, 1.0f);
    Frame(1020);
    CHECK(g_rec.Count("BL:1") == 1); // trigger waits while swing owns LMB
    Frame(1201);                     // swing releases; bow draw may now own LMB
    CHECK(g_rec.Count("BL:1") == 2);
    CHECK(LastTwoAre("BL:0", "BL:1"));
    XrFake::SetFloat("trigger", XrFake::Right, 0.0f);
    Frame(1210);
    CHECK(g_rec.LastIs("BL:0"));

    // --- Shutdown releases every held input and destroys XR objects ---------
    Restart();
    XrFake::SetVec2("stick", XrFake::Left, 0.0f, 1.0f);
    XrFake::SetFloat("trigger", XrFake::Right, 1.0f);
    XrFake::SetFloat("squeeze", XrFake::Right, 1.0f);
    Frame(1000);
    g_rec.Clear();
    MotionInput::Shutdown();
    CHECK(g_rec.Has("K87:0"));
    CHECK(g_rec.Has("BL:0"));
    CHECK(g_rec.Has("BR:0"));
    CHECK(XrFake::DestroyedSpaces() == 2);
    CHECK(XrFake::DestroyedActionSets() == 1);

    // --- XR failures are non-fatal and produce no injected input ------------
    Restart();
    XrFake::SetSyncResult(-10);
    XrFake::SetVec2("stick", XrFake::Left, 0.0f, 1.0f);
    Frame(1000);
    CHECK(XrFake::SyncCount() == 1);
    CHECK(g_rec.events.empty());

    MotionInput::Shutdown();
    printf("motion-input: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
