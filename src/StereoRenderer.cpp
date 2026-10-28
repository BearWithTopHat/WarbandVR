// SPDX-License-Identifier: GPL-3.0-only
#include "StereoRenderer.h"
#include "Config.h"
#include "Logger.h"
#include "MathUtil.h"
#include "Capture.h"
#include "OpenXRBridge.h"
#include "OneEuroFilter.h"
#include "Comfort.h"
#include "MouseMap.h"

#include <cstring>
#include <algorithm>

StereoRenderer& StereoRenderer::Get()
{
    static StereoRenderer s;
    return s;
}

// ---------------------------------------------------------------------------
// Device lifecycle
// ---------------------------------------------------------------------------

void StereoRenderer::OnDeviceCreated(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS* pp)
{
    if (pp) m_pp = *pp;
    if (m_qpcFreq.QuadPart == 0) QueryPerformanceFrequency(&m_qpcFreq);
    ReleaseResources();
    if (Config::Get().enableStereo)
    {
        CreateResources(device);
        // Cursor coordinates must match the composite transform or menu
        // hit-tests land on the wrong item (Options hover outside the menu).
        if (Config::Get().outputMode == 0 && Config::Get().sbsMouseRemap &&
            pp && pp->hDeviceWindow)
            MouseMap::Install(pp->hDeviceWindow);
    }
}

void StereoRenderer::OnReset(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS* pp)
{
    // Called BEFORE the real Reset: D3DPOOL_DEFAULT resources must be dead.
    ReleaseResources();
    m_active = false;
    m_hasView = m_hasProj = m_hasGameVp = false;
    if (pp) m_pp = *pp;
    (void)device;
}

void StereoRenderer::Shutdown()
{
    MouseMap::Uninstall();
    ReleaseResources();
    m_active = false;
}

void StereoRenderer::CreateResources(IDirect3DDevice9* device)
{
    UINT w = m_pp.BackBufferWidth;
    UINT h = m_pp.BackBufferHeight;
    if (w == 0 || h == 0)
    {
        IDirect3DSurface9* bb = nullptr;
        if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb)
        {
            D3DSURFACE_DESC desc{};
            bb->GetDesc(&desc);
            w = desc.Width; h = desc.Height;
            bb->Release();
        }
    }
    if (w == 0 || h == 0)
    {
        LOG_ERROR("StereoRenderer: cannot determine backbuffer size");
        return;
    }

    // Depth format from the game's own depth-stencil, so depth behavior
    // (Z-prepass, stencil) matches stock per eye.
    D3DFORMAT dsFmt = D3DFMT_D24S8;
    IDirect3DSurface9* gameDS = nullptr;
    if (SUCCEEDED(device->GetDepthStencilSurface(&gameDS)) && gameDS)
    {
        D3DSURFACE_DESC dsDesc{};
        gameDS->GetDesc(&dsDesc);
        dsFmt = dsDesc.Format;
        gameDS->Release();
    }
    m_dsFormat = dsFmt;

    D3DFORMAT fmt = (m_pp.BackBufferFormat != D3DFMT_UNKNOWN) ? m_pp.BackBufferFormat : D3DFMT_X8R8G8B8;

    // M3 fast path: on a D3D9Ex device (see dllmain steering) with
    // SharedSurface=1, eye targets are shared textures so OpenXRBridge can
    // open them from D3D11 for a GPU-to-GPU copy.
    bool wantShared = false;
    if (Config::Get().sharedSurface)
    {
        IDirect3DDevice9Ex* ex = nullptr;
        if (SUCCEEDED(device->QueryInterface(IID_IDirect3DDevice9Ex, reinterpret_cast<void**>(&ex))) && ex)
        {
            ex->Release();
            wantShared = true;
        }
        else
        {
            LOG_WARN("StereoRenderer: SharedSurface=1 but device is not Ex - plain eye targets (slow XR path)");
        }
    }

    bool ok = true;
    HRESULT hr = S_OK;
    for (int eye = 0; eye < 2 && ok; ++eye)
    {
        if (wantShared)
        {
            hr = device->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt,
                                       D3DPOOL_DEFAULT, &m_eyeTex[eye], &m_eyeShared[eye]);
            if (SUCCEEDED(hr))
                hr = m_eyeTex[eye]->GetSurfaceLevel(0, &m_eyeSurf[eye]);
            if (FAILED(hr))
            {
                LOG_WARN("StereoRenderer: shared eye texture failed hr=0x%08X - falling back to plain targets", hr);
                wantShared = false;
                ReleaseEyeTargets();
                eye = -1; // restart the loop on the plain path
                continue;
            }
        }
        else
        {
            hr = device->CreateRenderTarget(w, h, fmt, D3DMULTISAMPLE_NONE, 0,
                                            FALSE, &m_eyeSurf[eye], nullptr);
            if (FAILED(hr))
            {
                LOG_ERROR("StereoRenderer: CreateRenderTarget(%ux%u) eye=%d failed hr=0x%08X", w, h, eye, hr);
                m_eyeSurf[eye] = nullptr;
                ok = false;
                break;
            }
        }
        hr = device->CreateDepthStencilSurface(w, h, dsFmt, D3DMULTISAMPLE_NONE, 0,
                                               TRUE, &m_eyeDS[eye], nullptr);
        if (FAILED(hr))
        {
            LOG_ERROR("StereoRenderer: CreateDepthStencilSurface(%ux%u fmt=%d) eye=%d failed hr=0x%08X",
                      w, h, dsFmt, eye, hr);
            m_eyeDS[eye] = nullptr;
            ok = false;
            break;
        }
    }
    if (!ok)
    {
        ReleaseResources();
        return;
    }
    m_eyeW = w;
    m_eyeH = h;
    m_needClear = true;
    LOG_INFO("StereoRenderer: per-eye targets %ux%u (depth fmt=%d, shared=%d) created",
             w, h, dsFmt, m_eyeShared[0] ? 1 : 0);
}

void StereoRenderer::ReleaseEyeTargets()
{
    for (int eye = 0; eye < 2; ++eye)
    {
        if (m_eyeDS[eye])   { m_eyeDS[eye]->Release();   m_eyeDS[eye] = nullptr; }
        if (m_eyeSurf[eye]) { m_eyeSurf[eye]->Release(); m_eyeSurf[eye] = nullptr; }
        if (m_eyeTex[eye])  { m_eyeTex[eye]->Release();  m_eyeTex[eye] = nullptr; }
        m_eyeShared[eye] = nullptr; // borrowed handle alias, not a COM reference
    }
}

void StereoRenderer::ReleaseResources()
{
    ReleaseAllShadows();
    if (m_vignetteTex) { m_vignetteTex->Release(); m_vignetteTex = nullptr; }
    m_vignetteFailed = false;
    ReleaseEyeTargets();
    m_eyeW = m_eyeH = 0;
    m_curRT = nullptr;
    m_curRTIsMain = false;
    m_diverted.clear();
    m_divertedBinds.clear();
    m_needClear = true;
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------

void StereoRenderer::OnBeginScene(IDirect3DDevice9* device)
{
    CheckRecenterKey();

    // TEMP DIAG: F9 toggles register logging, F10 dumps the eye surfaces
    // (previous frame's content - exactly what is on screen).
    bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    if (f9 && !m_f9Down)
    {
        m_rbLog = !m_rbLog;
        if (!m_rbLog)
        {
            for (auto* surf : m_smallRTs) surf->Release();
            m_smallRTs.clear();
            m_rtDumpIdx = 0;
        }
        LOG_INFO("DIAG: RB logging %s", m_rbLog ? "ON" : "OFF");
    }
    m_f9Down = f9;
    bool f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    if (f10 && !m_f10Down && m_eyeSurf[0])
        m_dumpAtPresent = true; // dumps eyes + pre-composite backbuffer in OnPresent
    m_f10Down = f10;

    if (!Config::Get().enableStereo) return;
    if (!m_eyeSurf[0]) { CreateResources(device); if (!m_eyeSurf[0]) return; }
    m_active = true;

    // Track RT0 even for scenes where the game never calls SetRenderTarget
    // (e.g. the menu, which renders to the default backbuffer) - an unseen
    // bind must not silently read as "offscreen" and force mono.
    IDirect3DSurface9* rt0 = nullptr;
    if (SUCCEEDED(device->GetRenderTarget(0, &rt0)) && rt0)
    {
        SetCurrentRT(rt0);
        rt0->Release();
    }

    if (m_needClear)
    {
        m_needClear = false;
        ClearEyeTargets(device);
    }
}

void StereoRenderer::ClearEyeTargets(IDirect3DDevice9* device)
{
    // Called with m_internal held (from hkBeginScene). Clears each eye
    // target once per render frame; the game's own clears are untouched.
    IDirect3DSurface9* savedRT = nullptr;
    IDirect3DSurface9* savedDS = nullptr;
    device->GetRenderTarget(0, &savedRT);
    device->GetDepthStencilSurface(&savedDS);

    for (int eye = 0; eye < 2; ++eye)
    {
        device->SetRenderTarget(0, m_eyeSurf[eye]);
        if (m_eyeDS[eye]) device->SetDepthStencilSurface(m_eyeDS[eye]);
        device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
    }

    device->SetRenderTarget(0, savedRT);
    if (savedRT) savedRT->Release();
    device->SetDepthStencilSurface(savedDS);
    if (savedDS) savedDS->Release();
}

bool StereoRenderer::EnsureVignetteTexture(IDirect3DDevice9* device)
{
    if (m_vignetteTex) return true;
    if (m_vignetteFailed) return false;

    constexpr UINT kSize = 64;
    HRESULT hr = device->CreateTexture(kSize, kSize, 1, 0, D3DFMT_A8R8G8B8,
                                       D3DPOOL_MANAGED, &m_vignetteTex, nullptr);
    if (FAILED(hr) || !m_vignetteTex)
    {
        LOG_WARN("Comfort vignette texture creation failed hr=0x%08X - disabled for this device", hr);
        m_vignetteFailed = true;
        return false;
    }

    D3DLOCKED_RECT locked{};
    hr = m_vignetteTex->LockRect(0, &locked, nullptr, 0);
    if (FAILED(hr) || !locked.pBits)
    {
        LOG_WARN("Comfort vignette texture lock failed hr=0x%08X - disabled for this device", hr);
        m_vignetteTex->Release();
        m_vignetteTex = nullptr;
        m_vignetteFailed = true;
        return false;
    }
    for (UINT y = 0; y < kSize; ++y)
    {
        uint32_t* row = reinterpret_cast<uint32_t*>(
            static_cast<unsigned char*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch);
        const float v = (static_cast<float>(y) + 0.5f) / kSize;
        for (UINT x = 0; x < kSize; ++x)
        {
            const float u = (static_cast<float>(x) + 0.5f) / kSize;
            row[x] = Comfort::VignettePixel(u, v, Config::Get().vignetteStrength);
        }
    }
    m_vignetteTex->UnlockRect(0);
    LOG_INFO("Comfort vignette texture %ux%u created (strength %.2f)",
             kSize, kSize, Config::Get().vignetteStrength);
    return true;
}

namespace
{
    // Scoped save/restore of every piece of pipeline state the comfort
    // overlay touches. Restore() replays state in the exact reverse order of
    // acquisition; the Get* calls AddRef the COM objects, released after
    // restore. Runs inside DeviceHooks' internal guard, so none of these
    // calls feed back into the game's tracked state.
    struct OverlayState
    {
        IDirect3DSurface9* rt = nullptr;
        IDirect3DSurface9* ds = nullptr;
        IDirect3DBaseTexture9* tex = nullptr;
        IDirect3DVertexShader9* vs = nullptr;
        IDirect3DPixelShader9* ps = nullptr;
        D3DVIEWPORT9 vp{};
        DWORD fvf = 0;
        DWORD rsZ = 0, rsAlpha = 0, rsSrc = 0, rsDest = 0, rsScissor = 0;
        DWORD tssColorOp = 0, tssColorArg1 = 0, tssColorArg2 = 0;
        DWORD tssAlphaOp = 0, tssAlphaArg1 = 0, tssAlphaArg2 = 0;
        DWORD sampAddrU = 0, sampAddrV = 0, sampMag = 0, sampMin = 0;

        void Save(IDirect3DDevice9* d)
        {
            d->GetRenderTarget(0, &rt);
            d->GetDepthStencilSurface(&ds);
            d->GetViewport(&vp);
            d->GetFVF(&fvf);
            d->GetTexture(0, &tex);
            d->GetVertexShader(&vs);
            d->GetPixelShader(&ps);
            d->GetRenderState(D3DRS_ZENABLE, &rsZ);
            d->GetRenderState(D3DRS_ALPHABLENDENABLE, &rsAlpha);
            d->GetRenderState(D3DRS_SRCBLEND, &rsSrc);
            d->GetRenderState(D3DRS_DESTBLEND, &rsDest);
            d->GetRenderState(D3DRS_SCISSORTESTENABLE, &rsScissor);
            d->GetTextureStageState(0, D3DTSS_COLOROP, &tssColorOp);
            d->GetTextureStageState(0, D3DTSS_COLORARG1, &tssColorArg1);
            d->GetTextureStageState(0, D3DTSS_COLORARG2, &tssColorArg2);
            d->GetTextureStageState(0, D3DTSS_ALPHAOP, &tssAlphaOp);
            d->GetTextureStageState(0, D3DTSS_ALPHAARG1, &tssAlphaArg1);
            d->GetTextureStageState(0, D3DTSS_ALPHAARG2, &tssAlphaArg2);
            d->GetSamplerState(0, D3DSAMP_ADDRESSU, &sampAddrU);
            d->GetSamplerState(0, D3DSAMP_ADDRESSV, &sampAddrV);
            d->GetSamplerState(0, D3DSAMP_MAGFILTER, &sampMag);
            d->GetSamplerState(0, D3DSAMP_MINFILTER, &sampMin);
        }

        void Restore(IDirect3DDevice9* d)
        {
            d->SetViewport(&vp);
            d->SetRenderTarget(0, rt);
            d->SetDepthStencilSurface(ds);
            d->SetSamplerState(0, D3DSAMP_MINFILTER, sampMin);
            d->SetSamplerState(0, D3DSAMP_MAGFILTER, sampMag);
            d->SetSamplerState(0, D3DSAMP_ADDRESSV, sampAddrV);
            d->SetSamplerState(0, D3DSAMP_ADDRESSU, sampAddrU);
            d->SetTextureStageState(0, D3DTSS_ALPHAARG2, tssAlphaArg2);
            d->SetTextureStageState(0, D3DTSS_ALPHAARG1, tssAlphaArg1);
            d->SetTextureStageState(0, D3DTSS_ALPHAOP, tssAlphaOp);
            d->SetTextureStageState(0, D3DTSS_COLORARG2, tssColorArg2);
            d->SetTextureStageState(0, D3DTSS_COLORARG1, tssColorArg1);
            d->SetTextureStageState(0, D3DTSS_COLOROP, tssColorOp);
            d->SetRenderState(D3DRS_SCISSORTESTENABLE, rsScissor);
            d->SetRenderState(D3DRS_DESTBLEND, rsDest);
            d->SetRenderState(D3DRS_SRCBLEND, rsSrc);
            d->SetRenderState(D3DRS_ALPHABLENDENABLE, rsAlpha);
            d->SetRenderState(D3DRS_ZENABLE, rsZ);
            d->SetTexture(0, tex);
            d->SetFVF(fvf);
            d->SetVertexShader(vs);
            d->SetPixelShader(ps);

            if (tex) tex->Release();
            if (vs)  vs->Release();
            if (ps)  ps->Release();
            if (rt)  rt->Release();
            if (ds)  ds->Release();
        }
    };
}

void StereoRenderer::RenderComfortVignette(IDirect3DDevice9* device)
{
    const Config& cfg = Config::Get();
    if (!cfg.comfortVignette || !m_eyeSurf[0] || !m_eyeSurf[1]) return;
    if (!EnsureVignetteTexture(device)) return;

    struct OverlayVertex { float x, y, z, rhw; DWORD color; float u, v; };
    const float w = static_cast<float>(m_eyeW);
    const float h = static_cast<float>(m_eyeH);
    const OverlayVertex quad[4] = {
        { -0.5f,     -0.5f,     0.0f, 1.0f, 0xFFFFFFFFu, 0.0f, 0.0f },
        {  w - 0.5f, -0.5f,     0.0f, 1.0f, 0xFFFFFFFFu, 1.0f, 0.0f },
        { -0.5f,      h - 0.5f, 0.0f, 1.0f, 0xFFFFFFFFu, 0.0f, 1.0f },
        {  w - 0.5f,  h - 0.5f, 0.0f, 1.0f, 0xFFFFFFFFu, 1.0f, 1.0f },
    };

    OverlayState saved;
    saved.Save(device);

    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    device->SetTexture(0, m_vignetteTex);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);

    D3DVIEWPORT9 vp{};
    vp.X = 0; vp.Y = 0; vp.Width = m_eyeW; vp.Height = m_eyeH;
    vp.MinZ = 0.0f; vp.MaxZ = 1.0f;
    device->SetViewport(&vp);
    for (int eye = 0; eye < 2; ++eye)
    {
        device->SetRenderTarget(0, m_eyeSurf[eye]);
        device->SetDepthStencilSurface(nullptr);
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
    }

    saved.Restore(device);
}

void StereoRenderer::OnEndScene(IDirect3DDevice9* device)
{
    if (m_active)
        RenderComfortVignette(device);

    // Perf rollup: every 600 frames report average duplication cost.
    ++m_frameCount;
    if (m_frameCount % 600 == 0 && m_dupCalls > 0)
    {
        LOG_INFO("[PERF] %d frames: draw duplication avg %.1f us/draw, %.2f ms/frame (%d draws)",
                 m_frameCount, m_dupTimeUs / m_dupCalls,
                 (m_dupTimeUs / 600.0) / 1000.0, m_dupCalls / 600);
        m_dupTimeUs = 0.0;
        m_dupCalls = 0;
    }
}

// (defined with the dump helpers below; F10 tex dump needs the module anchor)
static void WriteBmp(const char* path, UINT w, UINT h, LONG pitch,
                     const unsigned char* bits);

void StereoRenderer::OnPresent(IDirect3DDevice9* device)
{
    if (!m_active) return;

    EndShadowPass(); // in case a shadow pass was left open (M2)

    if (Config::Get().dumpFrames > 0 && (m_presentCount++ % Config::Get().dumpFrames) == 0)
        DumpStereoSurface(device, m_dumpedFrames++);

    if (TakePresentDumpRequest()) // F10: eyes + the pre-composite backbuffer
    {
        DumpStereoSurface(device, m_dumpedFrames);
        DumpBackbuffer(device, m_dumpedFrames);

        // TEMP DIAG (tooltip hunt): dump the texture bound to stage 0 - the
        // tooltip's foreground quad samples the shared UI atlas, which the
        // game GDI-renders text into.
        IDirect3DBaseTexture9* t0 = nullptr;
        if (SUCCEEDED(device->GetTexture(0, &t0)) && t0)
        {
            IDirect3DTexture9* tex = nullptr;
            if (SUCCEEDED(t0->QueryInterface(IID_IDirect3DTexture9, reinterpret_cast<void**>(&tex))) && tex)
            {
                HMODULE d3dx = GetModuleHandleA("d3dx9_42.dll");
                if (!d3dx) d3dx = GetModuleHandleA("d3dx9_43.dll");
                typedef HRESULT (WINAPI* SaveTex_t)(LPCSTR, UINT, IDirect3DBaseTexture9*, const void*);
                SaveTex_t saveTex = d3dx
                    ? reinterpret_cast<SaveTex_t>(GetProcAddress(d3dx, "D3DXSaveTextureToFileA"))
                    : nullptr;
                if (saveTex)
                {
                    char dllDir[MAX_PATH] = {};
                    HMODULE self = nullptr;
                    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       reinterpret_cast<LPCSTR>(&WriteBmp), &self);
                    GetModuleFileNameA(self, dllDir, MAX_PATH);
                    char* slash = strrchr(dllDir, '\\');
                    if (slash) slash[1] = '\0';
                    char path[MAX_PATH];
                    snprintf(path, MAX_PATH, "%.230sWarbandVR_frame%03d_tex0.bmp", dllDir, m_dumpedFrames);
                    HRESULT hr = saveTex(path, 2 /*D3DXIFF_BMP*/, tex, nullptr); // NOLINT
                    LOG_INFO("DumpTex0: %s hr=0x%08X", path, hr);
                }
                tex->Release();
            }
            t0->Release();
        }
        ++m_dumpedFrames;
    }

    if (Config::Get().outputMode == 1)
    {
        // M3: hand both eye frames to OpenXR.
        OpenXRBridge::SubmitFrame(device, m_eyeSurf[0], m_eyeSurf[1]);
        // Monitor mirror/fallback: composite whenever the XR session isn't
        // running, and optionally always (spectator view, h2-mod-vr style).
        if (!OpenXRBridge::SessionRunning() || Config::Get().mirrorOnDesktop)
            CompositeToBackbuffer(device);
    }
    else
    {
        CompositeToBackbuffer(device);
    }

    m_active = false;
    m_needClear = true;

    // TEMP DIAG: show the noted small RTs as an on-screen debug overlay while
    // RB logging is on (CPU readbacks reset the device; GPU copies are safe).
    if (m_rbLog)
        ShowSmallRTsOnScreen(device);
}

void StereoRenderer::CompositeToBackbuffer(IDirect3DDevice9* device)
{
    IDirect3DSurface9* bb = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb)
        return;

    D3DSURFACE_DESC bbDesc{};
    bb->GetDesc(&bbDesc);
    const LONG halfW = static_cast<LONG>(bbDesc.Width / 2);
    const LONG bbH   = static_cast<LONG>(bbDesc.Height);

    for (int eye = 0; eye < 2; ++eye)
    {
        if (!m_eyeSurf[eye]) continue;
        D3DSURFACE_DESC srcDesc{};
        m_eyeSurf[eye]->GetDesc(&srcDesc);
        RECT src = { 0, 0, static_cast<LONG>(srcDesc.Width), static_cast<LONG>(srcDesc.Height) };

        // The game lays out menus and text for the FULL window width. Simply
        // stretching each full-width eye into its split panel distorts that
        // UI per panel (the "menu glitch" report). Fit the eye into the
        // panel while preserving its aspect ratio and letterbox the rest;
        // SBSAspectCorrect=0 restores the old stretch-to-fill behavior.
        RECT dst;
        if (Config::Get().sbsAspectCorrect)
        {
            const float sx = static_cast<float>(halfW) / static_cast<float>(srcDesc.Width);
            const float sy = static_cast<float>(bbH) / static_cast<float>(srcDesc.Height);
            const float s = sx < sy ? sx : sy;
            const LONG w = static_cast<LONG>(srcDesc.Width * s + 0.5f);
            const LONG h = static_cast<LONG>(srcDesc.Height * s + 0.5f);
            const LONG x0 = eye * halfW + (halfW - w) / 2;
            const LONG y0 = (bbH - h) / 2;
            dst = { x0, y0, x0 + w, y0 + h };
        }
        else
            dst = { eye * halfW, 0, (eye + 1) * halfW, bbH };

        // D3DTEXF_NONE keeps this legal for equal-format surfaces.
        device->StretchRect(m_eyeSurf[eye], &src, bb, &dst, D3DTEXF_NONE);
    }

    static bool s_loggedComposite = false;
    if (!s_loggedComposite)
    {
        s_loggedComposite = true;
        LOG_INFO("StereoRenderer: composite OK (%ux%u -> %ldx%ld x2)", m_eyeW, m_eyeH, halfW, bbH);
    }

    bb->Release();
}

// ---------------------------------------------------------------------------
// RT0 tracking / diverted surfaces
// ---------------------------------------------------------------------------

void StereoRenderer::SetCurrentRT(IDirect3DSurface9* surf)
{
    if (surf == m_curRT) return;
    m_curRT = surf;
    m_curRTIsMain = false;
    if (!surf || m_eyeW == 0) return;

    D3DSURFACE_DESC d{};
    if (FAILED(surf->GetDesc(&d))) return;
    m_curRTIsMain = (d.Width == m_eyeW && d.Height == m_eyeH);

    // TEMP DIAG: remember small offscreen RTs (the tooltip text pass renders
    // into one of them) for the F9-off dump.
    if (m_rbLog && !m_curRTIsMain && d.Width <= 1024 && d.Height <= 1024)
        NoteSmallRT(surf);

    // Verbose-only: RT switches happen constantly; these first-20 lines exist
    // for bring-up diagnosis of "is the main target the size we expect".
    static int s_loggedRT = 0;
    if (s_loggedRT < 20)
    {
        ++s_loggedRT;
        LOG_VERB("SetCurrentRT: %p %ux%u main=%d (eye %ux%u)", surf, d.Width, d.Height,
                 m_curRTIsMain ? 1 : 0, m_eyeW, m_eyeH);
    }

    if (m_curRTIsMain && Config::Get().enableStereo &&
        surf != m_eyeSurf[0] && surf != m_eyeSurf[1] && !IsDiverted(surf))
        m_diverted.push_back(surf);
}

bool StereoRenderer::IsDiverted(IDirect3DSurface9* surf) const
{
    for (const auto* s : m_diverted)
        if (s == surf) return true;
    return false;
}

void StereoRenderer::OnGameSetTexture(DWORD stage, IDirect3DBaseTexture9* tex)
{
    // Rebuild this stage's diverted/shadow bind state.
    m_divertedBinds.erase(std::remove(m_divertedBinds.begin(), m_divertedBinds.end(), stage),
                          m_divertedBinds.end());
    for (size_t i = 0; i < m_shadowBinds.size(); ++i)
        if (m_shadowBinds[i].first == stage)
        { m_shadowBinds.erase(m_shadowBinds.begin() + i); break; }

    if (!tex) return;

    IDirect3DTexture9* tex9 = nullptr;
    if (FAILED(tex->QueryInterface(IID_IDirect3DTexture9, reinterpret_cast<void**>(&tex9))) || !tex9)
        return;

    IDirect3DSurface9* surf = nullptr;
    if (SUCCEEDED(tex9->GetSurfaceLevel(0, &surf)) && surf)
    {
        if (IsDiverted(surf))
            m_divertedBinds.push_back(stage);

        SurfaceShadow* shadow = FindShadow(surf);
        if (shadow)
        {
            if (!shadow->gameTex)
            {
                shadow->gameTex = tex9;
                tex9->AddRef();
            }
            if (shadow->gameTex == tex9)
                m_shadowBinds.push_back({ stage, shadow });
        }
        surf->Release();
    }
    tex9->Release();
}

// ---------------------------------------------------------------------------
// Matrix capture / game state tracking
// ---------------------------------------------------------------------------

void StereoRenderer::CaptureViewMatrix(const D3DMATRIX& m)
{
    // Menu-lock bookkeeping: note when the view last actually changed.
    if (!m_hasView || memcmp(&m_baseView, &m, sizeof(D3DMATRIX)) != 0)
        m_lastViewChangeTick = GetTickCount64();

    m_baseView = m;
    m_hasView  = true;
    ++m_viewCaptures;

    int limit = Config::Get().logViewMatrices;
    if (m_loggedViews < limit || (m_viewCaptures % 600) == 1)
    {
        char buf[640];
        Mat::Format(m, buf, sizeof(buf));
        LOG_INFO("ViewMatrix[%d]:%s", m_loggedViews, buf);
        ++m_loggedViews;
    }
}

void StereoRenderer::CaptureProjectionMatrix(const D3DMATRIX& m)
{
    m_baseProj = m;
    m_hasProj  = true;
}

void StereoRenderer::SetGameViewport(const D3DVIEWPORT9& vp)
{
    m_gameVp = vp;
    m_hasGameVp = true;
}

void StereoRenderer::SetGameScissorRect(const RECT& r)
{
    m_gameScissor = r;
    m_hasScissor  = true;
}

void StereoRenderer::SetScissorTestEnabled(bool enabled)
{
    m_scissorEnabled = enabled;
}

// --- M4 plumbing -------------------------------------------------------------

void StereoRenderer::CheckRecenterKey()
{
    const Config& cfg = Config::Get();
    if (!cfg.headTracking) return;

    bool down = (GetAsyncKeyState(cfg.recenterKey) & 0x8000) != 0;
    if (down && !m_recenterKeyDown)
    {
        OpenXRBridge::Recenter(); // no-op until the M3 pose stream is live
        LOG_INFO("Recenter requested");
    }
    m_recenterKeyDown = down;
}

D3DMATRIX StereoRenderer::ApplyHeadDelta(const D3DMATRIX& view)
{
    const Config& cfg = Config::Get();
    if (!cfg.headTracking) return view;

    // Menu lock: if the camera hasn't moved for 250 ms we're on a menu/map
    // screen - freezing tracking there is much more comfortable.
    if (cfg.lockInMenus && (GetTickCount64() - m_lastViewChangeTick) > 250)
        return view;

    float ypr[3] = {}, xyz[3] = {};
    if (!OpenXRBridge::GetHeadPoseDelta(ypr, xyz))
        return view; // no pose source yet (M3/M4)

    // Optional OneEuro smoothing happens where the pose is sampled (M4),
    // not here - this path must stay allocation-free and cheap.

    // camera_eye = camera_base * T(headPos) * R(headRot)
    // view_eye   = inverse(...) * view_base = R^T * T(-headPos) * view_base
    float s = cfg.worldScale;
    D3DMATRIX r    = Mat::RotationYPR(ypr[0], ypr[1], ypr[2]);
    D3DMATRIX rt   = Mat::Transpose3x3(r);
    D3DMATRIX tInv = Mat::Translation(-xyz[0] * s, -xyz[1] * s, -xyz[2] * s);
    return Mat::Multiply(Mat::Multiply(rt, tInv), view);
}

// ---------------------------------------------------------------------------
// Per-eye application
// ---------------------------------------------------------------------------

// Second flat-UI family: matrices whose 3rd AND 4th rows carry no z/w
// contribution at all (|_31|,|_32|,|_34|,|_33|,|_43| ~ 0). Warband's
// screen-space text quads (map labels, the world-map tooltip mesh) use these:
// rows 1-2 hold a pure 2D scale and the shader computes w itself, so _44 is 0
// rather than 1 and the strict IsFlatOrtho w-check rejects them. 3D views
// always have a nonzero rotation row (_31/_32), a perspective term (_34), or
// a z translation (_43), so they stay excluded.
static bool IsZeroRow34UI(const D3DMATRIX& m)
{
    const float e = 1e-3f;
    return std::fabs(m._31) < e && std::fabs(m._32) < e
        && std::fabs(m._34) < e && std::fabs(m._33) < 0.5f
        && std::fabs(m._43) < e;
}

void StereoRenderer::ApplyEye(IDirect3DDevice9* device, int eye)
{
    const Config& cfg = Config::Get();

    if (m_shadowActive)
    {
        // M2 shadow pass: per-eye shadow targets, shared shadow depth.
        device->SetRenderTarget(0, m_shadowActive->eyeSurf[eye]);
        if (m_shadowActive->depth)
            device->SetDepthStencilSurface(m_shadowActive->depth);
    }
    else
    {
        device->SetRenderTarget(0, m_eyeSurf[eye]);
        if (m_eyeDS[eye])
            device->SetDepthStencilSurface(m_eyeDS[eye]);
    }

    // Viewport: the game's intended viewport, used as-is - each eye target
    // is game-sized, so no per-eye offset is needed anywhere.
    D3DVIEWPORT9 vp;
    if (m_hasGameVp) vp = m_gameVp;
    else
    {
        vp.X = 0; vp.Y = 0;
        vp.Width  = m_eyeW;
        vp.Height = m_eyeH;
        vp.MinZ = 0.0f; vp.MaxZ = 1.0f;
    }
    device->SetViewport(&vp);

    if (cfg.noZTest)
        device->SetRenderState(D3DRS_ZENABLE, FALSE); // diagnostic: bypass depth

    // Scissor: game coords, valid as-is on a game-sized eye target.
    if (m_scissorEnabled)
    {
        RECT r = m_hasScissor ? m_gameScissor
                              : RECT{ 0, 0, static_cast<LONG>(m_eyeW), static_cast<LONG>(m_eyeH) };
        device->SetScissorRect(&r);
    }

    // Samplers bound to the ACTIVE shadow pass's surface get this eye's copy
    // (M2). ONLY during that pass: applying these binds to later main-scene
    // draws would leak offscreen content into unrelated geometry (an
    // offscreen animal portrait could end up on the ocean).
    if (m_shadowActive)
        for (const auto& bind : m_shadowBinds)
            if (bind.second == m_shadowActive)
                device->SetTexture(bind.first, bind.second->eyeTex[eye]);

    if (!m_hasView && cfg.viewSource == 0) return; // nothing captured yet; draws will match mono

    const float halfSep = (cfg.ipd * cfg.worldScale) * 0.5f;

    if (cfg.viewSource == 0)
    {
        // M4: head pose delta composes onto the captured view first.
        D3DMATRIX headView = ApplyHeadDelta(m_baseView);

        // Fixed-function UI is often drawn with an IDENTITY view matrix and
        // an orthographic projection. A view-matrix-only classifier cannot
        // see that case: identity is a valid 3D view, so it was eye-shifted,
        // and under an ortho projection a view translation is a pure
        // screen-space offset - dialog text moved relative to backgrounds
        // that the flat path drew correctly (the encounter-dialog layout
        // bug). Gate on the PROJECTION type instead: only perspective scenes
        // get the eye view shift; orthographic/2D projections stay flat in
        // both eyes. With no projection captured yet, keep the old heuristic.
        const bool flatUI = m_hasProj
            ? !Mat::IsPerspective(m_baseProj)
            : Mat::IsFlatOrtho(headView, cfg.uiZScaleMax, cfg.uiWScaleEps, cfg.uiRowNormMax);
        D3DMATRIX eyeView  = flatUI ? headView : Mat::EyeView(headView, halfSep, eye);

        // Fixed-function path: overwrite the VIEW transform for this eye.
        device->SetTransform(D3DTS_VIEW, &eyeView);

        // M2 correctness: off-center projection replaces toe-in distortion.
        if (cfg.asymmetricProjection && m_hasProj)
        {
            D3DMATRIX eyeProj = Mat::EyeProjection(m_baseProj, halfSep, eye);
            device->SetTransform(D3DTS_PROJECTION, &eyeProj);
        }
    }
    else
    {
        // Shader path: read back the game's ACTUAL current register - never
        // trust a captured matrix. UI passes often don't re-upload before
        // every draw, so any captured value can stamp the wrong transform
        // on the wrong element (misaligned/reordered text). The readback is
        // taken once per eye pair (eye 1 would otherwise read eye 0's
        // already-shifted upload).
        if (eye == 0 || !m_readbackValid)
        {
            float cur[16] = {};
            if (FAILED(device->GetVertexShaderConstantF(cfg.vsViewRegister, cur, 4)))
                return;
            memcpy(&m_lastReadback, cur, sizeof(m_lastReadback));
            m_readbackValid = true;

            // Fail-safe for a wrong VSViewRegister guess: a garbage register
            // (NaN/Inf/zero/absurd) shifted per eye is far worse than mono,
            // so render this pair unshifted instead.
            m_readbackPlausible = Mat::IsPlausibleTransform(m_lastReadback);
            if (!m_readbackPlausible && !m_loggedImplausible)
            {
                char buf[256];
                Mat::Format(m_lastReadback, buf, sizeof(buf));
                LOG_WARN("VSViewRegister c%d readback implausible%s -- "
                         "rendering mono this pair (check [Stereo] VSViewRegister)",
                         cfg.vsViewRegister, buf);
                m_loggedImplausible = true;
            }
            if (m_rbLog) // TEMP DIAG: every pair's actual register (F9 toggles)
            {
                const D3DMATRIX& rm = m_lastReadback;
                const bool flat = !m_readbackPlausible
                               || Mat::IsFlatOrtho(rm, cfg.uiZScaleMax, cfg.uiWScaleEps,
                                                   cfg.uiRowNormMax);
                D3DVIEWPORT9 vpNow{};
                device->GetViewport(&vpNow);
                IDirect3DVertexShader9* vs = nullptr;
                device->GetVertexShader(&vs);
                LOG_INFO("RB pc=%d flat=%d plaus=%d vs=%p t=%.3f,%.3f,%.3f m33=%.3f m44=%.3f vp=%u,%u %ux%u gamevp=%u,%u %ux%u",
                         m_logPrimCount, flat ? 1 : 0, m_readbackPlausible ? 1 : 0,
                         static_cast<void*>(vs),
                         rm._41, rm._42, rm._43, rm._33, rm._44,
                         vpNow.X, vpNow.Y, vpNow.Width, vpNow.Height,
                         m_gameVp.X, m_gameVp.Y, m_gameVp.Width, m_gameVp.Height);
                if (vs) vs->Release();
            }
        }

        const D3DMATRIX& m = m_lastReadback;
        const bool flatUI = !m_readbackPlausible
                         || Mat::IsFlatOrtho(m, cfg.uiZScaleMax, cfg.uiWScaleEps,
                                             cfg.uiRowNormMax)
                         || IsZeroRow34UI(m);
        m_uploadedThisPair = false;
        if (!flatUI)
        {
            D3DMATRIX headView = ApplyHeadDelta(m);
            D3DMATRIX eyeView  = Mat::EyeView(headView, halfSep, eye);
            device->SetVertexShaderConstantF(cfg.vsViewRegister,
                                             reinterpret_cast<const float*>(&eyeView), 4);
            m_uploadedThisPair = true;
        }
    }
}

void StereoRenderer::RestoreGameState(IDirect3DDevice9* device)
{
    if (Config::Get().noZTest)
        device->SetRenderState(D3DRS_ZENABLE, TRUE); // restore after diagnostic
    if (m_hasGameVp)
        device->SetViewport(&m_gameVp);
    if (m_scissorEnabled && m_hasScissor)
        device->SetScissorRect(&m_gameScissor);
    // Restore the game's own texture for the ACTIVE shadow's stages only.
    // Every bind in m_shadowBinds has gameTex set at creation, so there is
    // no legitimate fallback to an eye copy - substituting one would leak
    // our render into the game's subsequent draws.
    if (m_shadowActive)
        for (const auto& bind : m_shadowBinds)
            if (bind.second == m_shadowActive && bind.second->gameTex)
                device->SetTexture(bind.first, bind.second->gameTex);
    if (m_uploadedThisPair && Config::Get().viewSource == 1)
    {
        // Put the game's own register back exactly as we found it.
        device->SetVertexShaderConstantF(Config::Get().vsViewRegister,
                                         reinterpret_cast<const float*>(&m_lastReadback), 4);
        m_uploadedThisPair = false;
    }
    if (m_hasView && Config::Get().viewSource == 0)
        device->SetTransform(D3DTS_VIEW, &m_baseView);
    if (m_hasProj && Config::Get().asymmetricProjection && Config::Get().viewSource == 0)
        device->SetTransform(D3DTS_PROJECTION, &m_baseProj);
}

void StereoRenderer::AddDuplicationTime(double microseconds, int drawCalls)
{
    m_dupTimeUs += microseconds;
    m_dupCalls  += drawCalls;
}

// ---------------------------------------------------------------------------
// Offscreen shadow passes (M2 - dormant unless Config::offscreenPasses)
// ---------------------------------------------------------------------------

StereoRenderer::SurfaceShadow* StereoRenderer::FindShadow(IDirect3DSurface9* gameSurf)
{
    for (auto& s : m_shadows)
        if (s.gameSurf == gameSurf) return &s;
    return nullptr;
}

StereoRenderer::SurfaceShadow* StereoRenderer::BeginShadowPass(IDirect3DDevice9* device,
                                                               IDirect3DSurface9* gameSurf)
{
    SurfaceShadow* shadow = FindShadow(gameSurf);
    if (shadow && shadow->eyeSurf[0] && shadow->eyeSurf[1])
        return shadow; // already have per-eye copies for this surface

    if (!shadow)
    {
        m_shadows.push_back(SurfaceShadow{});
        shadow = &m_shadows.back();
        shadow->gameSurf = gameSurf;
        gameSurf->AddRef();
    }

    D3DSURFACE_DESC desc{};
    if (FAILED(gameSurf->GetDesc(&desc))) return nullptr;

    // Clean slate: a previous attempt may have left partial resources.
    for (int eye = 0; eye < 2; ++eye)
    {
        if (shadow->eyeSurf[eye]) { shadow->eyeSurf[eye]->Release(); shadow->eyeSurf[eye] = nullptr; }
        if (shadow->eyeTex[eye])  { shadow->eyeTex[eye]->Release();  shadow->eyeTex[eye] = nullptr; }
    }

    for (int eye = 0; eye < 2; ++eye)
    {
        IDirect3DTexture9* tex = nullptr;
        HRESULT hr = device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET,
                                           desc.Format, D3DPOOL_DEFAULT, &tex, nullptr);
        if (FAILED(hr))
        {
            LOG_WARN("StereoRenderer: shadow texture %ux%u fmt=%d failed hr=0x%08X - pass stays mono",
                     desc.Width, desc.Height, desc.Format, hr);
            return nullptr;
        }
        shadow->eyeTex[eye] = tex;
        if (FAILED(tex->GetSurfaceLevel(0, &shadow->eyeSurf[eye])))
        {
            LOG_WARN("StereoRenderer: shadow GetSurfaceLevel failed - pass stays mono");
            return nullptr;
        }
    }
    return shadow;
}

bool StereoRenderer::ActivateShadowPass(IDirect3DDevice9* device, IDirect3DSurface9* gameSurf)
{
    if (!device || !gameSurf || gameSurf == m_eyeSurf[0] || gameSurf == m_eyeSurf[1])
    {
        EndShadowPass();
        return false;
    }

    D3DSURFACE_DESC desc{};
    if (FAILED(gameSurf->GetDesc(&desc)) ||
        (desc.Width == m_eyeW && desc.Height == m_eyeH))
    {
        EndShadowPass();
        return false;
    }

    SurfaceShadow* shadow = BeginShadowPass(device, gameSurf);
    if (!shadow)
    {
        EndShadowPass();
        return false;
    }

    // Match the pass depth format/size while keeping one shared depth buffer;
    // the two eye color targets are the parts later sampled independently.
    IDirect3DSurface9* gameDS = nullptr;
    if (SUCCEEDED(device->GetDepthStencilSurface(&gameDS)) && gameDS)
    {
        if (!EnsureShadowDepth(device, *shadow, gameDS))
        {
            gameDS->Release();
            EndShadowPass();
            return false;
        }
        gameDS->Release();
    }

    m_shadowActive = shadow;
    return true;
}

void StereoRenderer::EndShadowPass()
{
    m_shadowActive = nullptr;
}

void StereoRenderer::ReleaseShadow(SurfaceShadow& s)
{
    if (s.gameSurf) { s.gameSurf->Release(); s.gameSurf = nullptr; }
    if (s.gameTex)  { s.gameTex->Release();  s.gameTex = nullptr; }
    if (s.depth)    { s.depth->Release();    s.depth = nullptr; }
    for (int eye = 0; eye < 2; ++eye)
    {
        if (s.eyeSurf[eye]) { s.eyeSurf[eye]->Release(); s.eyeSurf[eye] = nullptr; }
        if (s.eyeTex[eye])  { s.eyeTex[eye]->Release();  s.eyeTex[eye] = nullptr; }
    }
}

void StereoRenderer::ReleaseAllShadows()
{
    for (auto& s : m_shadows) ReleaseShadow(s);
    m_shadows.clear();
    m_shadowBinds.clear();
    m_shadowActive = nullptr;
}

bool StereoRenderer::EnsureShadowDepth(IDirect3DDevice9* device, SurfaceShadow& s,
                                       IDirect3DSurface9* gameDS)
{
    D3DSURFACE_DESC want{};
    if (FAILED(gameDS->GetDesc(&want))) return false;

    if (s.depth)
    {
        D3DSURFACE_DESC have{};
        s.depth->GetDesc(&have);
        if (have.Format == want.Format && have.Width == want.Width && have.Height == want.Height)
            return true;
        s.depth->Release();
        s.depth = nullptr;
    }

    HRESULT hr = device->CreateDepthStencilSurface(want.Width, want.Height, want.Format,
                                                   D3DMULTISAMPLE_NONE, 0, TRUE,
                                                   &s.depth, nullptr);
    if (FAILED(hr))
    {
        LOG_WARN("StereoRenderer: shadow depth %ux%u fmt=%d failed hr=0x%08X",
                 want.Width, want.Height, want.Format, hr);
        return false;
    }
    return true;
}

HRESULT StereoRenderer::ClearBothEyes(IDirect3DDevice9* device, DWORD count, const D3DRECT* rects,
                                      DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{
    IDirect3DSurface9* savedRT = nullptr;
    IDirect3DSurface9* savedDS = nullptr;
    device->GetRenderTarget(0, &savedRT);
    device->GetDepthStencilSurface(&savedDS);

    HRESULT hr = S_OK;
    for (int eye = 0; eye < 2; ++eye)
    {
        device->SetRenderTarget(0, m_shadowActive ? m_shadowActive->eyeSurf[eye] : m_eyeSurf[eye]);
        if (!m_shadowActive && m_eyeDS[eye])
            device->SetDepthStencilSurface(m_eyeDS[eye]); // depth clears must hit the eye DS
        HRESULT r = device->Clear(count, rects, flags, color, z, stencil);
        if (FAILED(r)) hr = r;
    }

    // Leave the device's game-visible RT/DS exactly as the game set it.
    device->SetRenderTarget(0, savedRT);
    device->SetDepthStencilSurface(savedDS);
    if (savedRT) savedRT->Release();
    if (savedDS) savedDS->Release();
    return hr;
}

// ---------------------------------------------------------------------------
// Debug: dump eye surfaces to BMPs
// ---------------------------------------------------------------------------

static void WriteBmp(const char* path, UINT w, UINT h, LONG pitch,
                     const unsigned char* bits)
{
    FILE* f = nullptr;
    fopen_s(&f, path, "wb");
    if (!f) return;
    #pragma pack(push, 1)
    struct { char id[2]; unsigned size, r1, off; unsigned hdr, w, h; unsigned short planes, bpp;
             unsigned comp, img, xppm, yppm, colors, important; } hdr = {};
    #pragma pack(pop)
    const unsigned pixelBytes = w * 3 * h;
    hdr.id[0] = 'B'; hdr.id[1] = 'M';
    hdr.off = 54;
    hdr.size = 54 + pixelBytes;
    hdr.hdr = 40; hdr.w = w; hdr.h = h;
    hdr.planes = 1; hdr.bpp = 24;
    hdr.img = pixelBytes;
    fwrite(&hdr, 1, 54, f);
    for (int y = (int)h - 1; y >= 0; --y)
    {
        const unsigned char* srcRow = bits + (size_t)y * pitch;
        unsigned char row[4096 * 3];
        for (UINT x0 = 0; x0 < w; x0 += 4096)
        {
            UINT n = (w - x0 < 4096) ? (w - x0) : 4096;
            for (UINT x = 0; x < n; ++x)
            {
                row[x * 3 + 0] = srcRow[(x0 + x) * 4 + 0];
                row[x * 3 + 1] = srcRow[(x0 + x) * 4 + 1];
                row[x * 3 + 2] = srcRow[(x0 + x) * 4 + 2];
            }
            fwrite(row, 1, n * 3, f);
        }
    }
    fclose(f);
}

void StereoRenderer::DumpStereoSurface(IDirect3DDevice9* device, int index)
{
    char dllDir[MAX_PATH] = {};
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&WriteBmp), &self); // any this-module address
    GetModuleFileNameA(self, dllDir, MAX_PATH);
    char* slash = strrchr(dllDir, '\\');
    if (slash) slash[1] = '\0';

    for (int eye = 0; eye < 2; ++eye)
    {
        if (!m_eyeSurf[eye]) continue;
        D3DSURFACE_DESC desc{};
        m_eyeSurf[eye]->GetDesc(&desc);

        IDirect3DSurface9* sys = nullptr;
        HRESULT hr = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                         D3DPOOL_SYSTEMMEM, &sys, nullptr);
        if (FAILED(hr) || !sys) continue;
        hr = device->GetRenderTargetData(m_eyeSurf[eye], sys);
        if (SUCCEEDED(hr))
        {
            D3DLOCKED_RECT lr{};
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY)))
            {
                char path[MAX_PATH];
                snprintf(path, MAX_PATH, "%.230sWarbandVR_frame%03d_%c.bmp", dllDir, index,
                         eye == 0 ? 'L' : 'R');
                WriteBmp(path, desc.Width, desc.Height, lr.Pitch,
                         static_cast<const unsigned char*>(lr.pBits));
                LOG_INFO("DumpStereoSurface: wrote %s", path);
                sys->UnlockRect();
            }
        }
        sys->Release();
    }
}

// ---------------------------------------------------------------------------
// TEMP DIAG: tooltip text texture dump (F9-gated, once per toggle)
// ---------------------------------------------------------------------------

void StereoRenderer::NoteZen0Texture(IDirect3DBaseTexture9* tex)
{
    if (tex == m_zen0Tex) return;
    if (tex) tex->AddRef();
    if (m_zen0Tex) m_zen0Tex->Release();
    m_zen0Tex = tex;
}

void StereoRenderer::NoteZen1Texture(IDirect3DBaseTexture9* tex)
{
    if (tex == m_zen1Tex) return;
    if (tex) tex->AddRef();
    if (m_zen1Tex) m_zen1Tex->Release();
    m_zen1Tex = tex;
}

namespace
{
    void DumpOneTexture(IDirect3DBaseTexture9* t, const char* name)
    {
        if (!t) return;
        IDirect3DTexture9* tex = nullptr;
        if (FAILED(t->QueryInterface(IID_IDirect3DTexture9, reinterpret_cast<void**>(&tex))) || !tex)
            return;
        D3DSURFACE_DESC td{};
        tex->GetLevelDesc(0, &td);
        HMODULE d3dx = GetModuleHandleA("d3dx9_42.dll");
        typedef HRESULT (WINAPI* SaveTex_t)(LPCSTR, UINT, IDirect3DBaseTexture9*, const void*);
        SaveTex_t saveTex = d3dx
            ? reinterpret_cast<SaveTex_t>(GetProcAddress(d3dx, "D3DXSaveTextureToFileA"))
            : nullptr;
        HRESULT hr = E_FAIL;
        if (saveTex)
        {
            char dllDir[MAX_PATH] = {};
            HMODULE self = nullptr;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(&WriteBmp), &self);
            GetModuleFileNameA(self, dllDir, MAX_PATH);
            char* slash = strrchr(dllDir, '\\');
            if (slash) slash[1] = '\0';
            char path[MAX_PATH];
            snprintf(path, MAX_PATH, "%.230sWarbandVR_%s.tga", dllDir, name);
            hr = saveTex(path, 2 /*D3DXIFF_TGA*/, tex, nullptr); // NOLINT
        }
        LOG_INFO("DIAG: %s tex=%p %ux%u fmt=%d pool=%d hr=0x%08X",
                 name, static_cast<void*>(t), td.Width, td.Height, td.Format, td.Pool, hr);
        tex->Release();
    }
}

void StereoRenderer::NoteSmallRT(IDirect3DSurface9* surf)
{
    for (const auto* s : m_smallRTs)
        if (s == surf) return;
    if (m_smallRTs.size() >= 12) return;
    surf->AddRef();
    m_smallRTs.push_back(surf);
}

void StereoRenderer::ShowSmallRTsOnScreen(IDirect3DDevice9* device)
{
    if (m_smallRTs.empty()) return;
    IDirect3DSurface9* bb = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb)
        return;

    // Save every state we touch; leaking any of these destabilizes the game.
    IDirect3DSurface9* savedRT = nullptr;
    IDirect3DBaseTexture9* savedTex = nullptr;
    IDirect3DPixelShader9* savedPS = nullptr;
    IDirect3DVertexShader9* savedVS = nullptr;
    DWORD savedFVF = 0, savedZen = 0, savedZw = 0, savedAb = 0, savedCop = 0, savedCarg1 = 0;
    device->GetRenderTarget(0, &savedRT);
    device->GetTexture(0, &savedTex);
    device->GetPixelShader(&savedPS);
    device->GetVertexShader(&savedVS);
    device->GetFVF(&savedFVF);
    device->GetRenderState(D3DRS_ZENABLE, &savedZen);
    device->GetRenderState(D3DRS_ZWRITEENABLE, &savedZw);
    device->GetRenderState(D3DRS_ALPHABLENDENABLE, &savedAb);
    device->GetTextureStageState(0, D3DTSS_COLOROP, &savedCop);
    device->GetTextureStageState(0, D3DTSS_COLORARG1, &savedCarg1);

    device->SetRenderTarget(0, bb);
    D3DVIEWPORT9 fullVp = { 0, 0, m_eyeW, m_eyeH, 0.0f, 1.0f };
    device->SetViewport(&fullVp);
    device->SetPixelShader(nullptr);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

    struct V { float x, y, z, rhw, u, v; };
    int i = 0;
    HRESULT hrFirst = S_OK;
    for (auto* surf : m_smallRTs)
    {
        IDirect3DTexture9* tex = nullptr;
        if (SUCCEEDED(surf->GetContainer(IID_IDirect3DTexture9, reinterpret_cast<void**>(&tex))) && tex)
        {
            device->SetTexture(0, tex);
            const float x0 = 10.0f + (i % 8) * 266.0f;
            const float y0 = 10.0f + (i / 8) * 266.0f;
            const V quad[4] = {
                { x0,        y0,        0.0f, 1.0f, 0.0f, 0.0f },
                { x0 + 256,  y0,        0.0f, 1.0f, 1.0f, 0.0f },
                { x0,        y0 + 256,  0.0f, 1.0f, 0.0f, 1.0f },
                { x0 + 256,  y0 + 256,  0.0f, 1.0f, 1.0f, 1.0f },
            };
            const HRESULT hr = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(V));
            if (FAILED(hr) && SUCCEEDED(hrFirst)) hrFirst = hr;
            tex->Release();
        }
        ++i;
    }
    static bool s_loggedOverlay = false;
    if (!s_loggedOverlay)
    {
        s_loggedOverlay = true;
        LOG_INFO("DIAG: overlay %d RTs, first draw hr=0x%08X", (int)m_smallRTs.size(), hrFirst);
    }

    device->SetTexture(0, savedTex);
    device->SetPixelShader(savedPS);
    device->SetVertexShader(savedVS);
    device->SetFVF(savedFVF);
    device->SetRenderState(D3DRS_ZENABLE, savedZen);
    device->SetRenderState(D3DRS_ZWRITEENABLE, savedZw);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, savedAb);
    device->SetTextureStageState(0, D3DTSS_COLOROP, savedCop);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, savedCarg1);
    device->SetRenderTarget(0, savedRT);
    if (savedTex) savedTex->Release();
    if (savedPS) savedPS->Release();
    if (savedVS) savedVS->Release();
    if (savedRT) savedRT->Release();
    bb->Release();
}

void StereoRenderer::DumpNextSmallRT(IDirect3DDevice9* device)
{
    if (m_smallRTs.empty()) return;
    IDirect3DSurface9* surf = m_smallRTs[m_rtDumpIdx % m_smallRTs.size()];
    const int idx = static_cast<int>(m_rtDumpIdx % m_smallRTs.size());
    ++m_rtDumpIdx;

    D3DSURFACE_DESC d{};
    surf->GetDesc(&d);
    IDirect3DSurface9* sys = nullptr;
    HRESULT hr = device->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format,
                                                     D3DPOOL_SYSTEMMEM, &sys, nullptr);
    if (SUCCEEDED(hr) && sys)
        hr = device->GetRenderTargetData(surf, sys);
    if (SUCCEEDED(hr) && sys)
    {
        D3DLOCKED_RECT lr{};
        if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY)))
        {
            char dllDir[MAX_PATH] = {};
            HMODULE self = nullptr;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(&WriteBmp), &self);
            GetModuleFileNameA(self, dllDir, MAX_PATH);
            char* slash = strrchr(dllDir, '\\');
            if (slash) slash[1] = '\0';
            char path[MAX_PATH];
            snprintf(path, MAX_PATH, "%.230sWarbandVR_smallrt_%d.bmp", dllDir, idx);
            WriteBmp(path, d.Width, d.Height, lr.Pitch,
                     static_cast<const unsigned char*>(lr.pBits));
            LOG_INFO("DIAG: smallrt_%d %p %ux%u fmt=%d written", idx,
                     static_cast<void*>(surf), d.Width, d.Height, d.Format);
            sys->UnlockRect();
        }
    }
    if (sys) sys->Release();
}

void StereoRenderer::DumpNotedZen0Texture(IDirect3DDevice9* device)
{
    DumpOneTexture(m_zen0Tex, "tooltip_fg_tex");
    DumpOneTexture(m_zen1Tex, "tooltip_line_tex");

    // TEMP DIAG: write every small offscreen RT seen while logging to a BMP
    // (runs at present time - mid-scene readbacks reset the device).
    int idx = 0;
    for (auto* surf : m_smallRTs)
    {
        D3DSURFACE_DESC d{};
        surf->GetDesc(&d);
        IDirect3DSurface9* sys = nullptr;
        HRESULT hr = device
            ? device->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format,
                                                  D3DPOOL_SYSTEMMEM, &sys, nullptr)
            : E_FAIL;
        if (SUCCEEDED(hr) && sys)
            hr = device->GetRenderTargetData(surf, sys);
        if (SUCCEEDED(hr) && sys)
        {
            D3DLOCKED_RECT lr{};
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY)))
            {
                char dllDir[MAX_PATH] = {};
                HMODULE self = nullptr;
                GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   reinterpret_cast<LPCSTR>(&WriteBmp), &self);
                GetModuleFileNameA(self, dllDir, MAX_PATH);
                char* slash = strrchr(dllDir, '\\');
                if (slash) slash[1] = '\0';
                char path[MAX_PATH];
                snprintf(path, MAX_PATH, "%.230sWarbandVR_smallrt_%d.bmp", dllDir, idx);
                WriteBmp(path, d.Width, d.Height, lr.Pitch,
                         static_cast<const unsigned char*>(lr.pBits));
                LOG_INFO("DIAG: smallrt_%d %p %ux%u fmt=%d written", idx,
                         static_cast<void*>(surf), d.Width, d.Height, d.Format);
                sys->UnlockRect();
            }
        }
        if (sys) sys->Release();
        surf->Release();
        ++idx;
    }
    m_smallRTs.clear();
}

void StereoRenderer::DumpBackbuffer(IDirect3DDevice9* device, int index)
{
    IDirect3DSurface9* bb = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb)
        return;
    D3DSURFACE_DESC desc{};
    bb->GetDesc(&desc);

    IDirect3DSurface9* sys = nullptr;
    HRESULT hr = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                     D3DPOOL_SYSTEMMEM, &sys, nullptr);
    if (SUCCEEDED(hr) && sys)
    {
        hr = device->GetRenderTargetData(bb, sys);
        if (SUCCEEDED(hr))
        {
            D3DLOCKED_RECT lr{};
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY)))
            {
                char dllDir[MAX_PATH] = {};
                HMODULE self = nullptr;
                GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   reinterpret_cast<LPCSTR>(&WriteBmp), &self);
                GetModuleFileNameA(self, dllDir, MAX_PATH);
                char* slash = strrchr(dllDir, '\\');
                if (slash) slash[1] = '\0';
                char path[MAX_PATH];
                snprintf(path, MAX_PATH, "%.230sWarbandVR_frame%03d_BB.bmp", dllDir, index);
                WriteBmp(path, desc.Width, desc.Height, lr.Pitch,
                         static_cast<const unsigned char*>(lr.pBits));
                LOG_INFO("DumpBackbuffer: wrote %s", path);
                sys->UnlockRect();
            }
        }
    }
    if (sys) sys->Release();
    bb->Release();
}
