#include "StereoRenderer.h"
#include "Config.h"
#include "Logger.h"
#include "MathUtil.h"
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
    if (m_gameRT)     { m_gameRT->Release();     m_gameRT = nullptr; }
    if (m_stereoSurf) { m_stereoSurf->Release(); m_stereoSurf = nullptr; }
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
    // the SetRenderTarget hook currently passes that through untouched -
    // see docs/01_Design.md "render target redirection" for the M2 fix.
    if (!m_gameRT && SUCCEEDED(device->GetRenderTarget(0, &m_gameRT)) && m_gameRT)
    {
        if (SUCCEEDED(device->SetRenderTarget(0, m_stereoSurf)))
            m_active = true;
        else
            LOG_WARN("StereoRenderer: SetRenderTarget substitution failed");
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

    if (Config::Get().outputMode == 1)
    {
        // M3: hand the stereo frame to OpenXR instead of the monitor.
        OpenXRBridge::SubmitFrame(device, m_stereoSurf);
    }
    else
    {
        CompositeToBackbuffer(device);
    }

    // Restore the game's RT0 so the next frame starts from a clean state.
    if (m_gameRT)
    {
        device->SetRenderTarget(0, m_gameRT);
        m_gameRT->Release();
        m_gameRT = nullptr;
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
    device->StretchRect(m_stereoSurf, &srcL, bb, &dstL, D3DTEXF_NONE);
    device->StretchRect(m_stereoSurf, &srcR, bb, &dstR, D3DTEXF_NONE);

    bb->Release();
}

void StereoRenderer::CaptureViewMatrix(const D3DMATRIX& m)
{
    // Menu-lock bookkeeping: note when the view last actually changed.
    if (!m_hasView || memcmp(&m_baseView, &m, sizeof(D3DMATRIX)) != 0)
        m_lastViewChangeTick = GetTickCount64();

    m_baseView = m;
    m_hasView  = true;

    int limit = Config::Get().logViewMatrices;
    if (m_loggedViews < limit)
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

    // Viewport: left or right half of the stereo target. Uses the game's
    // most recent intended viewport (tracked by hkSetViewport); falls back
    // to querying the device on first use.
    D3DVIEWPORT9 vp;
    if (m_hasGameVp) vp = m_gameVp;
    else if (FAILED(device->GetViewport(&vp))) return;

    vp.X = (eye == 0) ? 0 : vp.Width; // stereo target is 2x width
    device->SetViewport(&vp);

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
        device->SetVertexShaderConstantF(cfg.vsViewRegister,
                                         reinterpret_cast<const float*>(&eyeView), 4);
    }
}

void StereoRenderer::RestoreGameState(IDirect3DDevice9* device)
{
    if (m_hasGameVp)
        device->SetViewport(&m_gameVp);
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
