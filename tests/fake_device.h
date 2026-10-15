// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// FakeDevice / FakeSurface: recording fakes for headless integration tests.
// FakeDevice implements the FULL IDirect3DDevice9 vtable (the generated
// stubs come from fake_device_device.inc / fake_trivial_generated.inc);
// the methods the renderer actually drives are hand-written below and
// behave like a real device (stored RT/DS/viewport/transforms/textures).

#include "d3d9.h"
#include <vector>
#include <string>
#include <cstring>

namespace Fake
{
    extern std::vector<std::string> g_calls;   // global call log
    void Record(const char* name);
    void Recordf(const char* fmt, ...);        // formatted entry
    void Clear();
    // helpers for assertions
    int  Count(const char* prefix);
    int  CountContaining(const char* prefix, const char* needle);
    std::string Join();
}

class FakeSurface : public IDirect3DSurface9
{
public:
    D3DSURFACE_DESC desc{};
    ULONG refs = 1;
    std::vector<char> bits; // LockRect backing store

    explicit FakeSurface(UINT w = 0, UINT h = 0, D3DFORMAT fmt = D3DFMT_X8R8G8B8,
                         D3DPOOL pool = D3DPOOL_DEFAULT, DWORD usage = 0)
    {
        desc.Width = w; desc.Height = h; desc.Format = fmt; desc.Pool = pool;
        desc.Usage = usage; desc.Type = D3DRTYPE_SURFACE;
        bits.resize(w && h ? (size_t)w * h * 4 : 0, 0);
    }

    HRESULT __stdcall QueryInterface(REFIID, void** pp) override { *pp = this; return S_OK; }
    ULONG __stdcall AddRef() override  { return ++refs; }
    ULONG __stdcall Release() override { return refs ? --refs : 0; }

    HRESULT __stdcall GetDesc(D3DSURFACE_DESC* pDesc) override { *pDesc = desc; return S_OK; }
    HRESULT __stdcall LockRect(D3DLOCKED_RECT* p, const RECT*, DWORD) override
    {
        p->Pitch = (INT)(desc.Width * 4);
        p->pBits = bits.empty() ? nullptr : bits.data();
        return S_OK;
    }
    HRESULT __stdcall UnlockRect() override { return S_OK; }

#include "fake_surface.inc"
};

class FakeDevice : public IDirect3DDevice9
{
public:
    // state a real device has
    IDirect3DSurface9* rt[4]   = {};
    IDirect3DSurface9* ds      = nullptr;
    IDirect3DBaseTexture9* textures[16] = {};
    D3DVIEWPORT9 vp  = {};
    D3DMATRIX    transforms[260] = {};   // indexed by D3DTRANSFORMSTATETYPE
    D3DPRESENT_PARAMETERS pp = {};
    ULONG refs = 1;
    int presentCount = 0;

    // Fault-injection knobs for reset/robustness tests.
    bool allowEx = true;
    int failCreateTextureCalls = 0;
    int failCreateRenderTargetCalls = 0;
    int failCreateDepthStencilCalls = 0;
    HRESULT resetResult = S_OK;
    HRESULT drawResult = S_OK;
    HRESULT presentResult = S_OK;
    HRESULT beginSceneResult = S_OK;
    HRESULT endSceneResult = S_OK;
    HRESULT clearResult = S_OK;

    // Additional state modeled for shader/scissor/assertion and overlay
    // state-restore checks.
    float vsConstants[256 * 4] = {};
    RECT scissor = {};
    bool hasScissor = false;
    DWORD renderStates[256] = {};
    DWORD stageStates[16][16] = {};
    DWORD samplerStates[16][16] = {};
    DWORD fvf = 0;
    IDirect3DVertexShader9* vertexShader = nullptr;
    IDirect3DPixelShader9* pixelShader = nullptr;

    explicit FakeDevice(UINT backW = 1600, UINT backH = 900)
    {
        pp.BackBufferWidth = backW; pp.BackBufferHeight = backH;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.BackBufferCount = 1;
        pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    }

    // --- IUnknown ---
    HRESULT __stdcall QueryInterface(REFIID, void** pp) override
    {
        if (!allowEx) { *pp = nullptr; return E_FAIL; }
        *pp = this; return S_OK;
    }
    ULONG __stdcall AddRef() override  { return ++refs; }
    ULONG __stdcall Release() override { return refs ? --refs : 0; }

    // --- resource creation ---
    HRESULT __stdcall CreateTexture(UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt,
                                    D3DPOOL pool, IDirect3DTexture9** ppTex, HANDLE*) override;
    HRESULT __stdcall CreateRenderTarget(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE ms,
                                         DWORD msq, BOOL, IDirect3DSurface9** ppSurf, HANDLE*) override;
    HRESULT __stdcall CreateDepthStencilSurface(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE ms,
                                                DWORD msq, BOOL, IDirect3DSurface9** ppSurf, HANDLE*) override;
    HRESULT __stdcall CreateOffscreenPlainSurface(UINT w, UINT h, D3DFORMAT fmt, D3DPOOL pool,
                                                  IDirect3DSurface9** ppSurf, HANDLE*) override;
    HRESULT __stdcall GetBackBuffer(UINT, UINT iBB, D3DBACKBUFFER_TYPE, IDirect3DSurface9** ppSurf) override;
    HRESULT __stdcall GetDirect3D(IDirect3D9** pp) override { *pp = nullptr; return S_OK; }
    HRESULT __stdcall GetDeviceCaps(D3DCAPS9* pCaps) override { memset(pCaps, 0, sizeof(*pCaps)); return S_OK; }

    // --- render state the renderer drives ---
    HRESULT __stdcall SetRenderTarget(DWORD idx, IDirect3DSurface9* s) override;
    HRESULT __stdcall GetRenderTarget(DWORD idx, IDirect3DSurface9** pp) override;
    HRESULT __stdcall SetDepthStencilSurface(IDirect3DSurface9* s) override;
    HRESULT __stdcall GetDepthStencilSurface(IDirect3DSurface9** pp) override;
    HRESULT __stdcall SetViewport(const D3DVIEWPORT9* v) override;
    HRESULT __stdcall GetViewport(D3DVIEWPORT9* v) override;
    HRESULT __stdcall SetTransform(D3DTRANSFORMSTATETYPE st, const D3DMATRIX* m) override;
    HRESULT __stdcall GetTransform(D3DTRANSFORMSTATETYPE st, D3DMATRIX* m) override;
    HRESULT __stdcall SetTexture(DWORD stage, IDirect3DBaseTexture9* t) override;
    HRESULT __stdcall SetRenderState(D3DRENDERSTATETYPE st, DWORD v) override;
    HRESULT __stdcall SetScissorRect(const RECT* r) override;
    HRESULT __stdcall SetVertexShaderConstantF(UINT reg, const float* data, UINT count) override;
    HRESULT __stdcall GetVertexShaderConstantF(UINT reg, float* data, UINT count) override;

    // --- frame / draw calls ---
    HRESULT __stdcall BeginScene() override { Fake::Record("BeginScene"); return beginSceneResult; }
    HRESULT __stdcall EndScene()   override { Fake::Record("EndScene");   return endSceneResult; }
    HRESULT __stdcall Present(const RECT*, const RECT*, HWND, const RGNDATA*) override
    { ++presentCount; Fake::Record("Present"); return presentResult; }
    HRESULT __stdcall Reset(D3DPRESENT_PARAMETERS* p) override
    {
        Fake::Record("Reset");
        if (FAILED(resetResult)) return resetResult;
        pp = *p;
        return S_OK;
    }
    HRESULT __stdcall Clear(DWORD, const D3DRECT*, DWORD flags, D3DCOLOR, float, DWORD) override;
    HRESULT __stdcall DrawPrimitive(D3DPRIMITIVETYPE t, UINT start, UINT count) override;
    HRESULT __stdcall DrawIndexedPrimitive(D3DPRIMITIVETYPE t, INT base, UINT minV, UINT numV,
                                           UINT startI, UINT count) override;
    HRESULT __stdcall DrawPrimitiveUP(D3DPRIMITIVETYPE t, UINT count, const void*, UINT) override;
    HRESULT __stdcall DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE t, UINT minV, UINT numV, UINT count,
                                             const void*, D3DFORMAT, const void*, UINT) override;
    HRESULT __stdcall DrawRectPatch(UINT h, const float* segs, const D3DRECTPATCH_INFO* i) override;
    HRESULT __stdcall DrawTriPatch(UINT h, const float*, const D3DTRIPATCH_INFO*) override;

    // --- copies / readback ---
    HRESULT __stdcall StretchRect(IDirect3DSurface9* src, const RECT*, IDirect3DSurface9* dst,
                                  const RECT*, D3DTEXTUREFILTERTYPE) override;
    HRESULT __stdcall GetRenderTargetData(IDirect3DSurface9* src, IDirect3DSurface9* dst) override;

    // vertex input plumbing (recorded; not otherwise modeled)
    HRESULT __stdcall SetVertexDeclaration(IDirect3DVertexDeclaration9*) override
    { Fake::Record("SetVertexDeclaration"); return S_OK; }
    HRESULT __stdcall SetFVF(DWORD f) override { fvf = f; Fake::Recordf("SetFVF 0x%X", f); return S_OK; }
    HRESULT __stdcall SetStreamSource(UINT, IDirect3DVertexBuffer9*, UINT, UINT) override
    { Fake::Record("SetStreamSource"); return S_OK; }
    HRESULT __stdcall SetIndices(IDirect3DIndexBuffer9*) override
    { Fake::Record("SetIndices"); return S_OK; }

#include "fake_device_device.inc"
};
