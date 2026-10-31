// SPDX-License-Identifier: GPL-3.0-only
// WarbandVR headless unit tests - pure logic only (no Windows/D3D9/XR).
// Build:  g++ -std=c++17 -Wall -Wextra -I../src test_main.cpp -o run_tests
// Run:    ./run_tests   (exit 0 = all pass)
//
// Math is checked against hand-computed values, not snapshots.

#include "Mat4.h"
#include "DupGate.h"
#include "MeleeCore.h"
#include "GestureZones.h"
#include "ParseUtil.h"
#include "OneEuroFilter.h"
#include "Comfort.h"
#include "MouseMap.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <limits>

static int g_failures = 0;
static int g_checks   = 0;

#define CHECK(cond) do { ++g_checks; if (!(cond)) { \
    ++g_failures; \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

// ---------------------------------------------------------------------------
// Eye math (MatPure)
// ---------------------------------------------------------------------------

// Standard D3D row-major perspective projection, n=0.1 f=100 fov~45 aspect=1.
static Mat4 TestProj()
{
    Mat4 p = {};
    const float n = 0.1f, f = 100.0f;
    p._11 = 2.41421f; p._22 = 2.41421f;
    p._33 = f / (f - n);   // 1.001001...
    p._34 = 1.0f;
    p._43 = -n * p._33;    // -0.100100...
    return p;
}

static void TestEyeView()
{
    Mat4 id = MatPure::Identity();
    // view_eye = T(+/-halfSep) * view_head; left eye (0) shifts +halfSep in x.
    Mat4 L = MatPure::EyeView(id, 0.032f, 0);
    Mat4 R = MatPure::EyeView(id, 0.032f, 1);
    CHECK(Near(L._41, +0.032f) && Near(L._42, 0.0f) && Near(L._43, 0.0f));
    CHECK(Near(R._41, -0.032f));
    // halfSep=0 degenerate: identity in, identity out
    Mat4 Z = MatPure::EyeView(id, 0.0f, 0);
    CHECK(memcmp(&Z, &id, sizeof(Mat4)) == 0);
    // Composes with a translated head view: T(s) * T(t) = T(s + t)
    Mat4 hv = MatPure::Translation(1.0f, 2.0f, 3.0f);
    Mat4 e = MatPure::EyeView(hv, 0.032f, 0);
    CHECK(Near(e._41, 0.032f + 1.0f) && Near(e._42, 2.0f) && Near(e._43, 3.0f));
}

static void TestIsPlausibleTransform()
{
    // Valid transforms must pass: identity view, perspective-flavoured
    // viewproj, and a small-scale orthographic UI matrix.
    CHECK(MatPure::IsPlausibleTransform(MatPure::Identity()));
    Mat4 vp = MatPure::Multiply(MatPure::Translation(3.0f, -2.0f, 5000.0f), TestProj());
    CHECK(MatPure::IsPlausibleTransform(vp));
    Mat4 ui = {};
    ui._11 = 2.0f / 1024.0f; ui._22 = -2.0f / 768.0f; ui._33 = 0.001f;
    ui._41 = -1.0f; ui._42 = 1.0f; ui._44 = 1.0f;
    CHECK(MatPure::IsPlausibleTransform(ui));

    // Garbage from a wrong register guess must fail.
    Mat4 zero = {};
    CHECK(!MatPure::IsPlausibleTransform(zero));           // all-zero register
    Mat4 nan = MatPure::Identity();
    nan._11 = std::numeric_limits<float>::quiet_NaN();
    CHECK(!MatPure::IsPlausibleTransform(nan));
    Mat4 inf = MatPure::Identity();
    inf._42 = std::numeric_limits<float>::infinity();
    CHECK(!MatPure::IsPlausibleTransform(inf));
    Mat4 huge = MatPure::Identity();
    huge._11 = 1.0e30f;
    CHECK(!MatPure::IsPlausibleTransform(huge));           // absurd magnitude
    Mat4 tiny = {};
    tiny._11 = 1.0e-9f;                                    // below noise floor
    CHECK(!MatPure::IsPlausibleTransform(tiny));
}

static void TestIsFlatOrtho()
{
    const float zMax = 0.5f, wEps = 0.01f, rnMax = 0.5f;
    // Ortho UI matrix: tiny z-scale, w == 1, tiny row norms.
    Mat4 ui = {};
    ui._11 = 2.0f / 1024.0f; ui._22 = -2.0f / 768.0f; ui._33 = 0.001f;
    ui._41 = -1.0f; ui._42 = 1.0f; ui._44 = 1.0f;
    CHECK(MatPure::IsFlatOrtho(ui, zMax, wEps, rnMax));
    // Identity (z-scale 1) is a 3D view, not UI.
    CHECK(!MatPure::IsFlatOrtho(MatPure::Identity(), zMax, wEps, rnMax));
    // viewproj products have w-row != affine.
    Mat4 vp = MatPure::Multiply(MatPure::Translation(1.0f, 2.0f, 3.0f), TestProj());
    CHECK(!MatPure::IsFlatOrtho(vp, zMax, wEps, rnMax));
    // Thresholds are parameters: a borderline z-scale flips with the cutoff.
    // (Row norms stay tiny so only the z cutoff decides.)
    Mat4 border = {};
    border._33 = 0.4f; border._44 = 1.0f;
    CHECK(MatPure::IsFlatOrtho(border, 0.5f, wEps, rnMax));
    CHECK(!MatPure::IsFlatOrtho(border, 0.3f, wEps, rnMax));
    // Boundary: exactly at the cutoff is NOT flat (strict <).
    Mat4 edge = {};
    edge._33 = 0.5f; edge._44 = 1.0f;
    CHECK(!MatPure::IsFlatOrtho(edge, 0.5f, wEps, rnMax));
    // Steep-pitch 3D view: cos(80 deg) shrinks _33 below the z cutoff, but
    // the orthonormal rows expose it as a rotation - it must stay stereo.
    Mat4 steep = MatPure::RotationYPR(0.0f, 80.0f * 3.14159265f / 180.0f, 0.0f);
    CHECK(steep._33 < 0.5f); // would trip the old z-only rule
    CHECK(!MatPure::IsFlatOrtho(steep, zMax, wEps, rnMax));
    // Row-norm parameter is honoured: raising it past 1 re-admits rotations.
    CHECK(MatPure::IsFlatOrtho(steep, zMax, wEps, 2.0f));
}

// Deterministic property fuzz for the two classifiers: constructed-valid
// matrices must be accepted, constructed-garbage must be rejected, and
// rotations must never classify as flat UI regardless of orientation.
static void TestClassifierFuzz()
{
    uint32_t s = 0xC1A551F1u;
    auto next = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; };
    auto unit = [&]() { return (next() % 2000000u) / 1000000.0f - 1.0f; }; // [-1,1)

    for (int i = 0; i < 2000; ++i)
    {
        switch (next() % 4)
        {
        case 0: // random 3D view: rotation + translation
        {
            Mat4 v = MatPure::Multiply(
                MatPure::RotationYPR(unit() * 3.14159f, unit() * 1.55f, unit() * 3.14159f),
                MatPure::Translation(unit() * 500.0f, unit() * 50.0f, unit() * 500.0f));
            CHECK(MatPure::IsPlausibleTransform(v));
            CHECK(!MatPure::IsFlatOrtho(v, 0.5f, 0.01f, 0.5f));
            break;
        }
        case 1: // pixel-space ortho UI at a random resolution
        {
            float w = 64.0f + (next() % 8129), h = 64.0f + (next() % 8129);
            Mat4 ui = {};
            ui._11 = 2.0f / w; ui._22 = (next() & 1 ? -2.0f : 2.0f) / h;
            ui._33 = 1.0f / (3.0f + next() % 10000); // < zScaleMax by construction
            ui._41 = -1.0f; ui._42 = 1.0f; ui._44 = 1.0f;
            CHECK(MatPure::IsPlausibleTransform(ui));
            CHECK(MatPure::IsFlatOrtho(ui, 0.5f, 0.01f, 0.5f));
            break;
        }
        case 2: // garbage register contents
        {
            Mat4 g = {};
            float* f = &g._11;
            switch (next() % 4)
            {
            case 0: break; // all zero
            case 1: f[next() % 16] = std::numeric_limits<float>::quiet_NaN(); break;
            case 2: f[next() % 16] = std::numeric_limits<float>::infinity(); break;
            case 3: f[next() % 16] = unit() * 1.0e30f; break;
            }
            CHECK(!MatPure::IsPlausibleTransform(g));
            break;
        }
        case 3: // viewproj product: always plausible
        {
            Mat4 v = MatPure::Multiply(
                MatPure::RotationYPR(unit() * 3.14159f, unit() * 1.55f, 0.0f),
                MatPure::Translation(unit() * 100.0f, unit() * 10.0f, unit() * 100.0f));
            Mat4 vp = MatPure::Multiply(v, TestProj());
            CHECK(MatPure::IsPlausibleTransform(vp));
            break;
        }
        }
    }
}

static void TestIsPerspective()
{
    CHECK(MatPure::IsPerspective(TestProj()));
    Mat4 ortho = MatPure::Identity();
    ortho._33 = 0.001f; ortho._34 = 0.0f; // _44 = 1 from Identity
    CHECK(!MatPure::IsPerspective(ortho));
    CHECK(!MatPure::IsPerspective(MatPure::Identity()));
}

static void TestEyeProjection()
{
    Mat4 p = TestProj();
    // nearPlane = -_43/_33 = 0.1 (hand-computed above)
    const float expectShift = 0.032f * p._11 / 0.1f; // 0.77254...
    // left eye: _31 = -halfSep * _11 / n ; right eye: +...
    Mat4 L = MatPure::EyeProjection(p, 0.032f, 0);
    Mat4 R = MatPure::EyeProjection(p, 0.032f, 1);
    CHECK(Near(L._31, -expectShift, 1e-3f));
    CHECK(Near(R._31, +expectShift, 1e-3f));
    // all other elements unchanged
    CHECK(Near(L._11, p._11) && Near(L._33, p._33) && Near(L._43, p._43));

    // halfSep=0 degenerate: shift is 0, rest unchanged
    Mat4 Z = MatPure::EyeProjection(p, 0.0f, 0);
    CHECK(Near(Z._31, 0.0f) && Near(Z._11, p._11));

    // _33 ~ 0 guard: unmodified
    Mat4 bad = p; bad._33 = 0.0f;
    Mat4 G = MatPure::EyeProjection(bad, 0.032f, 0);
    CHECK(memcmp(&G, &bad, sizeof(Mat4)) == 0);

    // orthographic (no _34 == 1): unmodified passthrough
    Mat4 ortho = MatPure::Identity();
    ortho._33 = 1.0f; ortho._34 = 0.0f; // _44 = 1 from Identity
    Mat4 O = MatPure::EyeProjection(ortho, 0.032f, 0);
    CHECK(memcmp(&O, &ortho, sizeof(Mat4)) == 0);

    // negative near plane (malformed): unmodified
    Mat4 neg = p; neg._43 = -neg._43; // flips extracted n to -0.1
    Mat4 N = MatPure::EyeProjection(neg, 0.032f, 0);
    CHECK(memcmp(&N, &neg, sizeof(Mat4)) == 0);
}

static void TestRotationTranspose()
{
    // R * R^T = I for pure rotations
    Mat4 r  = MatPure::RotationYPR(0.3f, -0.2f, 0.1f);
    Mat4 rt = MatPure::Transpose3x3(r);
    Mat4 i  = MatPure::Multiply(r, rt);
    Mat4 id = MatPure::Identity();
    for (int k = 0; k < 16; ++k)
        CHECK(Near((&i._11)[k], (&id._11)[k], 1e-5f));
}

// ---------------------------------------------------------------------------
// Duplication gate
// ---------------------------------------------------------------------------

static void TestDupGate()
{
    // Diverted texture wins over everything else.
    CHECK(DecideDuplication(true,  true,  false, true,  2,  64) == DupDecision::Skip);
    CHECK(DecideDuplication(true,  false, true,  false, 0,  64) == DupDecision::Skip);

    // Offscreen RT passes through unless explicitly in an enabled per-eye
    // shadow-pass substitution (Config::offscreenPasses).
    CHECK(DecideDuplication(false, false, false, true,  2,  64) == DupDecision::MonoPassThrough);
    CHECK(DecideDuplication(false, false, false, false, 500, 64) == DupDecision::MonoPassThrough);
    CHECK(DecideDuplication(false, false, true,  true,  2,  64) == DupDecision::StereoBothEyes);

    // SBSMonoUI threshold: at/below -> flat; above -> stereo.
    // The 2-triangle case is the engine composite quad (right-half-stale-sky
    // regression: it MUST go flat or the composite blit gets eye-shifted).
    CHECK(DecideDuplication(false, true, false, true, 2,  64) == DupDecision::FlatBothHalves);
    CHECK(DecideDuplication(false, true, false, true, 64, 64) == DupDecision::FlatBothHalves);
    CHECK(DecideDuplication(false, true, false, true, 65, 64) == DupDecision::StereoBothEyes);

    // primCount<=0 (DrawRectPatch etc.) is never classified as UI.
    CHECK(DecideDuplication(false, true, false, true, 0,  64) == DupDecision::StereoBothEyes);
    CHECK(DecideDuplication(false, true, false, true, -1, 64) == DupDecision::StereoBothEyes);

    // SBSMonoUI off: everything on the main RT duplicates.
    CHECK(DecideDuplication(false, true, false, false, 2, 64) == DupDecision::StereoBothEyes);
}

// ---------------------------------------------------------------------------
// Melee core
// ---------------------------------------------------------------------------

static void TestVelocityToView()
{
    float r, u;
    // yaw=0: right = +x, up = +y (identity mapping on x/y; z ignored for up)
    float v[3] = { 1.0f, 2.0f, -3.0f };
    Melee::VelocityToView(v, 0.0f, &r, &u);
    CHECK(Near(r, 1.0f) && Near(u, 2.0f));
    // yaw=+90 deg: head right vector = (cos, 0, -sin) = (0,0,-1);
    // v=(1,2,-3) -> right = 1*0 - (-3)*1 = +3
    const float pi2 = 1.5707963f;
    Melee::VelocityToView(v, pi2, &r, &u);
    CHECK(Near(r, 3.0f, 1e-3f) && Near(u, 2.0f));
}

static void TestClassifySwing()
{
    // App space: +x right, +y up, -z fwd.
    CHECK(Melee::ClassifySwing( 3.0f, 0.2f, 0.1f) == Melee::DIR_RIGHT);
    CHECK(Melee::ClassifySwing(-3.0f, 0.2f, 0.1f) == Melee::DIR_LEFT);
    CHECK(Melee::ClassifySwing( 0.2f, -3.0f, 0.1f) == Melee::DIR_OVERHEAD); // downward
    CHECK(Melee::ClassifySwing( 0.2f,  3.0f, 0.1f) == Melee::DIR_NONE);    // upward: no attack
    CHECK(Melee::ClassifySwing( 0.2f, 0.1f, -3.0f) == Melee::DIR_THRUST);  // forward = stab
    CHECK(Melee::ClassifySwing( 0.2f, 0.1f,  3.0f) == Melee::DIR_THRUST);  // |z| dominant
    // tie ay==ax resolves to the y branch (ay >= ax): downward -> overhead
    CHECK(Melee::ClassifySwing(-2.0f, -2.0f, 0.0f) == Melee::DIR_OVERHEAD);
}

static void TestFindPeak()
{
    Melee::Sample ring[4] = {};
    ring[0] = { 3.0f, 0.0f, 0.0f, 1000 }; // in window
    ring[1] = { 9.0f, 0.0f, 0.0f, 100 };  // stale (>600ms old at now=1000)
    ring[2] = { 0.0f, -2.5f, 0.0f, 900 }; // in window
    ring[3] = { 9.9f, 0.0f, 0.0f, 0 };    // tMs==0: never sampled, skipped
    Melee::Sample peak{};
    CHECK(Melee::FindPeak(ring, 4, 1000, 600, 1.8f, &peak));
    CHECK(Near(peak.vx, 3.0f)); // stale 9.0 and empty 9.9 must not win
    // min speed gate
    CHECK(!Melee::FindPeak(ring, 4, 1000, 600, 5.0f, &peak));
}

static void TestSteerRelease()
{
    using namespace Melee;
    // keep steering
    CHECK(SteerRelease(true, 100, 600, true, 2.0f, 1.8f) == RELEASE_NONE);
    // steer disabled -> immediate release
    CHECK(SteerRelease(false, 0, 600, true, 5.0f, 1.8f) == RELEASE_STEER_DISABLED_OR_TIMEOUT);
    // timeout
    CHECK(SteerRelease(true, 601, 600, true, 5.0f, 1.8f) == RELEASE_STEER_DISABLED_OR_TIMEOUT);
    CHECK(SteerRelease(true, 600, 600, true, 5.0f, 1.8f) == RELEASE_NONE); // boundary
    // lost tracking
    CHECK(SteerRelease(true, 100, 600, false, 0.0f, 1.8f) == RELEASE_LOST_TRACKING);
    // hand stopped: below half threshold after 120ms
    CHECK(SteerRelease(true, 200, 600, true, 0.8f, 1.8f) == RELEASE_HAND_STOPPED);
    CHECK(SteerRelease(true, 100, 600, true, 0.8f, 1.8f) == RELEASE_NONE); // too early
    CHECK(SteerRelease(true, 200, 600, true, 0.95f, 1.8f) == RELEASE_NONE); // fast enough
    // timeout wins over lost tracking (priority order preserved)
    CHECK(SteerRelease(true, 700, 600, false, 0.0f, 1.8f) == RELEASE_STEER_DISABLED_OR_TIMEOUT);
}

static void TestSteerAndBeginDeltas()
{
    int dx, dy;
    Melee::SteerDelta(1.0f, 1.0f, 40.0f, 60, &dx, &dy);
    CHECK(dx == 40 && dy == -40); // screen Y is down
    // per-poll clamp: a velocity spike cannot exceed maxDelta in one frame
    Melee::SteerDelta(50.0f, -50.0f, 40.0f, 60, &dx, &dy);
    CHECK(dx == 60 && dy == 60);
    Melee::SteerDelta(-50.0f, 50.0f, 40.0f, 60, &dx, &dy);
    CHECK(dx == -60 && dy == -60);
    // zero clamp disables steering output entirely
    Melee::SteerDelta(1.0f, 1.0f, 40.0f, 0, &dx, &dy);
    CHECK(dx == 0 && dy == 0);
    Melee::BeginDelta(Melee::DIR_LEFT, &dx, &dy);     CHECK(dx == -80 && dy == 0);
    Melee::BeginDelta(Melee::DIR_RIGHT, &dx, &dy);    CHECK(dx == 80 && dy == 0);
    Melee::BeginDelta(Melee::DIR_OVERHEAD, &dx, &dy); CHECK(dx == 0 && dy == -80);
    Melee::BeginDelta(Melee::DIR_THRUST, &dx, &dy);   CHECK(dx == 0 && dy == 80);
}

// ---------------------------------------------------------------------------
// Gesture zones (head at origin for simplicity)
// ---------------------------------------------------------------------------

static void TestHandDistance()
{
    const float l[3] = { -0.30f, 1.50f, -0.20f };
    const float r[3] = { 0.30f, 1.50f, -0.20f };
    CHECK(Near(Melee::HandDistance(l, r), 0.60f));
    CHECK(Near(Melee::HandDistance(l, l), 0.0f)); // identical points
}

static void TestGestureZones()
{
    const float head[3] = { 0.0f, 1.7f, 0.0f };
    float hp[3];

    // left hip
    hp[0] = -0.20f; hp[1] = 1.7f - 0.30f; hp[2] = 0.10f;
    CHECK(ClassifyZone(0, hp, head) == ZONE_HIP_L);
    // right hip
    hp[0] = 0.20f;
    CHECK(ClassifyZone(1, hp, head) == ZONE_HIP_R);
    // left hand cannot grab the right hip zone
    CHECK(ClassifyZone(0, hp, head) == ZONE_NONE);

    // --- forearm block zone -------------------------------------------------
    hp[0] = -0.15f; hp[1] = 1.60f; hp[2] = -0.25f; // left forearm in front of face
    CHECK(ClassifyZone(0, hp, head) == ZONE_BLOCK_FOREARM);
    hp[0] = 0.20f; hp[1] = 1.30f; hp[2] = 0.00f;   // right hip is not a block
    CHECK(ClassifyZone(1, hp, head) == ZONE_HIP_R);
    CHECK(ClassifyZone(0, hp, head) == ZONE_NONE);
    hp[0] = -0.15f; hp[1] = 1.90f; hp[2] = -0.25f; // too high
    CHECK(ClassifyZone(0, hp, head) == ZONE_NONE);
    hp[0] = -0.15f; hp[1] = 1.60f; hp[2] = 0.05f;  // in front of shoulder plane
    CHECK(ClassifyZone(0, hp, head) == ZONE_NONE);

    // over-shoulder (either hand)
    hp[0] = 0.0f; hp[1] = 1.7f + 0.10f; hp[2] = 0.20f;
    CHECK(ClassifyZone(1, hp, head) == ZONE_SHOULDER);
    CHECK(ClassifyZone(0, hp, head) == ZONE_SHOULDER);

    // quiver: right hand, chest height, behind-right.
    // NOTE (documented overlap): the hip band is checked first, so
    // dy in (-0.50,-0.18) with |dz|<0.30 resolves to HIP_R even when the
    // quiver box also matches; quiver is reachable at dy in [-0.18,-0.05)
    // below the shoulder band (dy <= -0.15) - e.g. dy=-0.16.
    hp[0] = 0.10f; hp[1] = 1.7f - 0.16f; hp[2] = 0.20f;
    CHECK(ClassifyZone(1, hp, head) == ZONE_QUIVER);
    // ...and the overlap band goes to the hip (order is intentional: hip
    // weapons outrank arrows):
    hp[1] = 1.7f - 0.30f;
    CHECK(ClassifyZone(1, hp, head) == ZONE_HIP_R);
    // quiver is right-hand-only
    hp[1] = 1.7f - 0.16f; hp[0] = -0.10f;
    CHECK(ClassifyZone(0, hp, head) == ZONE_NONE);

    // far away: nothing
    hp[0] = 1.5f; hp[1] = 1.7f; hp[2] = -1.0f;
    CHECK(ClassifyZone(1, hp, head) == ZONE_NONE);
}

// ---------------------------------------------------------------------------
// Parse helpers
// ---------------------------------------------------------------------------

static void TestParse()
{
    CHECK(Parse::KeyFromString("W") == 0x57);
    CHECK(Parse::KeyFromString("w") == 0x57);   // case-insensitive
    CHECK(Parse::KeyFromString("7") == 0x37);
    CHECK(Parse::KeyFromString("SPACE") == 0x20);
    CHECK(Parse::KeyFromString("space") == 0x20);
    CHECK(Parse::KeyFromString("F1") == 0x70);
    CHECK(Parse::KeyFromString("f6") == 0x75);
    CHECK(Parse::KeyFromString("F12") == 0x7B);
    CHECK(Parse::KeyFromString("F13") == 0);    // out of range
    CHECK(Parse::KeyFromString("F0") == 0);
    CHECK(Parse::KeyFromString("ENTER") == 0x0D);
    CHECK(Parse::KeyFromString("RETURN") == 0x0D);
    CHECK(Parse::KeyFromString("ESC") == 0x1B);
    CHECK(Parse::KeyFromString("SHIFT") == 0x10);
    CHECK(Parse::KeyFromString("CTRL") == 0x11);
    CHECK(Parse::KeyFromString("ALT") == 0x12);
    CHECK(Parse::KeyFromString("TAB") == 0x09);
    CHECK(Parse::KeyFromString("") == 0);
    CHECK(Parse::KeyFromString(nullptr) == 0);
    CHECK(Parse::KeyFromString(";") == 0);      // pure table: layout-dependent
                                                // punctuation stays 0 (prod
                                                // falls back to VkKeyScanA)
    CHECK(Parse::KeyFromString("NOPE") == 0);

    CHECK(Parse::ULong("500", 7) == 500);
    CHECK(Parse::ULong("0x1F", 7) == 31);
    CHECK(Parse::ULong("", 7) == 7);

    float v[3] = {};
    CHECK(Parse::Vec3("1 , 2 , 3", v) && Near(v[0], 1) && Near(v[1], 2) && Near(v[2], 3));
    CHECK(!Parse::Vec3("", v));
    CHECK(!Parse::Vec3("1 , 2", v));
}

// ---------------------------------------------------------------------------
// OneEuro determinism + convergence
// ---------------------------------------------------------------------------

static void TestOneEuro()
{
    // Determinism: identical sequences -> identical outputs.
    OneEuro a(90.0, 1.5, 0.02), b(90.0, 1.5, 0.02);
    const float inputs[8] = { 0.0f, 0.1f, 0.4f, 0.9f, 0.9f, 0.5f, 0.2f, 0.0f };
    for (int i = 0; i < 8; ++i)
    {
        float ra = a.Filter(inputs[i], 1.0 / 90.0);
        float rb = b.Filter(inputs[i], 1.0 / 90.0);
        CHECK(ra == rb);
    }
    // Convergence: constant input converges to it.
    OneEuro c(90.0, 1.5, 0.02);
    float out = 0.0f;
    for (int i = 0; i < 500; ++i) out = c.Filter(1.0f, 1.0 / 90.0);
    CHECK(Near(out, 1.0f, 1e-3f));
    // dt<=0 clamp: must not blow up or hang.
    OneEuro d(90.0);
    float r = d.Filter(0.5f, 0.0);
    CHECK(std::isfinite(r));
}

// ---------------------------------------------------------------------------
// Comfort policy / radial vignette
// ---------------------------------------------------------------------------

static void TestMouseMap()
{
    // 1600x900 window -> two 800x900 panels, eyes 1600x900, aspect-correct.
    // Fit is scale 0.5, left panel at (0,225), right at (800,225).
    long lx = 0, ly = 0;
    MouseMap::ClientToLogical(0, 100, 300, 800, 900, 1600, 900, true, &lx, &ly);
    CHECK(lx == 200 && ly == 150);           // (100/0.5, (300-225)/0.5)
    MouseMap::ClientToLogical(1, 900, 300, 800, 900, 1600, 900, true, &lx, &ly);
    CHECK(lx == 200 && ly == 150);           // same logical point on right eye

    // Center of the left panel's menu area maps back to logical center.
    MouseMap::ClientToLogical(0, 400, 450, 800, 900, 1600, 900, true, &lx, &ly);
    CHECK(lx == 800 && ly == 450);

    // Stretch-to-fill mapping: x halves, y unchanged.
    MouseMap::ClientToLogical(0, 100, 300, 800, 900, 1600, 900, false, &lx, &ly);
    CHECK(lx == 200 && ly == 300);
    MouseMap::ClientToLogical(1, 900, 300, 800, 900, 1600, 900, false, &lx, &ly);
    CHECK(lx == 200 && ly == 300);

    // Off-panel positions map outside the logical window (no false hover).
    MouseMap::ClientToLogical(0, 10, 10, 800, 900, 1600, 900, true, &lx, &ly);
    CHECK(ly < 0); // above the letterboxed image
}

static void TestComfort()
{
    float scale = 0.0f;
    CHECK(Comfort::WorldScaleFromPreset("compact", &scale) && Near(scale, 0.85f));
    CHECK(Comfort::WorldScaleFromPreset("REALISTIC", &scale) && Near(scale, 1.00f));
    CHECK(Comfort::WorldScaleFromPreset("large", &scale) && Near(scale, 1.15f));
    CHECK(Comfort::WorldScaleFromPreset("giant", &scale) && Near(scale, 1.35f));
    CHECK(!Comfort::WorldScaleFromPreset("tiny", &scale));
    CHECK(!Comfort::WorldScaleFromPreset(nullptr, &scale));

    CHECK(Comfort::Clamp01(-0.25f) == 0.0f);
    CHECK(Comfort::Clamp01(1.25f) == 1.0f);
    CHECK(Comfort::VignetteAlpha(0.5f, 0.5f, 1.0f) == 0);
    const unsigned center = Comfort::VignetteAlpha(0.65f, 0.65f, 1.0f);
    const unsigned cornerLow = Comfort::VignetteAlpha(1.0f, 1.0f, 0.25f);
    const unsigned cornerHigh = Comfort::VignetteAlpha(1.0f, 1.0f, 1.0f);
    CHECK(cornerLow > 0 && cornerHigh > cornerLow);
    CHECK(cornerHigh <= 176);
    CHECK(center < cornerHigh);
    CHECK(Comfort::VignettePixel(1.0f, 1.0f, 2.0f) == (176u << 24));
}

// ---------------------------------------------------------------------------

int main()
{
    TestEyeView();
    TestIsPlausibleTransform();
    TestIsFlatOrtho();
    TestIsPerspective();
    TestClassifierFuzz();
    TestEyeProjection();
    TestRotationTranspose();
    TestDupGate();
    TestVelocityToView();
    TestClassifySwing();
    TestFindPeak();
    TestSteerRelease();
    TestSteerAndBeginDeltas();
    TestHandDistance();
    TestGestureZones();
    TestParse();
    TestOneEuro();
    TestMouseMap();
    TestComfort();

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
