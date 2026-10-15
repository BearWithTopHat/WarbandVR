// SPDX-License-Identifier: GPL-3.0-only
// Headless integration tests: real DeviceHooks + StereoRenderer + Config +
// Logger compiled against the winshim, driven through a recording FakeDevice.
// Run from tests/: ./run_integration
//
// Scenarios assert the duplication gate end-to-end: which device calls
// happen, how many times, and with what render targets.

#include "windows.h"
#include "d3d9.h"
#include "fake_device.h"
#include "Config.h"
#include "Logger.h"
#include "StereoRenderer.h"
#include "DeviceHooks.h"
#include "MathUtil.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

static int g_failures = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { \
    ++g_failures; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void WriteIni(const char* text)
{
    mkdir("/tmp/wbvr_test", 0755);
    FILE* f = fopen("/tmp/wbvr_test/WarbandVR.ini", "w");
    fputs(text, f);
    fclose(f);
    setenv("WBVR_TEST_DIR", "/tmp/wbvr_test", 1);
}

static void WriteTestIni()
{
    WriteIni("[Stereo]\nEnableStereo=1\nSBSMonoUI=1\nUIMaxTriangles=64\nIPD=0.064\n"
             "[Comfort]\nWorldScalePreset=realistic\nVignette=0\n"
             "[Output]\nOutputMode=0\n[Debug]\nLogLevel=0\nDumpFrames=0\n");
}

static D3DMATRIX IdentityM()
{
    D3DMATRIX m{}; m._11 = m._22 = m._33 = m._44 = 1.0f; return m;
}

int main()
{
    // The logger writes under WBVR_TEST_DIR (winshim default below); create
    // it before the first Init or every log call silently no-ops on a clean
    // machine (regression: crash-log checks failed only on fresh sandboxes).
    mkdir("/tmp/wbvr_test", 0755);
    Log::Init(0);

    // Comfort preset parsing: preset supplies worldScale when no explicit
    // numeric [Stereo] WorldScale key exists; the expert numeric key wins.
    WriteIni("[Stereo]\nEnableStereo=1\n[Comfort]\nWorldScalePreset=large\n"
             "Vignette=1\nVignetteStrength=2.0\n[Debug]\nLogLevel=0\n");
    Config::Get().Load();
    CHECK(Config::Get().worldScale == 1.15f);
    CHECK(Config::Get().comfortVignette);
    CHECK(Config::Get().vignetteStrength == 1.0f);
    WriteIni("[Stereo]\nEnableStereo=1\nWorldScale=1.23\n"
             "[Comfort]\nWorldScalePreset=large\nVignette=0\n[Debug]\nLogLevel=0\n");
    Config::Get().Load();
    CHECK(Config::Get().worldScale == 1.23f);
    CHECK(!Config::Get().comfortVignette);

    // Controller preset: [Preset.<name>] overrides only the keys it sets;
    // base [Input] values remain for the rest. Empty preset = no override.
    WriteIni("[Input]\nSwingThreshold=1.8\nSteerScale=40.0\nBlockHoldTime=0.15\n"
             "ControllerPreset=quest\n"
             "[Preset.Quest]\nSwingThreshold=2.5\n[Debug]\nLogLevel=0\n");
    Config::Get().Load();
    CHECK(Config::Get().swingThreshold == 2.5f);   // preset override
    CHECK(Config::Get().steerScale == 40.0f);       // untouched by the preset
    WriteIni("[Input]\nSwingThreshold=1.9\nControllerPreset=\n[Debug]\nLogLevel=0\n");
    Config::Get().Load();
    CHECK(Config::Get().swingThreshold == 1.9f);    // empty preset: base value

    // --- crash handler logs module+RVA and never swallows -------------------
    {
        static int g_crashSite = 0;
        EXCEPTION_RECORD rec{};
        rec.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
        rec.ExceptionAddress = &g_crashSite;
        CONTEXT ctx{};
        EXCEPTION_POINTERS ep{ &rec, &ctx };
        CHECK(Log::CrashLogForTest(&ep) == EXCEPTION_CONTINUE_SEARCH);
        rec.ExceptionCode = 0x80000003; // breakpoint: deliberately not logged
        CHECK(Log::CrashLogForTest(&ep) == EXCEPTION_CONTINUE_SEARCH);
        FILE* f = fopen("/tmp/wbvr_test/WarbandVR_log.txt", "r");
        char buf[4096] = {};
        if (f) { (void)fread(buf, 1, sizeof(buf) - 1, f); fclose(f); }
        CHECK(strstr(buf, "CRASH code=0xC0000005 at d3d9.dll+") != nullptr);
        CHECK(strstr(buf, "80000003") == nullptr);
    }

    // --- previous log survives a relaunch (prev-log rotation) ---------------
    Log::Shutdown();
    Log::Init(0);
    {
        FILE* f = fopen("/tmp/wbvr_test/WarbandVR_log.prev.txt", "r");
        CHECK(f != nullptr); // the pre-relaunch log (with the CRASH line)
        if (f) fclose(f);
    }

    WriteTestIni();
    Config::Get().Load();
    CHECK(Config::Get().enableStereo);
    CHECK(Config::Get().uiMaxTriangles == 64);

    // --- device creation builds per-eye resources -------------------------
    FakeDevice dev(1600, 900);
    StereoRenderer::Get().OnDeviceCreated(&dev, &dev.pp);
    DeviceHooks::Get().OnDeviceCreated(&dev, &dev.pp);
    CHECK(!StereoRenderer::Get().IsActive()); // not before first BeginScene
    CHECK(StereoRenderer::Get().EyeSurface(0) != nullptr);
    CHECK(StereoRenderer::Get().EyeSurface(1) != nullptr);
    CHECK(StereoRenderer::Get().EyeSurface(0) != StereoRenderer::Get().EyeSurface(1));
    CHECK(StereoRenderer::Get().EyeWidth() == 1600 && StereoRenderer::Get().EyeHeight() == 900);

    IDirect3DSurface9* bb = nullptr;
    dev.GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);

    // Stereo arms at BeginScene (per-frame lifecycle), not at creation.
    hkSetRenderTarget(&dev, 0, bb);
    hkBeginScene(&dev);
    CHECK(StereoRenderer::Get().IsActive());

    // --- world draw on the main RT duplicates per eye ---------------------
    Fake::Clear();
    hkSetRenderTarget(&dev, 0, bb);
    CHECK(StereoRenderer::Get().CurrentRTIsMain());
    D3DMATRIX view = IdentityM();
    hkSetTransform(&dev, D3DTS_VIEW, &view);

    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 100); // world: 100 tris > 64
    CHECK(Fake::Count("DrawPrimitive") == 2);          // once per eye
    CHECK(Fake::Count("SetRenderTarget 0") >= 2);      // eye targets + restore
    // device ends back on the game's RT
    IDirect3DSurface9* cur = nullptr;
    dev.GetRenderTarget(0, &cur);
    CHECK(cur == bb);

    // --- tiny UI batch goes flat to both halves, no eye shift -------------
    Fake::Clear();
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 2);   // 2-triangle composite quad
    CHECK(Fake::Count("DrawPrimitive") == 2);
    // flat: no eye-shifted transform (eye shift would show as _41 != 0)
    CHECK(Fake::CountContaining("SetTransform", "_41=0.0000") == Fake::Count("SetTransform"));

    // --- above-threshold batch duplicates ---------------------------------
    Fake::Clear();
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 65);
    CHECK(Fake::Count("DrawPrimitive") == 2);
    CHECK(Fake::Count("SetTransform") > 0);            // eye matrices applied
    CHECK(Fake::CountContaining("SetTransform", "_41=-0.0320") > 0 ||
          Fake::CountContaining("SetTransform", "_41=0.0320") > 0); // eye shift present

    // --- steep-pitch view is NOT misclassified as flat UI -------------------
    // cos(pitch) shrinks _33 below the z cutoff; the orthonormal-row guard
    // must keep it stereo (regression: old z-only rule flattened this).
    D3DMATRIX steep = MatToD3D(MatPure::RotationYPR(0.0f, 80.0f * 3.14159265f / 180.0f, 0.0f));
    hkSetTransform(&dev, D3DTS_VIEW, &steep);
    Fake::Clear();
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 100);
    CHECK(Fake::Count("DrawPrimitive") == 2);
    CHECK(Fake::CountContaining("SetTransform", "_41=0.0320") > 0 ||
          Fake::CountContaining("SetTransform", "_41=-0.0320") > 0); // still shifted
    hkSetTransform(&dev, D3DTS_VIEW, &view); // back to the plain view

    // --- game's own offscreen RT passes through untouched -----------------
    Fake::Clear();
    FakeSurface shadowMap(256, 256);
    hkSetRenderTarget(&dev, 0, &shadowMap);
    CHECK(!StereoRenderer::Get().CurrentRTIsMain());
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 50);
    CHECK(Fake::Count("DrawPrimitive") == 1);          // mono passthrough
    hkSetRenderTarget(&dev, 0, bb);

    // --- draws sampling a diverted main-sized surface are skipped ---------
    // The backbuffer-sized "world texture" surface was RT0 while stereo was
    // active, so it is on the diverted list (its content lives in the eyes).
    Fake::Clear();
    FakeSurface worldTex(1600, 900);
    hkSetRenderTarget(&dev, 0, &worldTex);             // becomes diverted
    hkSetRenderTarget(&dev, 0, bb);
    class FakeSurfTex : public IDirect3DTexture9 { // minimal texture whose
    public:                                            // level-0 surface is the
        IDirect3DSurface9* s;                          // diverted one
        explicit FakeSurfTex(IDirect3DSurface9* surf) : s(surf) {}
        HRESULT __stdcall QueryInterface(REFIID, void** pp) override { *pp = this; return S_OK; }
        ULONG __stdcall AddRef() override { return 1; }
        ULONG __stdcall Release() override { return 1; }
        HRESULT __stdcall GetDevice(IDirect3DDevice9**) override { return S_OK; }
        HRESULT __stdcall SetPrivateData(REFGUID, const void*, DWORD, DWORD) override { return S_OK; }
        HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { return S_OK; }
        HRESULT __stdcall FreePrivateData(REFGUID) override { return S_OK; }
        DWORD __stdcall SetPriority(DWORD) override { return 0; }
        DWORD __stdcall GetPriority() override { return 0; }
        void __stdcall PreLoad() override {}
        D3DRESOURCETYPE __stdcall GetType() override { return D3DRTYPE_TEXTURE; }
        DWORD __stdcall SetLOD(DWORD) override { return 0; }
        DWORD __stdcall GetLOD() override { return 0; }
        DWORD __stdcall GetLevelCount() override { return 1; }
        HRESULT __stdcall SetAutoGenFilterType(D3DTEXTUREFILTERTYPE) override { return S_OK; }
        D3DTEXTUREFILTERTYPE __stdcall GetAutoGenFilterType() override { return D3DTEXF_NONE; }
        void __stdcall GenerateMipSubLevels() override {}
        HRESULT __stdcall GetLevelDesc(UINT, D3DSURFACE_DESC* p) override { return s->GetDesc(p); }
        HRESULT __stdcall GetSurfaceLevel(UINT, IDirect3DSurface9** pp) override { *pp = s; return S_OK; }
        HRESULT __stdcall LockRect(UINT, D3DLOCKED_RECT*, const RECT*, DWORD) override { return S_OK; }
        HRESULT __stdcall UnlockRect(UINT) override { return S_OK; }
        HRESULT __stdcall AddDirtyRect(const RECT*) override { return S_OK; }
    } wt(&worldTex);
    Fake::Clear();
    hkSetTexture(&dev, 0, &wt);
    CHECK(StereoRenderer::Get().DivertedTextureBound());
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 2);
    CHECK(Fake::Count("DrawPrimitive") == 0);          // skipped entirely
    hkSetTexture(&dev, 0, nullptr);                    // unbind
    CHECK(!StereoRenderer::Get().DivertedTextureBound());

    // --- internal guard passes straight through ----------------------------
    Fake::Clear();
    DeviceHooks::Get().m_internal = true;
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 100);
    CHECK(Fake::Count("DrawPrimitive") == 1);          // no duplication
    DeviceHooks::Get().m_internal = false;

    // --- comfort vignette draws the generated radial overlay into both eyes --
    Config::Get().comfortVignette = true;
    Config::Get().vignetteStrength = 0.75f;
    Fake::Clear();
    hkEndScene(&dev);
    CHECK(Fake::Count("CreateTexture 64x64") == 1);
    CHECK(Fake::Count("DrawPrimitiveUP") == 2);
    CHECK(Fake::Count("SetRenderTarget 0") >= 3); // eye0, eye1, restore
    CHECK(dev.renderStates[D3DRS_ALPHABLENDENABLE] == 0);
    CHECK(dev.renderStates[D3DRS_ZENABLE] == 0);
    CHECK(dev.fvf == 0);
    CHECK(dev.textures[0] == nullptr);
    Config::Get().comfortVignette = false;

    // --- Present composites both eyes to the backbuffer ---------------------
    Fake::Clear();
    hkPresent(&dev, nullptr, nullptr, nullptr, nullptr);
    CHECK(Fake::Count("StretchRect") == 2);
    CHECK(Fake::Count("Present") == 1);
    CHECK(!StereoRenderer::Get().IsActive());

    // Rearm for the next frame.
    hkSetRenderTarget(&dev, 0, bb);
    hkBeginScene(&dev);
    CHECK(StereoRenderer::Get().IsActive());

    // --- game scissor state is applied per eye and restored -----------------
    RECT sr = { 10, 20, 300, 200 };
    hkSetRenderState(&dev, D3DRS_SCISSORTESTENABLE, TRUE);
    hkSetScissorRect(&dev, &sr);
    Fake::Clear();
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 100);
    CHECK(Fake::Count("SetScissorRect") == 3); // eye0, eye1, restore game rect
    CHECK(dev.hasScissor);
    CHECK(dev.scissor.left == 10 && dev.scissor.right == 300);

    // --- ViewSource=1: eye shift uses VS register readback and restores it --
    Config::Get().viewSource = 1;
    Config::Get().vsViewRegister = 0;
    D3DMATRIX shaderView = IdentityM();
    shaderView._41 = 0.5f;
    hkSetVertexShaderConstantF(&dev, 0, reinterpret_cast<const float*>(&shaderView), 4);
    Fake::Clear();
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 100);
    CHECK(Fake::Count("DrawPrimitive") == 2);
    CHECK(Fake::Count("SetVSCF c0 x4") == 3); // eye0, eye1, restore
    CHECK(Fake::CountContaining("SetVSCF", "_41=0.5320") > 0 ||
          Fake::CountContaining("SetVSCF", "_41=0.4680") > 0);
    CHECK(dev.vsConstants[12] == shaderView._41); // game's register restored

    // --- ViewSource=1 with an implausible register: fail safe to mono -------
    // A wrong VSViewRegister guess yields garbage; the pair must render
    // unshifted (no upload, no restore) rather than stereo-shifting garbage.
    D3DMATRIX garbage = {};
    hkSetVertexShaderConstantF(&dev, 0, reinterpret_cast<const float*>(&garbage), 4);
    Fake::Clear();
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 100);
    CHECK(Fake::Count("DrawPrimitive") == 2);   // both eyes still draw
    CHECK(Fake::Count("SetVSCF c0 x4") == 0);   // no shifted upload, no restore

    D3DMATRIX hugeMat = IdentityM();
    hugeMat._11 = 1.0e30f; // beyond any sane world extent
    hkSetVertexShaderConstantF(&dev, 0, reinterpret_cast<const float*>(&hugeMat), 4);
    Fake::Clear();
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 100);
    CHECK(Fake::Count("SetVSCF c0 x4") == 0);
    Config::Get().viewSource = 0;

    // --- opt-in offscreen pass: per-eye shadow targets and clears -----------
    Config::Get().offscreenPasses = true;
    FakeSurface shadow256(256, 256);
    Fake::Clear();
    hkSetRenderTarget(&dev, 0, &shadow256);
    CHECK(StereoRenderer::Get().ShadowActive());
    CHECK(Fake::Count("CreateTexture 256x256") == 2);
    hkClear(&dev, 0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
    CHECK(Fake::Count("Clear") == 2);
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 65);
    CHECK(Fake::Count("DrawPrimitive") == 2);
    hkSetRenderTarget(&dev, 0, bb);
    CHECK(!StereoRenderer::Get().ShadowActive());
    Config::Get().offscreenPasses = false;

    // --- Reset releases DEFAULT-pool eyes, then rebuilds at the new size ----
    D3DPRESENT_PARAMETERS pp2 = dev.pp;
    pp2.BackBufferWidth = 800; pp2.BackBufferHeight = 600;
    Fake::Clear();
    CHECK(SUCCEEDED(hkReset(&dev, &pp2)));
    CHECK(StereoRenderer::Get().EyeSurface(0) == nullptr);
    IDirect3DSurface9* bb2 = nullptr;
    dev.GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb2);
    hkSetRenderTarget(&dev, 0, bb2);
    hkBeginScene(&dev);
    CHECK(StereoRenderer::Get().IsActive());
    CHECK(StereoRenderer::Get().EyeWidth() == 800);
    CHECK(StereoRenderer::Get().EyeHeight() == 600);
    CHECK(Fake::Count("CreateRenderTarget 800x600") == 2);

    // --- SharedSurface failure falls back to plain eye targets --------------
    Config::Get().sharedSurface = true;
    StereoRenderer::Get().Shutdown();
    FakeDevice exDev(1024, 768);
    exDev.failCreateTextureCalls = 1;
    Fake::Clear();
    StereoRenderer::Get().OnDeviceCreated(&exDev, &exDev.pp);
    CHECK(StereoRenderer::Get().EyeSurface(0) != nullptr);
    CHECK(StereoRenderer::Get().EyeSharedHandle(0) == nullptr);
    CHECK(Fake::Count("CreateTexture 1024x768") == 1);
    CHECK(Fake::Count("CreateRenderTarget 1024x768") == 2);
    Config::Get().sharedSurface = false;

    // --- eye creation failure is contained and retried next frame -----------
    StereoRenderer::Get().Shutdown();
    FakeDevice failDev(640, 480);
    failDev.failCreateRenderTargetCalls = 1;
    StereoRenderer::Get().OnDeviceCreated(&failDev, &failDev.pp);
    CHECK(StereoRenderer::Get().EyeSurface(0) == nullptr);
    hkBeginScene(&failDev);
    CHECK(StereoRenderer::Get().EyeSurface(0) != nullptr);
    CHECK(StereoRenderer::Get().EyeWidth() == 640);

    // --- failed Reset does not crash; resources stay released ---------------
    StereoRenderer::Get().OnDeviceCreated(&dev, &dev.pp);
    dev.resetResult = E_FAIL;
    D3DPRESENT_PARAMETERS pp3 = dev.pp;
    CHECK(hkReset(&dev, &pp3) == E_FAIL);
    CHECK(StereoRenderer::Get().EyeSurface(0) == nullptr);
    dev.resetResult = S_OK;

    // --- EnableStereo=0: stock passthrough ---------------------------------
    Config::Get().enableStereo = false;
    StereoRenderer::Get().Shutdown();
    StereoRenderer::Get().OnDeviceCreated(&dev, &dev.pp);
    CHECK(!StereoRenderer::Get().IsActive());
    Fake::Clear();
    hkDrawPrimitive(&dev, D3DPT_TRIANGLELIST, 0, 100);
    CHECK(Fake::Count("DrawPrimitive") == 1);
    CHECK(Fake::Count("SetTransform") == 0);

    // --- shutdown path runs clean ------------------------------------------
    Config::Get().enableStereo = true;
    StereoRenderer::Get().Shutdown();
    DeviceHooks::Get().Shutdown();
    Log::Shutdown();

    printf("integration: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
