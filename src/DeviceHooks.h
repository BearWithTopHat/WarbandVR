// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// DeviceHooks: stereo interception logic, invoked from DeviceProxy's
// overrides of the 18 intercepted IDirect3DDevice9 methods.
//
// Interception model (see docs/06, 2026-10-01 iteration 4): DeviceProxy is a
// full delegating wrapper - the game holds OUR object, so our vtable can
// never be swapped or restored by overlays (the Steam overlay defeats
// in-place vtable patching by swapping the object's vtable pointer to a
// heap shadow). The hkXxx functions below receive the REAL device; calling
// dev->Method(...) from them dispatches through whatever vtable the real
// device currently has, keeping the overlay in the call chain.
//
// Intercepted methods:
//   Reset Present SetRenderTarget SetDepthStencilSurface
//   BeginScene EndScene Clear SetTransform SetViewport
//   SetRenderState SetTexture SetScissorRect
//   DrawPrimitive DrawIndexedPrimitive DrawPrimitiveUP
//   DrawIndexedPrimitiveUP SetVertexShaderConstantF DrawRectPatch

#include <d3d9.h>

HRESULT __stdcall hkReset(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
HRESULT __stdcall hkPresent(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
HRESULT __stdcall hkSetRenderTarget(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);
HRESULT __stdcall hkSetDepthStencilSurface(IDirect3DDevice9*, IDirect3DSurface9*);
HRESULT __stdcall hkBeginScene(IDirect3DDevice9*);
HRESULT __stdcall hkEndScene(IDirect3DDevice9*);
HRESULT __stdcall hkClear(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD);
HRESULT __stdcall hkSetTransform(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
HRESULT __stdcall hkSetViewport(IDirect3DDevice9*, const D3DVIEWPORT9*);
HRESULT __stdcall hkSetRenderState(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD);
HRESULT __stdcall hkSetTexture(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
HRESULT __stdcall hkSetScissorRect(IDirect3DDevice9*, const RECT*);
HRESULT __stdcall hkDrawPrimitive(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
HRESULT __stdcall hkDrawIndexedPrimitive(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
HRESULT __stdcall hkDrawPrimitiveUP(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
HRESULT __stdcall hkDrawIndexedPrimitiveUP(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, UINT, const void*, D3DFORMAT, const void*, UINT);
HRESULT __stdcall hkSetVertexShaderConstantF(IDirect3DDevice9*, UINT, const float*, UINT);
HRESULT __stdcall hkDrawRectPatch(IDirect3DDevice9*, UINT, const float*, const D3DRECTPATCH_INFO*);

// Force D3DPRESENT_INTERVAL_IMMEDIATE when OpenXR output is configured:
// xrWaitFrame owns the pacing, and a driver-level present interval fights it
// (the D2R VR "VSync off - important" lesson). Shared by every device/reset
// path so the steering can't drift between them. `what` tags the log line.
void ForceImmediatePresentForXR(D3DPRESENT_PARAMETERS* pp, const char* what);

class DeviceHooks
{
public:
    static DeviceHooks& Get();

    void OnDeviceCreated(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS* pp);
    void Shutdown();

    // Vertex-format tracking (diagnostics): detects CPU-transformed (RHW)
    // geometry, which ignores all matrix and viewport manipulation.
    void TrackVertexDeclaration(IDirect3DVertexDeclaration9* decl);
    void TrackFVF(DWORD fvf);
    bool CurrentDrawIsRHW() const { return m_curIsRHW; }

    // Reentrancy guard: true while OUR code is executing D3D calls.
    // Every detour checks this first and passes straight through when set.
    bool m_internal = false;

private:
    DeviceHooks() = default;

    bool m_curIsRHW = false;
};
