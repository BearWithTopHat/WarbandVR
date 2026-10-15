// SPDX-License-Identifier: GPL-3.0-only
#include "fake_device.h"
#include <cstdarg>
#include <cstdio>

namespace Fake
{
    std::vector<std::string> g_calls;
    void Record(const char* name) { g_calls.emplace_back(name); }
    void Recordf(const char* fmt, ...)
    {
        char buf[256];
        va_list ap; va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        g_calls.emplace_back(buf);
    }
    void Clear() { g_calls.clear(); }
    int Count(const char* prefix)
    {
        int n = 0;
        const size_t len = strlen(prefix);
        for (auto& c : g_calls)
            if (c.compare(0, len, prefix) == 0) ++n;
        return n;
    }
    int CountContaining(const char* prefix, const char* needle)
    {
        int n = 0;
        const size_t len = strlen(prefix);
        for (auto& c : g_calls)
            if (c.compare(0, len, prefix) == 0 && c.find(needle) != std::string::npos) ++n;
        return n;
    }
    std::string Join()
    {
        std::string s;
        for (auto& c : g_calls) { s += c; s += '\n'; }
        return s;
    }
}

// --- FakeTexture: levels are FakeSurfaces ----------------------------------
class FakeTexture : public IDirect3DTexture9
{
public:
    std::vector<FakeSurface*> levels;
    D3DRESOURCETYPE rtype = D3DRTYPE_TEXTURE;
    ULONG refs = 1;

    FakeTexture(UINT w, UINT h, UINT lvls, D3DFORMAT fmt, D3DPOOL pool, DWORD usage)
    {
        if (!lvls) lvls = 1;
        for (UINT i = 0; i < lvls; ++i)
            levels.push_back(new FakeSurface(w >> i ? w >> i : 1, h >> i ? h >> i : 1, fmt, pool, usage));
    }
    ~FakeTexture() { for (auto* s : levels) delete s; }

    HRESULT __stdcall QueryInterface(REFIID, void** pp) override { *pp = this; return S_OK; }
    ULONG __stdcall AddRef() override  { return ++refs; }
    ULONG __stdcall Release() override { return refs ? --refs : 0; }

    DWORD __stdcall SetLOD(DWORD) override { return 0; }
    DWORD __stdcall GetLOD() override { return 0; }
    DWORD __stdcall GetLevelCount() override { return (DWORD)levels.size(); }
    HRESULT __stdcall SetAutoGenFilterType(D3DTEXTUREFILTERTYPE) override { return S_OK; }
    D3DTEXTUREFILTERTYPE __stdcall GetAutoGenFilterType() override { return D3DTEXF_NONE; }
    void __stdcall GenerateMipSubLevels() override {}
    HRESULT __stdcall GetLevelDesc(UINT i, D3DSURFACE_DESC* p) override
    { return i < levels.size() ? levels[i]->GetDesc(p) : E_FAIL; }
    HRESULT __stdcall GetSurfaceLevel(UINT i, IDirect3DSurface9** pp) override
    {
        if (i >= levels.size()) return E_FAIL;
        *pp = levels[i]; levels[i]->AddRef();
        return S_OK;
    }
    HRESULT __stdcall LockRect(UINT i, D3DLOCKED_RECT* p, const RECT* r, DWORD f) override
    { return i < levels.size() ? levels[i]->LockRect(p, r, f) : E_FAIL; }
    HRESULT __stdcall UnlockRect(UINT i) override
    { return i < levels.size() ? levels[i]->UnlockRect() : E_FAIL; }
    HRESULT __stdcall AddDirtyRect(const RECT*) override { return S_OK; }

    // IDirect3DResource9
    HRESULT __stdcall GetDevice(IDirect3DDevice9** pp) override { *pp = nullptr; return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID, const void*, DWORD, DWORD) override { return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID) override { return S_OK; }
    DWORD __stdcall SetPriority(DWORD) override { return 0; }
    DWORD __stdcall GetPriority() override { return 0; }
    void __stdcall PreLoad() override {}
    D3DRESOURCETYPE __stdcall GetType() override { return rtype; }
};

// --- FakeDevice hand-written behavior --------------------------------------
HRESULT __stdcall FakeDevice::CreateTexture(UINT w, UINT h, UINT levels, DWORD usage,
                                            D3DFORMAT fmt, D3DPOOL pool,
                                            IDirect3DTexture9** ppTex, HANDLE*)
{
    Fake::Recordf("CreateTexture %ux%u fmt=%d pool=%d", w, h, (int)fmt, (int)pool);
    if (failCreateTextureCalls > 0) { --failCreateTextureCalls; *ppTex = nullptr; return E_FAIL; }
    *ppTex = new FakeTexture(w, h, levels, fmt, pool, usage);
    return S_OK;
}
HRESULT __stdcall FakeDevice::CreateRenderTarget(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE,
                                                 DWORD, BOOL, IDirect3DSurface9** ppSurf, HANDLE*)
{
    Fake::Recordf("CreateRenderTarget %ux%u", w, h);
    if (failCreateRenderTargetCalls > 0) { --failCreateRenderTargetCalls; *ppSurf = nullptr; return E_FAIL; }
    *ppSurf = new FakeSurface(w, h, fmt, D3DPOOL_DEFAULT, D3DUSAGE_RENDERTARGET);
    return S_OK;
}
HRESULT __stdcall FakeDevice::CreateDepthStencilSurface(UINT w, UINT h, D3DFORMAT fmt,
                                                        D3DMULTISAMPLE_TYPE, DWORD, BOOL,
                                                        IDirect3DSurface9** ppSurf, HANDLE*)
{
    Fake::Recordf("CreateDepthStencilSurface %ux%u", w, h);
    if (failCreateDepthStencilCalls > 0) { --failCreateDepthStencilCalls; *ppSurf = nullptr; return E_FAIL; }
    *ppSurf = new FakeSurface(w, h, fmt);
    return S_OK;
}
HRESULT __stdcall FakeDevice::CreateOffscreenPlainSurface(UINT w, UINT h, D3DFORMAT fmt,
                                                          D3DPOOL pool, IDirect3DSurface9** ppSurf, HANDLE*)
{
    Fake::Recordf("CreateOffscreenPlainSurface %ux%u", w, h);
    *ppSurf = new FakeSurface(w, h, fmt, pool);
    return S_OK;
}
HRESULT __stdcall FakeDevice::GetBackBuffer(UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9** ppSurf)
{
    *ppSurf = new FakeSurface(pp.BackBufferWidth, pp.BackBufferHeight, pp.BackBufferFormat);
    return S_OK;
}

HRESULT __stdcall FakeDevice::SetRenderTarget(DWORD idx, IDirect3DSurface9* s)
{
    if (idx < 4) rt[idx] = s;
    Fake::Recordf("SetRenderTarget %u %p", idx, (void*)s);
    return S_OK;
}
HRESULT __stdcall FakeDevice::GetRenderTarget(DWORD idx, IDirect3DSurface9** pp)
{
    *pp = idx < 4 ? rt[idx] : nullptr;
    if (*pp) (*pp)->AddRef();
    return S_OK;
}
HRESULT __stdcall FakeDevice::SetDepthStencilSurface(IDirect3DSurface9* s)
{
    ds = s; Fake::Recordf("SetDS %p", (void*)s); return S_OK;
}
HRESULT __stdcall FakeDevice::GetDepthStencilSurface(IDirect3DSurface9** pp)
{
    *pp = ds; if (ds) ds->AddRef(); return S_OK;
}
HRESULT __stdcall FakeDevice::SetViewport(const D3DVIEWPORT9* v)
{
    vp = *v; Fake::Recordf("SetViewport %ux%u@%u,%u", v->Width, v->Height, v->X, v->Y);
    return S_OK;
}
HRESULT __stdcall FakeDevice::GetViewport(D3DVIEWPORT9* v) { *v = vp; return S_OK; }
HRESULT __stdcall FakeDevice::SetTransform(D3DTRANSFORMSTATETYPE st, const D3DMATRIX* m)
{
    transforms[(UINT)st] = *m;
    Fake::Recordf("SetTransform %d _41=%.4f", (int)st, m->_41);
    return S_OK;
}
HRESULT __stdcall FakeDevice::GetTransform(D3DTRANSFORMSTATETYPE st, D3DMATRIX* m)
{
    *m = transforms[(UINT)st]; return S_OK;
}
HRESULT __stdcall FakeDevice::SetTexture(DWORD stage, IDirect3DBaseTexture9* t)
{
    if (stage < 16) textures[stage] = t;
    return S_OK;
}
HRESULT __stdcall FakeDevice::SetRenderState(D3DRENDERSTATETYPE st, DWORD v)
{
    renderStates[(UINT)st & 0xFF] = v;
    Fake::Recordf("SetRenderState %d=%u", (int)st, v);
    return S_OK;
}
HRESULT __stdcall FakeDevice::SetScissorRect(const RECT* r)
{
    if (!r) return E_POINTER;
    scissor = *r;
    hasScissor = true;
    Fake::Recordf("SetScissorRect %ld,%ld,%ld,%ld", r->left, r->top, r->right, r->bottom);
    return S_OK;
}
HRESULT __stdcall FakeDevice::SetVertexShaderConstantF(UINT reg, const float* data, UINT count)
{
    if (!data || reg >= 256) return E_POINTER;
    UINT n = count * 4;
    if (reg * 4 + n > 256 * 4) n = 256 * 4 - reg * 4;
    memcpy(vsConstants + reg * 4, data, n * sizeof(float));
    Fake::Recordf("SetVSCF c%u x%u _41=%.4f", reg, count, count >= 4 ? data[12] : 0.0f);
    return S_OK;
}
HRESULT __stdcall FakeDevice::GetVertexShaderConstantF(UINT reg, float* data, UINT count)
{
    if (!data || reg >= 256) return E_POINTER;
    UINT n = count * 4;
    if (reg * 4 + n > 256 * 4) n = 256 * 4 - reg * 4;
    memcpy(data, vsConstants + reg * 4, n * sizeof(float));
    return S_OK;
}

HRESULT __stdcall FakeDevice::Clear(DWORD, const D3DRECT*, DWORD flags, D3DCOLOR, float, DWORD)
{
    Fake::Recordf("Clear flags=%u", flags); return clearResult;
}
HRESULT __stdcall FakeDevice::DrawPrimitive(D3DPRIMITIVETYPE t, UINT, UINT count)
{
    Fake::Recordf("DrawPrimitive t=%d n=%u", (int)t, count); return drawResult;
}
HRESULT __stdcall FakeDevice::DrawIndexedPrimitive(D3DPRIMITIVETYPE t, INT, UINT, UINT, UINT, UINT count)
{
    Fake::Recordf("DrawIndexedPrimitive t=%d n=%u", (int)t, count); return drawResult;
}
HRESULT __stdcall FakeDevice::DrawPrimitiveUP(D3DPRIMITIVETYPE t, UINT count, const void*, UINT)
{
    Fake::Recordf("DrawPrimitiveUP t=%d n=%u", (int)t, count); return drawResult;
}
HRESULT __stdcall FakeDevice::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE t, UINT, UINT, UINT count,
                                                     const void*, D3DFORMAT, const void*, UINT)
{
    Fake::Recordf("DrawIndexedPrimitiveUP t=%d n=%u", (int)t, count); return drawResult;
}
HRESULT __stdcall FakeDevice::DrawRectPatch(UINT h, const float*, const D3DRECTPATCH_INFO*)
{
    Fake::Recordf("DrawRectPatch h=%u", h); return drawResult;
}
HRESULT __stdcall FakeDevice::DrawTriPatch(UINT h, const float*, const D3DTRIPATCH_INFO*)
{
    Fake::Recordf("DrawTriPatch h=%u", h); return drawResult;
}

HRESULT __stdcall FakeDevice::StretchRect(IDirect3DSurface9*, const RECT*, IDirect3DSurface9*,
                                          const RECT*, D3DTEXTUREFILTERTYPE)
{
    Fake::Record("StretchRect"); return S_OK;
}
HRESULT __stdcall FakeDevice::GetRenderTargetData(IDirect3DSurface9*, IDirect3DSurface9*)
{
    Fake::Record("GetRenderTargetData"); return S_OK;
}
