#pragma once

// DeviceHooks: vtable-hooks the game's IDirect3DDevice9 and implements the
// stereo strategy: duplicate every draw call once per eye with a shifted
// view matrix, into a double-width render target.
//
// Hooked functions (IDirect3DDevice9 vtable indices in parentheses):
//   Reset(16) Present(17) SetRenderTarget(37) BeginScene(41) EndScene(42)
//   Clear(43) SetTransform(44) SetViewport(47) SetScissorRect(75)
//   DrawPrimitive(81) DrawIndexedPrimitive(82) DrawPrimitiveUP(83)
//   DrawIndexedPrimitiveUP(84) SetVertexShaderConstantF(94) DrawRectPatch(115)
//
// To hook additional methods: add the index to InstallHooks(), write a
// hkXxx detour, and ALWAYS call through the stored original - never call a
// hooked method on the device without holding g_internal (reentrancy guard).

#include <d3d9.h>
#include <memory>

class VTableHook;

class DeviceHooks
{
public:
    static DeviceHooks& Get();

    void OnDeviceCreated(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS* pp);
    void Shutdown();

    // Reentrancy guard: true while OUR code is executing D3D calls.
    // Every detour checks this first and passes straight through when set.
    bool m_internal = false;

private:
    DeviceHooks() = default;
    void InstallHooks(IDirect3DDevice9* device);

    IDirect3DDevice9* m_device = nullptr;
    std::unique_ptr<VTableHook> m_vt;
    D3DPRESENT_PARAMETERS m_pp = {};
};
