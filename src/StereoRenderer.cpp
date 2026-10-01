#include "StereoRenderer.h"
#include "Config.h"
#include "Logger.h"
#include "MathUtil.h"
#include "Capture.h"
#include "OpenXRBridge.h"
#include "OneEuroFilter.h"

#include <cstring>

StereoRenderer& StereoRenderer::Get()
{
    static StereoRenderer s;
    return s;
}

void StereoRenderer::OnDeviceCreated(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS* pp)
{
    m_device = device;
    if (pp) m_pp = *pp;
    if (m_qpcFreq.QuadPart == 0) QueryPerformanceFrequency(&m_qpcFreq);
    ReleaseResources();
    if (Config::Get().enableStereo)
        CreateResources(device);
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
    ReleaseResources();
    m_device = nullptr;
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

    D3DFORMAT fmt = (m_pp.BackBufferFormat != D3DFMT_UNKNOWN) ? m_pp.BackBufferFormat : D3DFMT_X8R8G8B8;
    HRESULT hr = device->CreateRenderTarget(w * 2, h, fmt, D3DMULTISAMPLE_NONE, 0,
                                            FALSE, &m_stereoSurf, nullptr);
    if (FAILED(hr))
    {
        LOG_ERROR("StereoRenderer: CreateRenderTarget(%ux%u) failed hr=0x%08X", w * 2, h, hr);
        m_stereoSurf = nullptr;
        return;
    }
    LOG_INFO("StereoRenderer: stereo target %ux%u created", w * 2, h);
}

void StereoRenderer::ReleaseResources()
{
    ReleaseAllShadows();
    if (m_gameDS)     { m_gameDS->Release();     m_gameDS = nullptr; }
    if (m_stereoDS)   { m_stereoDS->Release();   m_stereoDS = nullptr; }
    if (m_gameRT)     { m_gameRT->Release();     m_gameRT = nullptr; }
    if (m_stereoSurf) { m_stereoSurf->Release(); m_stereoSurf = nullptr; }
}

// Creates the double-width depth-stencil paired with the stereo target.
// D3D9 rejects SetRenderTarget when the bound depth-stencil is smaller than
// the render target, so without this the substitution fails outright.
bool StereoRenderer::CreateStereoDepth(IDirect3DDevice9* device)
{
    if (m_stereoDS || !m_stereoSurf) return m_stereoDS != nullptr;

    IDirect3DSurface9* gameDS = nullptr;
    if (FAILED(device->GetDepthStencilSurface(&gameDS)) || !gameDS)
        return false; // game renders depthless; no substitution needed

    D3DSURFACE_DESC dsDesc{};
    gameDS->GetDesc(&dsDesc);
    gameDS->Release();

    D3DSURFACE_DESC rtDesc{};
    m_stereoSurf->GetDesc(&rtDesc);

    HRESULT hr = device->CreateDepthStencilSurface(rtDesc.Width, rtDesc.Height, dsDesc.Format,
                                                   D3DMULTISAMPLE_NONE, 0, TRUE,
                                                   &m_stereoDS, nullptr);
    if (FAILED(hr))
    {
        LOG_ERROR("StereoRenderer: CreateDepthStencilSurface(%ux%u fmt=%d) failed hr=0x%08X",
                  rtDesc.Width, rtDesc.Height, dsDesc.Format, hr);
        m_stereoDS = nullptr;
        return false;
    }
    LOG_INFO("StereoRenderer: stereo depth %ux%u fmt=%d created",
             rtDesc.Width, rtDesc.Height, dsDesc.Format);
    return true;
}

int StereoRenderer::StereoHalfWidth() const
{
    if (!m_stereoSurf) return 0;
    D3DSURFACE_DESC desc{};
    m_stereoSurf->GetDesc(&desc);
    return static_cast<int>(desc.Width / 2);
}

void StereoRenderer::OnBeginScene(IDirect3DDevice9* device)
{
    m_inScene = true;
    CheckRecenterKey();
    if (!Config::Get().enableStereo) return;
    if (!m_stereoSurf) { CreateResources(device); if (!m_stereoSurf) return; }

    // Remember the game's current RT0 and substitute our stereo target.
    // NOTE: if the game later re-points RT0 itself (post-processing passes),
    // that is routed through HandleSetRenderTarget - see docs/01_Design.md
    // "render target redirection".
    if (!m_gameRT && SUCCEEDED(device->GetRenderTarget(0, &m_gameRT)) && m_gameRT)
    {
        if (Capture::TraceActive())
        {
            D3DSURFACE_DESC d{};
            m_gameRT->GetDesc(&d);
            Capture::Trace("OnBeginScene: substituting (gameRT=%p %ux%u)", m_gameRT, d.Width, d.Height);
        }
        if (!CreateStereoDepth(device))
            LOG_WARN("StereoRenderer: no stereo depth buffer; SetRenderTarget may fail");

        // Remember the game's depth-stencil so OnPresent can restore it.
        IDirect3DSurface9* curDS = nullptr;
        if (SUCCEEDED(device->GetDepthStencilSurface(&curDS)) && curDS)
            m_gameDS = curDS; // keeps the Get ref

        if (SUCCEEDED(device->SetRenderTarget(0, m_stereoSurf)))
        {
            if (m_stereoDS)
                device->SetDepthStencilSurface(m_stereoDS);
            m_active = true;
        }
        else
        {
            LOG_WARN("StereoRenderer: SetRenderTarget substitution failed");
        }
    }
}

void StereoRenderer::OnEndScene(IDirect3DDevice9* device)
{
    (void)device;
    m_inScene = false;

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

void StereoRenderer::OnPresent(IDirect3DDevice9* device)
{
    if (!m_active || !m_stereoSurf) return;

    EndShadowPass(); // in case the game left a pass open at frame end

    if (Config::Get().dumpFrames > 0 && (m_presentCount++ % Config::Get().dumpFrames) == 0)
        DumpStereoSurface(device, m_dumpedFrames++);

    if (Config::Get().outputMode == 1)
    {
        // M3: hand the stereo frame to OpenXR instead of the monitor.
        OpenXRBridge::SubmitFrame(device, m_stereoSurf);
    }
    else
    {
        CompositeToBackbuffer(device);
    }

    // Restore the game's RT0 and depth so the next frame starts clean.
    if (m_gameRT)
    {
        device->SetRenderTarget(0, m_gameRT);
        m_gameRT->Release();
        m_gameRT = nullptr;
    }
    if (m_gameDS)
    {
        device->SetDepthStencilSurface(m_gameDS);
        m_gameDS->Release();
        m_gameDS = nullptr;
    }
    m_active = false;
}

void StereoRenderer::CompositeToBackbuffer(IDirect3DDevice9* device)
{
    IDirect3DSurface9* bb = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb)
        return;

    D3DSURFACE_DESC desc{};
    m_stereoSurf->GetDesc(&desc);
    const LONG halfW = static_cast<LONG>(desc.Width / 2);
    const LONG h     = static_cast<LONG>(desc.Height);

    D3DSURFACE_DESC bbDesc{};
    bb->GetDesc(&bbDesc);
    const LONG bbHalfW = static_cast<LONG>(bbDesc.Width / 2);
    const LONG bbH     = static_cast<LONG>(bbDesc.Height);

    RECT srcL = { 0,     0, halfW,        h };
    RECT srcR = { halfW, 0, halfW * 2,    h };
    RECT dstL = { 0,       0, bbHalfW,     bbH };
    RECT dstR = { bbHalfW, 0, bbHalfW * 2, bbH };

    // D3DTEXF_NONE keeps this legal for equal-format surfaces.
    HRESULT hrL = device->StretchRect(m_stereoSurf, &srcL, bb, &dstL, D3DTEXF_NONE);
    HRESULT hrR = device->StretchRect(m_stereoSurf, &srcR, bb, &dstR, D3DTEXF_NONE);
    static bool s_loggedComposite = false;
    if (!s_loggedComposite)
    {
        s_loggedComposite = true;
        if (FAILED(hrL) || FAILED(hrR))
            LOG_ERROR("StereoRenderer: composite StretchRect failed hrL=0x%08X hrR=0x%08X", hrL, hrR);
        else
            LOG_INFO("StereoRenderer: composite OK (%ldx%ld -> %ldx%ld x2)",
                     halfW, h, bbHalfW, bbH);
    }

    bb->Release();
}

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

// --- M4 plumbing -----------------------------------------------------------

void StereoRenderer::CheckRecenterKey()
{
    const Config& cfg = Config::Get();
    if (!cfg.headTracking) return;

    bool down = (GetAsyncKeyState(cfg.recenterKey) & 0x8000) != 0;
    if (down && !m_recenterKeyDown)
        LOG_INFO("Recenter key pressed - (pose reference reset lands with the M3 pose source)");
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

void StereoRenderer::ApplyEye(IDirect3DDevice9* device, int eye)
{
    const Config& cfg = Config::Get();

    // Viewport: derived from the game's most recent intended viewport
    // (tracked by hkSetViewport); falls back to querying the device.
    D3DVIEWPORT9 vp;
    if (m_hasGameVp) vp = m_gameVp;
    else if (FAILED(device->GetViewport(&vp))) return;

    if (m_shadowActive)
    {
        // Per-eye shadow targets are game-sized: full viewport, no offset.
        device->SetRenderTarget(0, m_shadowActive->eyeSurf[eye]);
        if (m_shadowActive->depth)
            device->SetDepthStencilSurface(m_shadowActive->depth);
    }
    else
    {
        vp.X = (eye == 0) ? 0 : vp.Width; // stereo target is 2x width
    }
    device->SetViewport(&vp);

    // Scissor: same coordinate space as the target, so offset per eye.
    if (m_scissorEnabled && m_hasScissor)
    {
        RECT r = m_gameScissor;
        const LONG off = ActiveEyeOffset();
        if (eye == 1) { r.left += off; r.right += off; }
        device->SetScissorRect(&r);
    }

    // Samplers bound to a shadowed surface get this eye's copy.
    for (const auto& bind : m_shadowBinds)
        device->SetTexture(bind.first, bind.second->eyeTex[eye]);

    if (!m_hasView) return; // nothing captured yet; draws will match mono

    const float halfSep = (cfg.ipd * cfg.worldScale) * 0.5f;

    // M4: head pose delta composes onto the captured view first.
    D3DMATRIX headView = ApplyHeadDelta(m_baseView);
    D3DMATRIX eyeView  = Mat::EyeView(headView, halfSep, eye);

    if (cfg.viewSource == 0)
    {
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
        // Shader path: overwrite the register holding the view (or viewproj).
        // TODO(M0/M2): if VSMatrixIsViewProj, eyeView must be multiplied by the
        // captured projection matrix (asymmetric variant when enabled) here
        // instead of sent raw.
        if (m_loggedEyes < 20) // TEMP DIAG: what do we actually upload per eye?
        {
            char buf[640];
            Mat::Format(eyeView, buf, sizeof(buf));
            LOG_INFO("EyeMat eye=%d base._41=%.3f,%s", eye, m_baseView._41, buf);
            ++m_loggedEyes;
        }
        device->SetVertexShaderConstantF(cfg.vsViewRegister,
                                         reinterpret_cast<const float*>(&eyeView), 4);
    }
}

void StereoRenderer::RestoreGameState(IDirect3DDevice9* device)
{
    if (m_shadowActive)
        device->SetRenderTarget(0, m_shadowActive->eyeSurf[0]);
    if (m_hasGameVp)
        device->SetViewport(&m_gameVp);
    if (m_scissorEnabled && m_hasScissor)
        device->SetScissorRect(&m_gameScissor);
    for (const auto& bind : m_shadowBinds)
        device->SetTexture(bind.first, bind.second->gameTex ? bind.second->gameTex
                                                            : bind.second->eyeTex[0]);
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

// --- Scissor tracking (M2) --------------------------------------------------

void StereoRenderer::SetGameScissorRect(const RECT& r)
{
    m_gameScissor = r;
    m_hasScissor  = true;
}

void StereoRenderer::SetScissorTestEnabled(bool enabled)
{
    m_scissorEnabled = enabled;
}

int StereoRenderer::ActiveEyeOffset() const
{
    if (m_shadowActive) return 0;
    return StereoHalfWidth();
}

// --- Render-target routing --------------------------------------------------

HRESULT StereoRenderer::HandleSetRenderTarget(IDirect3DDevice9* device, DWORD idx,
                                              IDirect3DSurface9* surf)
{
    if (idx != 0 || !m_active)
        return device->SetRenderTarget(idx, surf);

    // Game returning to the surface we substituted at BeginScene.
    if (surf && surf == m_gameRT)
    {
        Capture::Trace("SetRenderTarget: re-substituting stereo (surf=%p)", surf);
        EndShadowPass();
        if (m_stereoDS) device->SetDepthStencilSurface(m_stereoDS);
        return device->SetRenderTarget(0, m_stereoSurf);
    }

    // Redundant re-set of the surface the active shadow is standing in for.
    if (surf && m_shadowActive && surf == m_shadowActive->gameSurf)
        return S_OK;

    // Game moving to its own offscreen surface mid-frame.
    if (Capture::TraceActive() && surf)
    {
        D3DSURFACE_DESC d{};
        surf->GetDesc(&d);
        Capture::Trace("SetRenderTarget: PASS-THROUGH (surf=%p %ux%u, m_gameRT=%p)",
                       surf, d.Width, d.Height, m_gameRT);
    }
    EndShadowPass();
    if (!surf || !Config::Get().offscreenPasses)
        return device->SetRenderTarget(idx, surf);

    SurfaceShadow* shadow = BeginShadowPass(device, surf);
    if (!shadow)
        return device->SetRenderTarget(idx, surf); // fall back to mono rendering

    m_shadowActive = shadow;
    return device->SetRenderTarget(0, shadow->eyeSurf[0]);
}

HRESULT StereoRenderer::HandleSetDepthStencil(IDirect3DDevice9* device, IDirect3DSurface9* ds)
{
    if (!m_active)
        return device->SetDepthStencilSurface(ds);

    // Remember the game's intent so OnPresent restores the right surface.
    if (ds != m_gameDS)
    {
        if (m_gameDS) m_gameDS->Release();
        m_gameDS = ds;
        if (m_gameDS) m_gameDS->AddRef();
    }

    if (m_shadowActive)
    {
        if (ds && EnsureShadowDepth(device, *m_shadowActive, ds))
            return device->SetDepthStencilSurface(m_shadowActive->depth);
        return device->SetDepthStencilSurface(nullptr);
    }
    if (m_stereoDS)
        return device->SetDepthStencilSurface(m_stereoDS);
    return device->SetDepthStencilSurface(ds);
}

// --- Texture tracking -------------------------------------------------------

void StereoRenderer::OnGameSetTexture(DWORD stage, IDirect3DBaseTexture9* tex)
{
    if (m_shadows.empty()) return; // no shadow passes seen yet; nothing to track

    // Drop any active shadow bind on this stage; re-added below if the new
    // texture is shadowed.
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

// --- Per-eye Clear while a shadow pass is bound -----------------------------

HRESULT StereoRenderer::ClearBothEyes(IDirect3DDevice9* device, DWORD count, const D3DRECT* rects,
                                      DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{
    HRESULT hr = S_OK;
    for (int eye = 0; eye < 2; ++eye)
    {
        device->SetRenderTarget(0, m_shadowActive->eyeSurf[eye]);
        HRESULT r = device->Clear(count, rects, flags, color, z, stencil);
        if (FAILED(r)) hr = r;
    }
    device->SetRenderTarget(0, m_shadowActive->eyeSurf[0]);
    return hr;
}

// --- Offscreen shadow passes (M2) -------------------------------------------

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

// The shadow pass needs a legal depth-stencil pairing: match the format of
// whatever depth the game intends, sized to the (game-sized) eye targets.
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

// --- Debug: dump the stereo surface to a BMP --------------------------------
// [Debug] DumpFrames=N dumps the stereo target of the first N presents to
// WarbandVR_frameNNN.bmp next to the dll. Lets us see exactly what each eye
// half contains without any VR hardware.

namespace { char g_dumpAnchor = 0; } // module-address anchor for GetModuleHandleEx

void StereoRenderer::DumpStereoSurface(IDirect3DDevice9* device, int index)
{
    if (!m_stereoSurf) return;
    D3DSURFACE_DESC desc{};
    m_stereoSurf->GetDesc(&desc);

    IDirect3DSurface9* sys = nullptr;
    HRESULT hr = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                     D3DPOOL_SYSTEMMEM, &sys, nullptr);
    if (FAILED(hr) || !sys)
    {
        LOG_WARN("DumpStereoSurface: CreateOffscreenPlainSurface failed hr=0x%08X", hr);
        return;
    }
    hr = device->GetRenderTargetData(m_stereoSurf, sys);
    if (FAILED(hr))
    {
        LOG_WARN("DumpStereoSurface: GetRenderTargetData failed hr=0x%08X", hr);
        sys->Release();
        return;
    }

    D3DLOCKED_RECT lr{};
    if (FAILED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY)))
    {
        sys->Release();
        return;
    }

    char path[MAX_PATH];
    char dllDir[MAX_PATH] = {};
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&g_dumpAnchor), &self);
    GetModuleFileNameA(self, dllDir, MAX_PATH);
    char* slash = strrchr(dllDir, '\\');
    if (slash) slash[1] = '\0';
    snprintf(path, MAX_PATH, "%sWarbandVR_frame%03d.bmp", dllDir, index);

    const int w = (int)desc.Width, h = (int)desc.Height;
    const int rowBytes = w * 3;
    const unsigned pixelBytes = rowBytes * h;
    FILE* f = nullptr;
    fopen_s(&f, path, "wb");
    if (f)
    {
        #pragma pack(push, 1)
        struct { char id[2]; unsigned size, r1, off; unsigned hdr, w, h; unsigned short planes, bpp;
                 unsigned comp, img, xppm, yppm, colors, important; } hdr = {};
        #pragma pack(pop)
        hdr.id[0] = 'B'; hdr.id[1] = 'M';
        hdr.off = 54;
        hdr.size = 54 + pixelBytes;
        hdr.hdr = 40; hdr.w = w; hdr.h = h;
        hdr.planes = 1; hdr.bpp = 24;
        hdr.img = pixelBytes;
        fwrite(&hdr, 1, 54, f);
        // BMP is bottom-up BGR; source is top-down BGRA (X8R8G8B8)
        for (int y = h - 1; y >= 0; --y)
        {
            const unsigned char* srcRow = (const unsigned char*)lr.pBits + (size_t)y * lr.Pitch;
            // Convert BGRA -> BGR in chunks to avoid a full-size stack buffer
            unsigned char row[4096 * 3];
            for (int x0 = 0; x0 < w; x0 += 4096)
            {
                int n = (w - x0 < 4096) ? (w - x0) : 4096;
                for (int x = 0; x < n; ++x)
                {
                    row[x * 3 + 0] = srcRow[(x0 + x) * 4 + 0];
                    row[x * 3 + 1] = srcRow[(x0 + x) * 4 + 1];
                    row[x * 3 + 2] = srcRow[(x0 + x) * 4 + 2];
                }
                fwrite(row, 1, n * 3, f);
            }
        }
        fclose(f);
        LOG_INFO("DumpStereoSurface: wrote %s (%ux%u)", path, w, h);
    }
    sys->UnlockRect();
    sys->Release();
}
