#include "StereoRenderer.h"
#include "Config.h"
#include "Logger.h"
#include "MathUtil.h"
#include "Capture.h"
#include "OpenXRBridge.h"
#include "OneEuroFilter.h"

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
    bool ok = true;
    for (int eye = 0; eye < 2 && ok; ++eye)
    {
        HRESULT hr = device->CreateRenderTarget(w, h, fmt, D3DMULTISAMPLE_NONE, 0,
                                                FALSE, &m_eyeSurf[eye], nullptr);
        if (FAILED(hr))
        {
            LOG_ERROR("StereoRenderer: CreateRenderTarget(%ux%u) eye=%d failed hr=0x%08X", w, h, eye, hr);
            m_eyeSurf[eye] = nullptr;
            ok = false;
            break;
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
    LOG_INFO("StereoRenderer: per-eye targets %ux%u (depth fmt=%d) created", w, h, dsFmt);
}

void StereoRenderer::ReleaseResources()
{
    ReleaseAllShadows();
    for (int eye = 0; eye < 2; ++eye)
    {
        if (m_eyeDS[eye])   { m_eyeDS[eye]->Release();   m_eyeDS[eye] = nullptr; }
        if (m_eyeSurf[eye]) { m_eyeSurf[eye]->Release(); m_eyeSurf[eye] = nullptr; }
    }
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
    m_inScene = true;
    CheckRecenterKey();
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
    if (!m_active) return;

    EndShadowPass(); // in case a shadow pass was left open (M2)

    if (Config::Get().dumpFrames > 0 && (m_presentCount++ % Config::Get().dumpFrames) == 0)
        DumpStereoSurface(device, m_dumpedFrames++);

    if (Config::Get().outputMode == 1)
    {
        // M3: hand the stereo frame to OpenXR instead of the monitor.
        // (Bridge currently takes one surface; M3 will take both eyes.)
        OpenXRBridge::SubmitFrame(device, m_eyeSurf[0]);
    }
    else
    {
        CompositeToBackbuffer(device);
    }

    m_active = false;
    m_needClear = true;
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
        RECT dst = { eye * halfW, 0, (eye + 1) * halfW, bbH };
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

    static int s_loggedRT = 0;
    if (s_loggedRT < 20)
    {
        ++s_loggedRT;
        LOG_INFO("SetCurrentRT: %p %ux%u main=%d (eye %ux%u)", surf, d.Width, d.Height,
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
// Per-eye application
// ---------------------------------------------------------------------------

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

    // Samplers bound to a shadowed surface get this eye's copy (M2).
    for (const auto& bind : m_shadowBinds)
        device->SetTexture(bind.first, bind.second->eyeTex[eye]);

    if (!m_hasView && cfg.viewSource == 0) return; // nothing captured yet; draws will match mono

    const float halfSep = (cfg.ipd * cfg.worldScale) * 0.5f;

    if (cfg.viewSource == 0)
    {
        // M4: head pose delta composes onto the captured view first.
        D3DMATRIX headView = ApplyHeadDelta(m_baseView);

        // UI passes use an orthographic projection (tiny z-scale, w == 1).
        // Stereo-shifting those misaligns text against flat-drawn boxes, so
        // draw them flat (identical in both eyes). 3D view/world matrices are
        // orthonormal (|_33| ~ 1); viewproj matrices are non-affine.
        const bool flatUI = std::fabs(headView._33) < 0.5f && std::fabs(headView._44 - 1.0f) < 0.01f;
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
            if (m_loggedRB < 200) // TEMP DIAG: every pair's actual register
            {
                ++m_loggedRB;
                D3DVIEWPORT9 vpNow{};
                device->GetViewport(&vpNow);
                LOG_INFO("RB cap=%u t=%.3f,%.3f,%.3f m33=%.3f m44=%.3f vp=%u,%u %ux%u gamevp=%u,%u %ux%u",
                         m_viewCaptures,
                         m_lastReadback._41, m_lastReadback._42, m_lastReadback._43,
                         m_lastReadback._33, m_lastReadback._44,
                         vpNow.X, vpNow.Y, vpNow.Width, vpNow.Height,
                         m_gameVp.X, m_gameVp.Y, m_gameVp.Width, m_gameVp.Height);
            }
        }

        const D3DMATRIX& m = m_lastReadback;
        const bool flatUI = std::fabs(m._33) < 0.5f && std::fabs(m._44 - 1.0f) < 0.01f;
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
    for (const auto& bind : m_shadowBinds)
        device->SetTexture(bind.first, bind.second->gameTex ? bind.second->gameTex
                                                            : bind.second->eyeTex[0]);
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
    HRESULT hr = S_OK;
    for (int eye = 0; eye < 2; ++eye)
    {
        device->SetRenderTarget(0, m_shadowActive ? m_shadowActive->eyeSurf[eye] : m_eyeSurf[eye]);
        HRESULT r = device->Clear(count, rects, flags, color, z, stencil);
        if (FAILED(r)) hr = r;
    }
    device->SetRenderTarget(0, m_shadowActive ? m_shadowActive->eyeSurf[0] : m_eyeSurf[0]);
    return hr;
}

// ---------------------------------------------------------------------------
// Debug: dump eye surfaces to BMPs
// ---------------------------------------------------------------------------

namespace { char g_dumpAnchor = 0; } // module-address anchor for GetModuleHandleEx

static void WriteBmp(const char* path, IDirect3DSurface9* sys, UINT w, UINT h, LONG pitch,
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
                       reinterpret_cast<LPCSTR>(&g_dumpAnchor), &self);
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
                snprintf(path, MAX_PATH, "%sWarbandVR_frame%03d_%c.bmp", dllDir, index,
                         eye == 0 ? 'L' : 'R');
                WriteBmp(path, sys, desc.Width, desc.Height, lr.Pitch,
                         static_cast<const unsigned char*>(lr.pBits));
                LOG_INFO("DumpStereoSurface: wrote %s", path);
                sys->UnlockRect();
            }
        }
        sys->Release();
    }
}
