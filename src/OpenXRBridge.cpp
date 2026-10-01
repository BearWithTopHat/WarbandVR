#include "OpenXRBridge.h"
#include "Logger.h"

#ifdef WBVR_WITH_OPENXR
// ============================================================================
// REAL IMPLEMENTATION (M3) - skeleton.
// Requires: OpenXR SDK headers + openxr_loader.lib, and a D3D11 device
// created alongside the game's D3D9 device for the xrCreateSession binding.
// Fill in following docs/03_Agent_Prompts.md task M3 and Khronos hello_xr.
// ============================================================================

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <d3d11.h>

namespace
{
    XrInstance  g_instance  = XR_NULL_HANDLE;
    XrSession   g_session   = XR_NULL_HANDLE;
    XrSpace     g_appSpace  = XR_NULL_HANDLE;
    XrSwapchain g_swap[2]   = { XR_NULL_HANDLE, XR_NULL_HANDLE };

    ID3D11Device*        g_d3d11  = nullptr;
    ID3D11DeviceContext* g_d3d11Ctx = nullptr;
}

bool OpenXRBridge::Init(IDirect3DDevice9* device)
{
    (void)device;
    LOG_INFO("OpenXRBridge: init requested");

    // TODO(M3): 1) xrCreateInstance with XR_KHR_D3D11_enable extension
    // TODO(M3): 2) create D3D11 device (feature level 11_0), keep context
    // TODO(M3): 3) xrGetSystem + xrCreateSession with XrGraphicsBindingD3D11KHR
    // TODO(M3): 4) xrCreateReferenceSpace(LOCAL), two swapchains (one per eye)
    // TODO(M3): 5) begin the xrWaitFrame/xrBeginFrame loop on a worker thread
    LOG_ERROR("OpenXRBridge: Init not implemented yet (see docs/03_Agent_Prompts.md task M3)");
    return false;
}

void OpenXRBridge::SubmitFrame(IDirect3DDevice9* device, IDirect3DSurface9* stereoSurface)
{
    (void)device; (void)stereoSurface;
    // TODO(M3): copy stereo halves into swapchain images.
    // Short-term: GetRenderTargetData to system memory -> Map/Copy to a
    //   D3D11 texture -> CopySubresourceRegion into swapchain image (slow but
    //   correct; good enough to see pixels in the headset).
    // Long-term: D3D9Ex shared render target -> OpenSharedResource on D3D11,
    //   zero-copy. Requires the game device to be D3D9Ex; see design doc.
    // Then: xrAcquireSwapchainImage / xrWaitSwapchainImage / release,
    //   xrEndFrame with two projection layers.
}

void OpenXRBridge::Shutdown()
{
    // TODO(M3): destroy swapchains, space, session, instance (reverse order)
    if (g_d3d11Ctx) { g_d3d11Ctx->Release(); g_d3d11Ctx = nullptr; }
    if (g_d3d11)    { g_d3d11->Release();    g_d3d11 = nullptr; }
}

bool OpenXRBridge::GetHeadPoseDelta(float* outYawPitchRoll, float* outXYZ)
{
    (void)outYawPitchRoll; (void)outXYZ;
    // TODO(M4): locate view space pose via xrLocateViews at the predicted
    // display time; return orientation/position relative to a reference pose
    // captured at "recenter" time.
    return false;
}

#else
// ============================================================================
// STUB IMPLEMENTATION (default build, no OpenXR SDK required)
// ============================================================================

bool OpenXRBridge::Init(IDirect3DDevice9*)
{
    LOG_WARN("OpenXRBridge: built without WBVR_WITH_OPENXR - OpenXR unavailable");
    return false;
}

void OpenXRBridge::SubmitFrame(IDirect3DDevice9*, IDirect3DSurface9*)
{
    LOG_WARN("OpenXRBridge::SubmitFrame called in stub build - set OutputMode=0 or rebuild with WBVR_WITH_OPENXR");
}

void OpenXRBridge::Shutdown() {}

bool OpenXRBridge::GetHeadPoseDelta(float*, float*) { return false; }

#endif // WBVR_WITH_OPENXR
