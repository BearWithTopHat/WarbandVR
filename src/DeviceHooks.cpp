#include "DeviceHooks.h"
#include "VTableHook.h"
#include "StereoRenderer.h"
#include "Config.h"
#include "Logger.h"
#include "Capture.h"
#include "CameraScan.h"

#include <vector>
#include <iterator>
#include <cstring>

// ---------------------------------------------------------------------------
// Original function pointers (filled by InstallHooks)
// ---------------------------------------------------------------------------
namespace
{
    typedef HRESULT (__stdcall IDirect3DDevice9::*MemberFn)();

    #define DECL_ORIG(name, sig) sig name = nullptr

    typedef HRESULT (__stdcall *Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
    typedef HRESULT (__stdcall *Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
    typedef HRESULT (__stdcall *SetRenderTarget_t)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);
    typedef HRESULT (__stdcall *BeginScene_t)(IDirect3DDevice9*);
    typedef HRESULT (__stdcall *EndScene_t)(IDirect3DDevice9*);
    typedef HRESULT (__stdcall *Clear_t)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD);
    typedef HRESULT (__stdcall *SetTransform_t)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
    typedef HRESULT (__stdcall *SetViewport_t)(IDirect3DDevice9*, const D3DVIEWPORT9*);
    typedef HRESULT (__stdcall *SetScissorRect_t)(IDirect3DDevice9*, const RECT*);
    typedef HRESULT (__stdcall *DrawPrimitive_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
    typedef HRESULT (__stdcall *DrawIndexedPrimitive_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
    typedef HRESULT (__stdcall *DrawPrimitiveUP_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
    typedef HRESULT (__stdcall *DrawIndexedPrimitiveUP_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, UINT, const void*, D3DFORMAT, const void*, UINT);
    typedef HRESULT (__stdcall *SetVertexShaderConstantF_t)(IDirect3DDevice9*, UINT, const float*, UINT);
    typedef HRESULT (__stdcall *DrawRectPatch_t)(IDirect3DDevice9*, UINT, const float*, const D3DRECTPATCH_INFO*);

    DECL_ORIG(o_Reset, Reset_t);
    DECL_ORIG(o_Present, Present_t);
    DECL_ORIG(o_SetRenderTarget, SetRenderTarget_t);
    DECL_ORIG(o_BeginScene, BeginScene_t);
    DECL_ORIG(o_EndScene, EndScene_t);
    DECL_ORIG(o_Clear, Clear_t);
    DECL_ORIG(o_SetTransform, SetTransform_t);
    DECL_ORIG(o_SetViewport, SetViewport_t);
    DECL_ORIG(o_SetScissorRect, SetScissorRect_t);
    DECL_ORIG(o_DrawPrimitive, DrawPrimitive_t);
    DECL_ORIG(o_DrawIndexedPrimitive, DrawIndexedPrimitive_t);
    DECL_ORIG(o_DrawPrimitiveUP, DrawPrimitiveUP_t);
    DECL_ORIG(o_DrawIndexedPrimitiveUP, DrawIndexedPrimitiveUP_t);
    DECL_ORIG(o_SetVertexShaderConstantF, SetVertexShaderConstantF_t);
    DECL_ORIG(o_DrawRectPatch, DrawRectPatch_t);

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

        HRESULT hr = S_OK;
        DeviceHooks::Get().m_internal = true;
        StereoRenderer::Get().ApplyEye(dev, 0);
        DeviceHooks::Get().m_internal = false;
        hr = draw();

        DeviceHooks::Get().m_internal = true;
        StereoRenderer::Get().ApplyEye(dev, 1);
        DeviceHooks::Get().m_internal = false;
        hr = draw();

        DeviceHooks::Get().m_internal = true;
        StereoRenderer::Get().RestoreGameState(dev);
        DeviceHooks::Get().m_internal = false;

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
// Detours
// ---------------------------------------------------------------------------
HRESULT __stdcall hkReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp)
{
    if (DeviceHooks::Get().m_internal) return o_Reset(dev, pp);
    LOG_INFO("Reset(%ux%u)", pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0);
    StereoRenderer::Get().OnReset(dev, pp);       // release DEFAULT-pool stuff FIRST
    HRESULT hr = o_Reset(dev, pp);
    if (SUCCEEDED(hr)) LOG_INFO("Reset succeeded"); else LOG_ERROR("Reset failed hr=0x%08X", hr);
    return hr;
}

HRESULT __stdcall hkPresent(IDirect3DDevice9* dev, const RECT* s, const RECT* d, HWND w, const RGNDATA* r)
{
    if (DeviceHooks::Get().m_internal) return o_Present(dev, s, d, w, r);
    if (StereoOn())
    {
        DeviceHooks::Get().m_internal = true;
        StereoRenderer::Get().OnPresent(dev);
        DeviceHooks::Get().m_internal = false;
    }
    return o_Present(dev, s, d, w, r);
}

HRESULT __stdcall hkSetRenderTarget(IDirect3DDevice9* dev, DWORD idx, IDirect3DSurface9* surf)
{
    // Pass-through for now. M2 work: track offscreen passes so post-processing
    // and shadow maps don't break stereo (docs/01_Design.md).
    return o_SetRenderTarget(dev, idx, surf);
}

HRESULT __stdcall hkBeginScene(IDirect3DDevice9* dev)
{
    if (DeviceHooks::Get().m_internal) return o_BeginScene(dev);
    Capture::BeginFrame();
    HRESULT hr = o_BeginScene(dev);
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
    if (DeviceHooks::Get().m_internal) return o_EndScene(dev);
    Capture::EndFrame();
    CameraScan::OnFrame();
    StereoRenderer::Get().OnEndScene(dev);
    return o_EndScene(dev);
}

HRESULT __stdcall hkClear(IDirect3DDevice9* dev, DWORD count, const D3DRECT* rects,
                          DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return o_Clear(dev, count, rects, flags, color, z, stencil);

    if (rects == nullptr)
    {
        // Whole-target clear covers both eyes in one call.
        return o_Clear(dev, count, rects, flags, color, z, stencil);
    }

    // Region clears are in surface coordinates, so they must be duplicated
    // per eye with each half's horizontal offset (unlike viewports, Clear
    // rects do NOT follow the viewport).
    const LONG halfW = static_cast<LONG>(StereoRenderer::Get().StereoHalfWidth());
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
        hr = o_Clear(dev, count, eyeRects.data(), flags, color, z, stencil);
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
    return o_SetTransform(dev, state, m);
}

HRESULT __stdcall hkSetViewport(IDirect3DDevice9* dev, const D3DVIEWPORT9* vp)
{
    // Track the game's intended viewport so ApplyEye derives correct per-eye
    // viewports even when the game changes it mid-frame.
    if (!DeviceHooks::Get().m_internal && vp)
        StereoRenderer::Get().SetGameViewport(*vp);
    return o_SetViewport(dev, vp);
}

HRESULT __stdcall hkSetScissorRect(IDirect3DDevice9* dev, const RECT* rect)
{
    // TODO(M2): per-eye offset when scissor test is enabled.
    return o_SetScissorRect(dev, rect);
}

HRESULT __stdcall hkDrawPrimitive(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, UINT start, UINT count)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return o_DrawPrimitive(dev, type, start, count);
    return DrawBothEyes(dev, static_cast<int>(count),
                        [&] { return o_DrawPrimitive(dev, type, start, count); });
}

HRESULT __stdcall hkDrawIndexedPrimitive(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                                         UINT minIndex, UINT numVertices, UINT startIndex, UINT count)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return o_DrawIndexedPrimitive(dev, type, baseVertex, minIndex, numVertices, startIndex, count);
    return DrawBothEyes(dev, static_cast<int>(count), [&] {
        return o_DrawIndexedPrimitive(dev, type, baseVertex, minIndex, numVertices, startIndex, count); });
}

HRESULT __stdcall hkDrawPrimitiveUP(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, UINT count,
                                    const void* data, UINT stride)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return o_DrawPrimitiveUP(dev, type, count, data, stride);
    return DrawBothEyes(dev, static_cast<int>(count),
                        [&] { return o_DrawPrimitiveUP(dev, type, count, data, stride); });
}

HRESULT __stdcall hkDrawIndexedPrimitiveUP(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type,
                                           UINT minVertex, UINT numVertices, UINT count,
                                           const void* indexData, D3DFORMAT indexFormat,
                                           const void* vertexData, UINT stride)
{
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return o_DrawIndexedPrimitiveUP(dev, type, minVertex, numVertices, count, indexData, indexFormat, vertexData, stride);
    return DrawBothEyes(dev, static_cast<int>(count), [&] {
        return o_DrawIndexedPrimitiveUP(dev, type, minVertex, numVertices, count, indexData, indexFormat, vertexData, stride); });
}

HRESULT __stdcall hkSetVertexShaderConstantF(IDirect3DDevice9* dev, UINT reg, const float* data, UINT count)
{
    // M0/M2: when ViewSource=1, the captured view(/proj) matrix lives in a VS
    // register. Capture here like hkSetTransform does for fixed-function.
    if (!DeviceHooks::Get().m_internal)
    {
        Capture::OnSetVSConstantF(reg, data, count); // M0 tooling (no-op when off)

        if (Config::Get().enableStereo && Config::Get().viewSource == 1 &&
            reg == static_cast<UINT>(Config::Get().vsViewRegister) && count >= 4 && data)
        {
            D3DMATRIX m;
            memcpy(&m, data, sizeof(D3DMATRIX));
            StereoRenderer::Get().CaptureViewMatrix(m);
        }
    }
    return o_SetVertexShaderConstantF(dev, reg, data, count);
}

HRESULT __stdcall hkDrawRectPatch(IDirect3DDevice9* dev, UINT handle, const float* segs, const D3DRECTPATCH_INFO* info)
{
    // Patches are essentially unused by Warband; duplicate anyway for safety.
    // (No primitive count available - pass -1 so the UI heuristic never fires.)
    if (DeviceHooks::Get().m_internal || !StereoOn())
        return o_DrawRectPatch(dev, handle, segs, info);
    return DrawBothEyes(dev, -1, [&] { return o_DrawRectPatch(dev, handle, segs, info); });
}

// ---------------------------------------------------------------------------
// Installation
// ---------------------------------------------------------------------------
DeviceHooks& DeviceHooks::Get()
{
    static DeviceHooks s;
    return s;
}

void DeviceHooks::OnDeviceCreated(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS* pp)
{
    Shutdown(); // in case of a second device (rare, but be safe)
    m_device = device;
    if (pp) m_pp = *pp;
    InstallHooks(device);
    StereoRenderer::Get().OnDeviceCreated(device, pp);
}

void DeviceHooks::Shutdown()
{
    if (m_vt) { m_vt->UnhookAll(); m_vt.reset(); }
    m_device = nullptr;
}

void DeviceHooks::InstallHooks(IDirect3DDevice9* device)
{
    m_vt = std::make_unique<VTableHook>(device);

    struct { int index; void* detour; void** original; const char* name; } hooks[] = {
        { 16,  &hkReset,                   reinterpret_cast<void**>(&o_Reset),                   "Reset" },
        { 17,  &hkPresent,                 reinterpret_cast<void**>(&o_Present),                 "Present" },
        { 37,  &hkSetRenderTarget,         reinterpret_cast<void**>(&o_SetRenderTarget),         "SetRenderTarget" },
        { 41,  &hkBeginScene,              reinterpret_cast<void**>(&o_BeginScene),              "BeginScene" },
        { 42,  &hkEndScene,                reinterpret_cast<void**>(&o_EndScene),                "EndScene" },
        { 43,  &hkClear,                   reinterpret_cast<void**>(&o_Clear),                   "Clear" },
        { 44,  &hkSetTransform,            reinterpret_cast<void**>(&o_SetTransform),            "SetTransform" },
        { 47,  &hkSetViewport,             reinterpret_cast<void**>(&o_SetViewport),             "SetViewport" },
        { 75,  &hkSetScissorRect,          reinterpret_cast<void**>(&o_SetScissorRect),          "SetScissorRect" },
        { 81,  &hkDrawPrimitive,           reinterpret_cast<void**>(&o_DrawPrimitive),           "DrawPrimitive" },
        { 82,  &hkDrawIndexedPrimitive,    reinterpret_cast<void**>(&o_DrawIndexedPrimitive),    "DrawIndexedPrimitive" },
        { 83,  &hkDrawPrimitiveUP,         reinterpret_cast<void**>(&o_DrawPrimitiveUP),         "DrawPrimitiveUP" },
        { 84,  &hkDrawIndexedPrimitiveUP,  reinterpret_cast<void**>(&o_DrawIndexedPrimitiveUP),  "DrawIndexedPrimitiveUP" },
        { 94,  &hkSetVertexShaderConstantF,reinterpret_cast<void**>(&o_SetVertexShaderConstantF),"SetVertexShaderConstantF" },
        { 115, &hkDrawRectPatch,           reinterpret_cast<void**>(&o_DrawRectPatch),           "DrawRectPatch" },
    };

    int ok = 0;
    for (auto& h : hooks)
    {
        if (m_vt->Hook(h.index, h.detour, h.original))
            ++ok;
        else
            LOG_ERROR("Failed to hook %s (index %d)", h.name, h.index);
    }
    LOG_INFO("DeviceHooks: %d/%zu vtable hooks installed on device %p", ok, std::size(hooks), device);
}
