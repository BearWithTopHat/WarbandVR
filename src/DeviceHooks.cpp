#include "DeviceHooks.h"
#include "StereoRenderer.h"
#include "Config.h"
#include "Logger.h"
#include "Capture.h"
#include "CameraScan.h"

#include <vector>
#include <iterator>
#include <cstring>

// ---------------------------------------------------------------------------
// Shared detour machinery
// ---------------------------------------------------------------------------
namespace
{
    // Execute one game draw call for both eyes. `draw` invokes the ORIGINAL.
    // primCount drives the UI heuristic: tiny batches (HUD text, icons) are
    // drawn once, mono, when SBSMonoUI is on - doubling them buys nothing and
    // produces the classic "double vision UI" artifact.
    template <typename F>
    HRESULT DrawBothEyes(IDirect3DDevice9* dev, int primCount, F&& draw)
    {
        const Config& cfg = Config::Get();
        if (cfg.sbsMonoUI && primCount > 0 && primCount <= cfg.uiMaxTriangles)
            return draw(); // single mono draw

        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);

        DeviceHooks::Get().m_internal = true;
        StereoRenderer::Get().ApplyEye(dev, 0);
        DeviceHooks::Get().m_internal = false;
        HRESULT hrL = draw();

        DeviceHooks::Get().m_internal = true;
        StereoRenderer::Get().ApplyEye(dev, 1);
        DeviceHooks::Get().m_internal = false;
        HRESULT hrR = draw();

        DeviceHooks::Get().m_internal = true;
        StereoRenderer::Get().RestoreGameState(dev);
        DeviceHooks::Get().m_internal = false;

        // Report the first failure; previously a failing left-eye draw was
        // silently masked by the right-eye result.
        HRESULT hr = FAILED(hrL) ? hrL : hrR;
        if (FAILED(hr))
        {
            static int loggedFails = 0;
            if (loggedFails < 10)
            {
                ++loggedFails;
                LOG_WARN("DrawBothEyes: eye draw failed hrL=0x%08X hrR=0x%08X", hrL, hrR);
            }
        }

        QueryPerformanceCounter(&t1);
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        double us = static_cast<double>(t1.QuadPart - t0.QuadPart) * 1e6 / freq.QuadPart;
        StereoRenderer::Get().AddDuplicationTime(us, 2);
        return hr;
    }

    inline bool StereoOn()
    {
        return Config::Get().enableStereo && StereoRenderer::Get().IsActive();
    }
}

// ---------------------------------------------------------------------------
// Detours (invoked from DeviceProxy overrides; `dev` is the REAL device)
// ---------------------------------------------------------------------------
HRESULT __stdcall hkReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp)
{
    if (DeviceHooks::Get().m_internal) return dev->Reset(pp);
    LOG_INFO("Reset(%ux%u)", pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0);
    StereoRenderer::Get().OnReset(dev, pp);       // release DEFAULT-pool stuff FIRST
    HRESULT hr = dev->Reset(pp);
    if (SUCCEEDED(hr)) LOG_INFO("Reset succeeded"); else LOG_ERROR("Reset failed hr=0x%08X", hr);
    return hr;
}

HRESULT __stdcall hkPresent(IDirect3DDevice9* dev, const RECT* s, const RECT* d, HWND w, const RGNDATA* r)
{
    if (DeviceHooks::Get().m_internal) return dev->Present(s, d, w, r);
    if (StereoOn())
    {
        DeviceHooks::Get().m_internal = true;
        StereoRenderer::Get().OnPresent(dev);
        DeviceHooks::Get().m_internal = false;
    }
    return dev->Present(s, d, w, r);
}

HRESULT __stdcall hkSetRenderTarget(IDirect3DDevice9* dev, DWORD idx, IDirect3DSurface9* surf)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->SetRenderTarget(idx, surf);
    Capture::Trace("SetRenderTarget(%u, %p)", idx, surf);

    // Routed through StereoRenderer: re-applies the stereo substitution when
    // the game returns to its main target, and (M2, OffscreenPasses=1) gives
    // the game's own mid-frame targets per-eye shadow copies.
    DeviceHooks::Get().m_internal = true;
    HRESULT hr = StereoRenderer::Get().HandleSetRenderTarget(dev, idx, surf);
    DeviceHooks::Get().m_internal = false;
    return hr;
}

HRESULT __stdcall hkSetDepthStencilSurface(IDirect3DDevice9* dev, IDirect3DSurface9* ds)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->SetDepthStencilSurface(ds);

    // The game re-pointing the depth buffer mid-frame would pair its small
    // depth surface with our double-width target (illegal in D3D9); the
    // renderer substitutes the matching stereo/shadow depth instead.
    DeviceHooks::Get().m_internal = true;
    HRESULT hr = StereoRenderer::Get().HandleSetDepthStencil(dev, ds);
    DeviceHooks::Get().m_internal = false;
    return hr;
}

HRESULT __stdcall hkBeginScene(IDirect3DDevice9* dev)
{
    if (DeviceHooks::Get().m_internal) return dev->BeginScene();
    Capture::Trace("BeginScene");
    Capture::BeginFrame();
    HRESULT hr = dev->BeginScene();
    if (SUCCEEDED(hr))
    {
        DeviceHooks::Get().m_internal = true;
        StereoRenderer::Get().OnBeginScene(dev);  // substitute stereo RT0
        DeviceHooks::Get().m_internal = false;
    }
    return hr;
}

HRESULT __stdcall hkEndScene(IDirect3DDevice9* dev)
{
    if (DeviceHooks::Get().m_internal) return dev->EndScene();
    Capture::Trace("EndScene");
    Capture::EndFrame();
    CameraScan::OnFrame();
    StereoRenderer::Get().OnEndScene(dev);
    return dev->EndScene();
}

HRESULT __stdcall hkClear(IDirect3DDevice9* dev, DWORD count, const D3DRECT* rects,
                          DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->Clear(count, rects, flags, color, z, stencil);
    Capture::Trace("Clear(count=%u rects=%p flags=0x%X z=%.2f)", count, rects, flags, z);

    // While a shadow pass is bound the target is per-eye (game-sized), so
    // Clear must run once per eye with unshifted rects.
    if (StereoRenderer::Get().ShadowActive())
    {
        DeviceHooks::Get().m_internal = true;
        HRESULT hr = StereoRenderer::Get().ClearBothEyes(dev, count, rects, flags, color, z, stencil);
        DeviceHooks::Get().m_internal = false;
        return hr;
    }

    if (rects == nullptr)
    {
        // Whole-target clear covers both eyes in one call.
        return dev->Clear(count, rects, flags, color, z, stencil);
    }

    // Region clears are in surface coordinates, so they must be duplicated
    // per eye with each half's horizontal offset (unlike viewports, Clear
    // rects do NOT follow the viewport).
    const LONG halfW = static_cast<LONG>(StereoRenderer::Get().ActiveEyeOffset());
    HRESULT hr = S_OK;
    for (int eye = 0; eye < 2; ++eye)
    {
        const LONG offset = (eye == 0) ? 0 : halfW;
        std::vector<D3DRECT> eyeRects(count);
        for (DWORD i = 0; i < count; ++i)
        {
            eyeRects[i].x1 = rects[i].x1 + offset;
            eyeRects[i].x2 = rects[i].x2 + offset;
            eyeRects[i].y1 = rects[i].y1;
            eyeRects[i].y2 = rects[i].y2;
        }
        hr = dev->Clear(count, eyeRects.data(), flags, color, z, stencil);
    }
    return hr;
}

HRESULT __stdcall hkSetTransform(IDirect3DDevice9* dev, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m)
{
    if (!DeviceHooks::Get().m_internal && m)
    {
        Capture::OnSetTransform(state, m); // M0 tooling (no-op when off)

        if (Config::Get().enableStereo && Config::Get().viewSource == 0)
        {
            if (state == D3DTS_VIEW)
                StereoRenderer::Get().CaptureViewMatrix(*m);
            else if (state == D3DTS_PROJECTION)
                StereoRenderer::Get().CaptureProjectionMatrix(*m);
        }
    }
    return dev->SetTransform(state, m);
}

HRESULT __stdcall hkSetViewport(IDirect3DDevice9* dev, const D3DVIEWPORT9* vp)
{
    // Track the game's intended viewport so ApplyEye derives correct per-eye
    // viewports even when the game changes it mid-frame.
    if (!DeviceHooks::Get().m_internal && vp)
    {
        StereoRenderer::Get().SetGameViewport(*vp);
        if (Capture::TraceActive())
            Capture::Trace("SetViewport(%u,%u %ux%u)", vp->X, vp->Y, vp->Width, vp->Height);
    }
    return dev->SetViewport(vp);
}

HRESULT __stdcall hkSetRenderState(IDirect3DDevice9* dev, D3DRENDERSTATETYPE state, DWORD value)
{
    if (!DeviceHooks::Get().m_internal && state == D3DRS_SCISSORTESTENABLE)
    {
        StereoRenderer::Get().SetScissorTestEnabled(value != 0);
        if (Capture::TraceActive())
            Capture::Trace("ScissorTestEnable=%u", value);
    }
    return dev->SetRenderState(state, value);
}

HRESULT __stdcall hkSetTexture(IDirect3DDevice9* dev, DWORD stage, IDirect3DBaseTexture9* tex)
{
    // Learns which sampler stages bind a shadowed surface so ApplyEye can
    // swap in the per-eye copy (cheap no-op when no shadows exist).
    if (!DeviceHooks::Get().m_internal && StereoOn())
        StereoRenderer::Get().OnGameSetTexture(stage, tex);
    return dev->SetTexture(stage, tex);
}

HRESULT __stdcall hkSetScissorRect(IDirect3DDevice9* dev, const RECT* rect)
{
    // Track the game's intended rect; ApplyEye re-applies it per eye with a
    // horizontal offset when the scissor test is enabled.
    if (!DeviceHooks::Get().m_internal && rect)
    {
        StereoRenderer::Get().SetGameScissorRect(*rect);
        if (Capture::TraceActive())
            Capture::Trace("SetScissorRect(%ld,%ld,%ld,%ld)", rect->left, rect->top, rect->right, rect->bottom);
    }
    return dev->SetScissorRect(rect);
}

HRESULT __stdcall hkDrawPrimitive(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, UINT start, UINT count)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->DrawPrimitive(type, start, count);
    if (Capture::TraceActive())
        Capture::Trace("DrawPrimitive(type=%d start=%u count=%u)", type, start, count);
    return DrawBothEyes(dev, static_cast<int>(count),
                        [&] { return dev->DrawPrimitive(type, start, count); });
}

HRESULT __stdcall hkDrawIndexedPrimitive(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                                         UINT minIndex, UINT numVertices, UINT startIndex, UINT count)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->DrawIndexedPrimitive(type, baseVertex, minIndex, numVertices, startIndex, count);
    if (Capture::TraceActive())
        Capture::Trace("DrawIndexedPrimitive(type=%d numVerts=%u count=%u)", type, numVertices, count);
    return DrawBothEyes(dev, static_cast<int>(count), [&] {
        return dev->DrawIndexedPrimitive(type, baseVertex, minIndex, numVertices, startIndex, count); });
}

HRESULT __stdcall hkDrawPrimitiveUP(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, UINT count,
                                    const void* data, UINT stride)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->DrawPrimitiveUP(type, count, data, stride);
    return DrawBothEyes(dev, static_cast<int>(count),
                        [&] { return dev->DrawPrimitiveUP(type, count, data, stride); });
}

HRESULT __stdcall hkDrawIndexedPrimitiveUP(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type,
                                           UINT minVertex, UINT numVertices, UINT count,
                                           const void* indexData, D3DFORMAT indexFormat,
                                           const void* vertexData, UINT stride)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->DrawIndexedPrimitiveUP(type, minVertex, numVertices, count, indexData, indexFormat, vertexData, stride);
    return DrawBothEyes(dev, static_cast<int>(count), [&] {
        return dev->DrawIndexedPrimitiveUP(type, minVertex, numVertices, count, indexData, indexFormat, vertexData, stride); });
}

HRESULT __stdcall hkSetVertexShaderConstantF(IDirect3DDevice9* dev, UINT reg, const float* data, UINT count)
{
    // M0/M2: when ViewSource=1, the captured view(/proj) matrix lives in a VS
    // register. Capture here like hkSetTransform does for fixed-function.
    if (!DeviceHooks::Get().m_internal)
    {
        Capture::OnSetVSConstantF(reg, data, count); // M0 tooling (no-op when off)

        if (Capture::TraceActive() && count >= 4 && data)
        {
            const float* f = data;
            Capture::Trace("VSCF c%u count=%u | %.3f %.3f %.3f %.3f", reg, count,
                           f[0], f[1], f[2], f[3]);
        }
        if (Config::Get().enableStereo && Config::Get().viewSource == 1 &&
            reg == static_cast<UINT>(Config::Get().vsViewRegister) && count >= 4 && data)
        {
            D3DMATRIX m;
            memcpy(&m, data, sizeof(D3DMATRIX));
            StereoRenderer::Get().CaptureViewMatrix(m);
        }
    }
    return dev->SetVertexShaderConstantF(reg, data, count);
}

HRESULT __stdcall hkDrawRectPatch(IDirect3DDevice9* dev, UINT handle, const float* segs, const D3DRECTPATCH_INFO* info)
{
    // Patches are essentially unused by Warband; duplicate anyway for safety.
    // (No primitive count available - pass -1 so the UI heuristic never fires.)
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return dev->DrawRectPatch(handle, segs, info);
    return DrawBothEyes(dev, -1, [&] { return dev->DrawRectPatch(handle, segs, info); });
}

// ---------------------------------------------------------------------------
// Device lifecycle
// ---------------------------------------------------------------------------
DeviceHooks& DeviceHooks::Get()
{
    static DeviceHooks s;
    return s;
}

void DeviceHooks::OnDeviceCreated(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS* pp)
{
    m_device = device;
    if (pp) m_pp = *pp;
    StereoRenderer::Get().OnDeviceCreated(device, pp);
}

void DeviceHooks::Shutdown()
{
    m_device = nullptr;
}
