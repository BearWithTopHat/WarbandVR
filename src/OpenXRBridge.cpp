// SPDX-License-Identifier: GPL-3.0-only
#include "OpenXRBridge.h"
#include "StereoRenderer.h"
#include "Config.h"
#include "Logger.h"
#include "MotionInput.h"
#include "OneEuroFilter.h"

#ifdef WBVR_WITH_OPENXR

// OpenXR structs are brace-initialised with just their type tag; the rest is
// zero. That idiom trips -Wmissing-field-initializers on every XR struct.
#ifdef __clang__
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

// ============================================================================
// Real implementation (M3). Game-thread-coupled frame loop; see header.
// ============================================================================

// d3d11.h/dxgi.h MUST precede the OpenXR headers: the Khronos
// openxr_platform.h includes nothing itself and references ID3D11Device,
// ID3D11Texture2D, and D3D_FEATURE_LEVEL in the D3D11 binding structs.
// (Our zig test shim forward-declared those types, which hid this ordering
// requirement from every headless gate - the first real MSVC OpenXR build
// failed here.)
#include <d3d11.h>
#include <dxgi.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <cstring>
#include <cmath>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "openxr_loader.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif

namespace
{
    // --- core handles -------------------------------------------------------
    XrInstance  g_instance = XR_NULL_HANDLE;
    XrSystemId  g_system   = XR_NULL_SYSTEM_ID;
    XrSession   g_session  = XR_NULL_HANDLE;
    XrSpace     g_appSpace = XR_NULL_HANDLE;
    bool        g_running  = false;   // session began successfully
    bool        g_dead     = false;   // init failed; see g_deadRetryTick
    ULONGLONG   g_deadRetryTick = 0;  // retry init every few seconds (runtime
                                      // may come up after the game launches)

    // --- D3D11 side ---------------------------------------------------------
    ID3D11Device*        g_d3d11    = nullptr;
    ID3D11DeviceContext* g_d3d11Ctx = nullptr;

    // --- swapchains ---------------------------------------------------------
    struct EyeChain
    {
        XrSwapchain swapchain = XR_NULL_HANDLE;
        std::vector<ID3D11Texture2D*> images; // enumerated XrSwapchainImageD3D11KHR
        UINT width = 0, height = 0;
    };
    EyeChain g_chain[2];

    // --- submit path resources ---------------------------------------------
    UINT g_eyeW = 0, g_eyeH = 0;   // cached at Init (swapchain/eye size)
    // slow path
    IDirect3DSurface9*   g_sysSurf[2]  = { nullptr, nullptr };  // D3DPOOL_SYSTEMMEM
    ID3D11Texture2D*     g_staging[2]  = { nullptr, nullptr };  // STAGING w/ CPU write
    // fast path (SharedSurface=1)
    ID3D11Texture2D*     g_shared[2]   = { nullptr, nullptr };  // opened from D3D9Ex handles
    bool                 g_fastPath    = false;
    IDirect3DQuery9*     g_syncQuery   = nullptr; // D3D9 event query (SharedSyncMode=1)

    // --- blend mode (queried; OPAQUE preferred) ----------------------------
    XrEnvironmentBlendMode g_blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

    // --- frame state --------------------------------------------------------
    XrFrameState         g_frameState = {};
    bool                 g_haveFrameState = false;
    ULONGLONG            g_lastSubmitTick = 0; // wall clock of last xrEndFrame

    // --- pose / M4 ----------------------------------------------------------
    XrPosef   g_refPose  = {};        // recenter reference
    bool      g_havePose = false;     // a view locate has succeeded at least once
    bool      g_haveRef  = false;
    float     g_smoothYPR[3] = {};
    float     g_smoothPos[3] = {};
    float     g_headAbs[3]   = {};   // absolute head position, app space (M5 gestures)
    OneEuro   g_fYaw{90.0}, g_fPitch{90.0}, g_fRoll{90.0};
    OneEuro   g_fPx{90.0}, g_fPy{90.0}, g_fPz{90.0};
    bool      g_filtersInit = false;
    XrTime    g_lastSampleTime = 0;

    void InitFilters()
    {
        if (g_filtersInit) return;
        const Config& cfg = Config::Get();
        double f = 90.0; // nominal sample rate; dt is supplied per sample anyway
        g_fYaw   = OneEuro(f, cfg.smoothingMinCutoff, cfg.smoothingBeta);
        g_fPitch = OneEuro(f, cfg.smoothingMinCutoff, cfg.smoothingBeta);
        g_fRoll  = OneEuro(f, cfg.smoothingMinCutoff, cfg.smoothingBeta);
        g_fPx = OneEuro(f, cfg.smoothingMinCutoff * 2, cfg.smoothingBeta);
        g_fPy = OneEuro(f, cfg.smoothingMinCutoff * 2, cfg.smoothingBeta);
        g_fPz = OneEuro(f, cfg.smoothingMinCutoff * 2, cfg.smoothingBeta);
        g_filtersInit = true;
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------
    // Destroy everything Init may have partially created; safe to call on
    // any partial state. Keeps the D3D11 device (session-independent).
    void CleanupPartial()
    {
        MotionInput::Shutdown();
        for (int eye = 0; eye < 2; ++eye)
        {
            for (auto* t : g_chain[eye].images) if (t) t->Release();
            g_chain[eye].images.clear();
            if (g_chain[eye].swapchain) { xrDestroySwapchain(g_chain[eye].swapchain); g_chain[eye].swapchain = XR_NULL_HANDLE; }
            if (g_sysSurf[eye]) { g_sysSurf[eye]->Release(); g_sysSurf[eye] = nullptr; }
            if (g_staging[eye]) { g_staging[eye]->Release(); g_staging[eye] = nullptr; }
            if (g_shared[eye])  { g_shared[eye]->Release();  g_shared[eye] = nullptr; }
            g_chain[eye].width = g_chain[eye].height = 0;
        }
        if (g_appSpace) { xrDestroySpace(g_appSpace); g_appSpace = XR_NULL_HANDLE; }
        if (g_session)  { xrDestroySession(g_session); g_session = XR_NULL_HANDLE; }
        if (g_instance) { xrDestroyInstance(g_instance); g_instance = XR_NULL_HANDLE; }
        g_running = false;
        g_haveFrameState = false;
        g_lastSubmitTick = 0;
        g_havePose = g_haveRef = false;
    }

    void Fail(const char* why)
    {
        LOG_WARN("OpenXRBridge: init failed (%s) - retrying in 5 s", why);
        CleanupPartial();
        g_dead = true;
        g_deadRetryTick = GetTickCount64();
    }

    bool XrOk(XrResult r, const char* what)
    {
        if (XR_SUCCEEDED(r)) return true;
        char name[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(g_instance, r, name);
        LOG_ERROR("OpenXR %s failed: %s (%d)", what, name, (int)r);
        // Mid-loop session/instance loss can surface as an error code before
        // the loss event is polled; treat it like the event - tear down and
        // let the 5 s retry rebuild. xrResultToString tolerated the dead
        // instance above (it is specified to work with XR_NULL_HANDLE).
        if (r == XR_ERROR_SESSION_LOST || r == XR_ERROR_INSTANCE_LOST)
        {
            CleanupPartial();
            g_dead = true;
            g_deadRetryTick = GetTickCount64();
        }
        return false;
    }

    // Quaternion (x,y,z,w) -> yaw/pitch/roll for R = Ry(yaw)Rx(pitch)Rz(roll).
    void QuatToYPR(float qx, float qy, float qz, float qw, float* ypr)
    {
        float m02 = 2.0f * (qx * qz + qy * qw);
        float m10 = 2.0f * (qx * qy + qz * qw);
        float m11 = 1.0f - 2.0f * (qx * qx + qz * qz);
        float m12 = 2.0f * (qy * qz - qx * qw);
        float m22 = 1.0f - 2.0f * (qx * qx + qy * qy);

        float s = -m12; // sin(pitch)
        if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
        float pitch = asinf(s);
        float yaw   = atan2f(m02, m22);
        float roll  = atan2f(m10, m11);
        ypr[0] = yaw; ypr[1] = pitch; ypr[2] = roll;
    }

    XrQuaternionf QuatMul(const XrQuaternionf& a, const XrQuaternionf& b)
    {
        return {
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
        };
    }

    XrQuaternionf QuatConj(const XrQuaternionf& q) { return { -q.x, -q.y, -q.z, q.w }; }

    // Damped nlerp for averaging the two eye orientations.
    XrQuaternionf QuatAvg(const XrQuaternionf& a, const XrQuaternionf& b)
    {
        XrQuaternionf bb = b;
        float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
        if (dot < 0.0f) { bb.x = -bb.x; bb.y = -bb.y; bb.z = -bb.z; bb.w = -bb.w; }
        XrQuaternionf r = { (a.x+bb.x)*0.5f, (a.y+bb.y)*0.5f, (a.z+bb.z)*0.5f, (a.w+bb.w)*0.5f };
        float len = sqrtf(r.x*r.x + r.y*r.y + r.z*r.z + r.w*r.w);
        if (len > 1e-6f) { r.x/=len; r.y/=len; r.z/=len; r.w/=len; }
        return r;
    }

    // -----------------------------------------------------------------------
    // Init / shutdown
    // -----------------------------------------------------------------------
    bool CreateD3D11Device()
    {
        // Session retries after a partial failure keep the session-independent
        // D3D11 device. Reuse it rather than leaking one device per attempt.
        if (g_d3d11 && g_d3d11Ctx)
            return true;

        // Match the adapter the runtime requires (D3D11 on the HMD's GPU).
        PFN_xrGetD3D11GraphicsRequirementsKHR pfnReq = nullptr;
        if (!XrOk(xrGetInstanceProcAddr(g_instance, "xrGetD3D11GraphicsRequirementsKHR",
                                        reinterpret_cast<PFN_xrVoidFunction*>(&pfnReq)),
                  "xrGetInstanceProcAddr(xrGetD3D11GraphicsRequirementsKHR)"))
            return false;

        XrGraphicsRequirementsD3D11KHR req{ XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
        if (!XrOk(pfnReq(g_instance, g_system, &req), "xrGetD3D11GraphicsRequirementsKHR"))
            return false;

        IDXGIFactory1* factory = nullptr;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))))
            return false;

        IDXGIAdapter1* chosen = nullptr;
        for (UINT i = 0;; ++i)
        {
            IDXGIAdapter1* a = nullptr;
            if (factory->EnumAdapters1(i, &a) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC1 d{};
            a->GetDesc1(&d);
            if (memcmp(&d.AdapterLuid, &req.adapterLuid, sizeof(LUID)) == 0)
            {
                chosen = a;
                LOG_INFO("OpenXRBridge: using runtime-required adapter %d", i);
                break;
            }
            a->Release();
        }
        if (!chosen) factory->EnumAdapters1(0, &chosen); // fallback: default adapter
        factory->Release();
        if (!chosen)
        {
            LOG_ERROR("OpenXRBridge: no DXGI adapter");
            return false;
        }

        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
        HRESULT hr = D3D11CreateDevice(chosen, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                                       levels, 1, D3D11_SDK_VERSION,
                                       &g_d3d11, nullptr, &g_d3d11Ctx);
        chosen->Release();
        if (FAILED(hr))
        {
            LOG_ERROR("OpenXRBridge: D3D11CreateDevice failed hr=0x%08X", hr);
            return false;
        }
        return true;
    }

    bool CreateSwapchains(UINT w, UINT h)
    {
        for (int eye = 0; eye < 2; ++eye)
        {
            XrSwapchainCreateInfo sci{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
            sci.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            sci.format     = DXGI_FORMAT_B8G8R8X8_UNORM; // matches D3DFMT_X8R8G8B8 layout
            sci.width      = w;
            sci.height     = h;
            sci.mipCount   = 1;
            sci.faceCount  = 1;
            sci.arraySize  = 1;
            sci.sampleCount = 1;
            if (!XrOk(xrCreateSwapchain(g_session, &sci, &g_chain[eye].swapchain), "xrCreateSwapchain"))
                return false;

            uint32_t count = 0;
            xrEnumerateSwapchainImages(g_chain[eye].swapchain, 0, &count, nullptr);
            std::vector<XrSwapchainImageD3D11KHR> imgs(count, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
            if (!XrOk(xrEnumerateSwapchainImages(g_chain[eye].swapchain, count, &count,
                                                 reinterpret_cast<XrSwapchainImageBaseHeader*>(imgs.data())),
                      "xrEnumerateSwapchainImages"))
                return false;
            g_chain[eye].images.reserve(count);
            for (auto& im : imgs) g_chain[eye].images.push_back(im.texture);
            g_chain[eye].width = w;
            g_chain[eye].height = h;
        }
        LOG_INFO("OpenXRBridge: swapchains %ux%u created", w, h);
        return true;
    }

    bool CreateSlowPath(IDirect3DDevice9* device, UINT w, UINT h)
    {
        for (int eye = 0; eye < 2; ++eye)
        {
            if (FAILED(device->CreateOffscreenPlainSurface(w, h, D3DFMT_X8R8G8B8,
                                                           D3DPOOL_SYSTEMMEM, &g_sysSurf[eye], nullptr)))
            {
                LOG_ERROR("OpenXRBridge: CreateOffscreenPlainSurface eye=%d failed", eye);
                return false;
            }

            D3D11_TEXTURE2D_DESC td{};
            td.Width = w; td.Height = h;
            td.MipLevels = 1; td.ArraySize = 1;
            td.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_STAGING;
            td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(g_d3d11->CreateTexture2D(&td, nullptr, &g_staging[eye])))
            {
                LOG_ERROR("OpenXRBridge: staging texture eye=%d failed", eye);
                return false;
            }
        }
        LOG_INFO("OpenXRBridge: slow copy path ready (%ux%u)", w, h);
        return true;
    }

    bool CreateFastPath()
    {
        // Eye surfaces must be D3D9Ex shared textures (see StereoRenderer
        // CreateResources / [Output] SharedSurface=1).
        for (int eye = 0; eye < 2; ++eye)
        {
            HANDLE h = StereoRenderer::Get().EyeSharedHandle(eye);
            if (!h) return false;
            HRESULT hr = g_d3d11->OpenSharedResource(h, __uuidof(ID3D11Texture2D),
                                                     reinterpret_cast<void**>(&g_shared[eye]));
            if (FAILED(hr))
            {
                LOG_WARN("OpenXRBridge: OpenSharedResource eye=%d failed hr=0x%08X", hr, eye);
                return false;
            }
        }
        g_fastPath = true;
        LOG_INFO("OpenXRBridge: fast GPU-to-GPU path active (D3D9Ex shared surfaces)");
        return true;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool OpenXRBridge::Init(IDirect3DDevice9* device)
{
    if (g_session) return true;
    if (g_dead) return false;

    // --- instance -----------------------------------------------------------
    const char* extensions[] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };
    XrInstanceCreateInfo ici{ XR_TYPE_INSTANCE_CREATE_INFO };
    strcpy_s(ici.applicationInfo.applicationName, "WarbandVR");
    strcpy_s(ici.applicationInfo.engineName, "WarbandVR");
    ici.applicationInfo.applicationVersion = 1;
    ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    ici.enabledExtensionCount = 1;
    ici.enabledExtensionNames = extensions;
    if (!XrOk(xrCreateInstance(&ici, &g_instance), "xrCreateInstance"))
    {
        Fail("no OpenXR runtime");
        return false;
    }

    XrSystemGetInfo sgi{ XR_TYPE_SYSTEM_GET_INFO };
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (!XrOk(xrGetSystem(g_instance, &sgi, &g_system), "xrGetSystem"))
    {
        Fail("xrGetSystem");
        return false;
    }

    // --- D3D11 + session ----------------------------------------------------
    if (!CreateD3D11Device()) { Fail("D3D11 device"); return false; }

    XrGraphicsBindingD3D11KHR binding{ XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    binding.device = g_d3d11;
    XrSessionCreateInfo sci{ XR_TYPE_SESSION_CREATE_INFO };
    sci.systemId = g_system;
    sci.next = &binding;
    if (!XrOk(xrCreateSession(g_instance, &sci, &g_session), "xrCreateSession"))
    {
        Fail("xrCreateSession");
        return false;
    }

    XrReferenceSpaceCreateInfo rci{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    rci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rci.poseInReferenceSpace.orientation.w = 1.0f;
    if (!XrOk(xrCreateReferenceSpace(g_session, &rci, &g_appSpace), "xrCreateReferenceSpace"))
    {
        Fail("xrCreateReferenceSpace");
        return false;
    }

    // --- blend mode ----------------------------------------------------------
    {
        uint32_t n = 0;
        xrEnumerateEnvironmentBlendModes(g_instance, g_system,
                                         XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n, nullptr);
        std::vector<XrEnvironmentBlendMode> modes(n);
        if (n && XR_SUCCEEDED(xrEnumerateEnvironmentBlendModes(g_instance, g_system,
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, &n, modes.data())))
        {
            g_blend = modes[0];
            for (XrEnvironmentBlendMode m : modes)
                if (m == XR_ENVIRONMENT_BLEND_MODE_OPAQUE) { g_blend = m; break; }
            LOG_INFO("OpenXRBridge: blend mode %d (%u supported)", (int)g_blend, n);
        }
    }

    // --- swapchains sized to the eye surfaces -------------------------------
    UINT w = StereoRenderer::Get().EyeWidth();
    UINT h = StereoRenderer::Get().EyeHeight();
    if (w == 0 || h == 0)
    {
        D3DSURFACE_DESC d{};
        IDirect3DSurface9* eye0 = StereoRenderer::Get().EyeSurface(0);
        if (eye0 && SUCCEEDED(eye0->GetDesc(&d))) { w = d.Width; h = d.Height; }
    }
    if (w == 0 || h == 0)
    {
        Fail("eye size unknown");
        return false;
    }
    if (!CreateSwapchains(w, h)) { Fail("xrCreateSwapchain"); return false; }
    g_eyeW = w; g_eyeH = h;

    if (!Config::Get().sharedSurface || !CreateFastPath())
    {
        if (Config::Get().sharedSurface)
            LOG_WARN("OpenXRBridge: SharedSurface=1 but fast path unavailable - falling back to slow copy");
        if (!CreateSlowPath(device, w, h)) { Fail("slow-path resources"); return false; }
    }

    // M5: actions are bound to the session; safe to ignore failure (input
    // emulation is optional for M3 verification).
    MotionInput::Init();

    InitFilters();
    LOG_INFO("OpenXRBridge: initialised (fastPath=%d)", g_fastPath ? 1 : 0);
    return true;
}

void OpenXRBridge::Shutdown()
{
    CleanupPartial(); // canonical teardown (XR objects + submit resources)
    if (g_syncQuery) { g_syncQuery->Release(); g_syncQuery = nullptr; }
    if (g_d3d11Ctx) { g_d3d11Ctx->Release(); g_d3d11Ctx = nullptr; }
    if (g_d3d11)    { g_d3d11->Release();    g_d3d11 = nullptr; }
    g_dead = false;
}

namespace
{
    // Poll session events; drive the begin/end state machine.
    void PumpEvents()
    {
        XrEventDataBuffer ev{ XR_TYPE_EVENT_DATA_BUFFER };
        while (xrPollEvent(g_instance, &ev) == XR_SUCCESS)
        {
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
            {
                auto* sc = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
                switch (sc->state)
                {
                case XR_SESSION_STATE_READY:
                {
                    XrSessionBeginInfo bi{ XR_TYPE_SESSION_BEGIN_INFO };
                    bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (XrOk(xrBeginSession(g_session, &bi), "xrBeginSession"))
                    {
                        g_running = true;
                        LOG_INFO("OpenXRBridge: session running");
                    }
                    break;
                }
                case XR_SESSION_STATE_STOPPING:
                    xrEndSession(g_session);
                    g_running = false;
                    break;
                case XR_SESSION_STATE_LOSS_PENDING:
                    // The runtime is about to destroy the session out from
                    // under us. Tear everything down ourselves and arm the
                    // 5 s retry so Init rebuilds instance+session cleanly.
                    LOG_WARN("OpenXRBridge: session loss pending - reinit in 5 s");
                    CleanupPartial();
                    g_dead = true;
                    g_deadRetryTick = GetTickCount64();
                    break;
                case XR_SESSION_STATE_EXITING:
                    // Runtime asked to end VR (e.g. headset standby/SteamVR
                    // restart). Destroy the session so the SubmitFrame retry
                    // path (which keys on !g_session) can re-init later.
                    LOG_WARN("OpenXRBridge: session exiting - reinit in 5 s");
                    CleanupPartial();
                    g_dead = true;
                    g_deadRetryTick = GetTickCount64();
                    break;
                default:
                    break;
                }
            }
            else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
            {
                LOG_WARN("OpenXRBridge: instance loss pending - reinit in 5 s");
                CleanupPartial();
                g_dead = true;
                g_deadRetryTick = GetTickCount64();
            }
            ev = XrEventDataBuffer{ XR_TYPE_EVENT_DATA_BUFFER };
        }
    }

    bool CopyEyeSlow(IDirect3DDevice9* device, IDirect3DSurface9* src, int eye, ID3D11Texture2D* dst)
    {
        HRESULT hr = device->GetRenderTargetData(src, g_sysSurf[eye]);
        if (FAILED(hr))
        {
            LOG_WARN("OpenXRBridge: GetRenderTargetData eye=%d hr=0x%08X", eye, hr);
            return false;
        }

        D3DLOCKED_RECT lr{};
        if (FAILED(g_sysSurf[eye]->LockRect(&lr, nullptr, D3DLOCK_READONLY)))
            return false;

        D3D11_MAPPED_SUBRESOURCE ms{};
        if (FAILED(g_d3d11Ctx->Map(g_staging[eye], 0, D3D11_MAP_WRITE, 0, &ms)))
        {
            g_sysSurf[eye]->UnlockRect();
            return false;
        }

        (void)src;
        for (UINT y = 0; y < g_eyeH; ++y)
            memcpy(static_cast<unsigned char*>(ms.pData) + y * ms.RowPitch,
                   static_cast<const unsigned char*>(lr.pBits) + y * lr.Pitch,
                   g_eyeW * 4);
        g_d3d11Ctx->Unmap(g_staging[eye], 0);
        g_sysSurf[eye]->UnlockRect();

        g_d3d11Ctx->CopySubresourceRegion(dst, 0, 0, 0, 0, g_staging[eye], 0, nullptr);
        return true;
    }

    // SharedSyncMode=1: the cross-API (D3D9Ex -> D3D11) shared-surface copy
    // has no fence, and a bare D3D11 Flush does not guarantee the D3D9 writes
    // are complete on every driver. A D3D9 event query is the documented
    // GPU-completion mechanism: Issue(END) then pump GetData(FLUSH) until
    // TRUE stalls until ALL prior D3D9 work (both eyes) is finished. Stronger
    // than flush, at the cost of a GPU sync stall once per frame.
    void WaitForD3D9Work(IDirect3DDevice9* device)
    {
        if (!g_syncQuery &&
            (FAILED(device->CreateQuery(D3DQUERYTYPE_EVENT, &g_syncQuery)) || !g_syncQuery))
        {
            LOG_WARN("OpenXRBridge: event query unavailable; SharedSyncMode=1 falls back to flush");
            return;
        }
        g_syncQuery->Issue(D3DISSUE_END);
        BOOL done = FALSE;
        int spins = 0;
        while (done != TRUE && spins < 1000000)
        {
            if (FAILED(g_syncQuery->GetData(&done, sizeof(done), D3DGETDATA_FLUSH)))
                break; // device lost: bail rather than spin forever
            ++spins;
        }
    }

    void SubmitEyes(IDirect3DDevice9* device, IDirect3DSurface9* left, IDirect3DSurface9* right,
                    ID3D11Texture2D** outImages, bool* submittedEye)
    {
        IDirect3DSurface9* srcs[2] = { left, right };
        if (g_fastPath && Config::Get().sharedSyncMode == 1)
            WaitForD3D9Work(device);
        for (int eye = 0; eye < 2; ++eye)
        {
            uint32_t index = 0;
            if (!XrOk(xrAcquireSwapchainImage(g_chain[eye].swapchain, nullptr, &index),
                      "xrAcquireSwapchainImage"))
                continue;
            XrSwapchainImageWaitInfo wi{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
            wi.timeout = XR_INFINITE_DURATION;
            if (!XrOk(xrWaitSwapchainImage(g_chain[eye].swapchain, &wi),
                      "xrWaitSwapchainImage"))
            {
                xrReleaseSwapchainImage(g_chain[eye].swapchain, nullptr);
                continue;
            }

            ID3D11Texture2D* dst = g_chain[eye].images[index];
            if (g_fastPath)
            {
                g_d3d11Ctx->CopySubresourceRegion(dst, 0, 0, 0, 0, g_shared[eye], 0, nullptr);
                // Cross-API (D3D9Ex -> D3D11) shared-surface copy has no
                // fence; a flush per eye keeps the D3D9 writes visible in
                // order on every driver. Cheap; revisit only if perf-bound.
                g_d3d11Ctx->Flush();
            }
            else if (!CopyEyeSlow(device, srcs[eye], eye, dst))
            {
                // Copy failed: dst still holds a previous frame's content.
                // Do not mark the eye submitted; the frame is skipped rather
                // than presenting one stale eye with fresh-pose reprojection.
                xrReleaseSwapchainImage(g_chain[eye].swapchain, nullptr);
                continue;
            }

            xrReleaseSwapchainImage(g_chain[eye].swapchain, nullptr);
            outImages[eye] = dst;
            submittedEye[eye] = true;
        }
    }

    // xrLocateViews at the predicted display time; updates the M4 pose state.
    void SamplePose(XrTime predictedTime, XrView* outViews)
    {
        XrViewLocateInfo li{ XR_TYPE_VIEW_LOCATE_INFO };
        li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        li.displayTime = predictedTime;
        li.space = g_appSpace;

        XrViewState vs{ XR_TYPE_VIEW_STATE };
        uint32_t count = 0;
        if (!XrOk(xrLocateViews(g_session, &li, &vs, 2, &count, outViews), "xrLocateViews") || count < 2)
            return;
        if ((vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0)
            return;

        // Head pose ~ midpoint of the two eye views.
        XrPosef head{};
        head.position.x = (outViews[0].pose.position.x + outViews[1].pose.position.x) * 0.5f;
        head.position.y = (outViews[0].pose.position.y + outViews[1].pose.position.y) * 0.5f;
        head.position.z = (outViews[0].pose.position.z + outViews[1].pose.position.z) * 0.5f;
        head.orientation = QuatAvg(outViews[0].pose.orientation, outViews[1].pose.orientation);

        g_headAbs[0] = head.position.x;
        g_headAbs[1] = head.position.y;
        g_headAbs[2] = head.position.z;

        if (!g_haveRef)
        {
            g_refPose = head;
            g_haveRef = true;
        }

        // Delta orientation = head * inverse(ref)
        XrQuaternionf dq = QuatMul(head.orientation, QuatConj(g_refPose.orientation));
        float ypr[3];
        QuatToYPR(dq.x, dq.y, dq.z, dq.w, ypr);

        float pos[3] = {
            head.position.x - g_refPose.position.x,
            head.position.y - g_refPose.position.y,
            head.position.z - g_refPose.position.z
        };

        // OpenXR is right-handed (-Z forward); D3D/Warband view space is
        // left-handed (+Z forward). Defaults below reflect that; the
        // [Tracking] Invert* keys exist so sign conventions can be fixed
        // without a rebuild if a given runtime/game disagrees.
        const Config& cfg = Config::Get();
        if (cfg.invertYaw)   ypr[0] = -ypr[0];
        if (cfg.invertPitch) ypr[1] = -ypr[1];
        if (cfg.invertRoll)  ypr[2] = -ypr[2];
        if (cfg.invertPosX)  pos[0] = -pos[0];
        if (cfg.invertPosZ)  pos[2] = -pos[2];

        double dt = 1.0 / 90.0;
        if (g_lastSampleTime != 0)
            dt = (double)(predictedTime - g_lastSampleTime) * 1e-9;
        g_lastSampleTime = predictedTime;

        if (cfg.trackingSmoothing)
        {
            g_smoothYPR[0] = g_fYaw.Filter(ypr[0], dt);
            g_smoothYPR[1] = g_fPitch.Filter(ypr[1], dt);
            g_smoothYPR[2] = g_fRoll.Filter(ypr[2], dt);
            g_smoothPos[0] = g_fPx.Filter(pos[0], dt);
            g_smoothPos[1] = g_fPy.Filter(pos[1], dt);
            g_smoothPos[2] = g_fPz.Filter(pos[2], dt);
        }
        else
        {
            memcpy(g_smoothYPR, ypr, sizeof(ypr));
            memcpy(g_smoothPos, pos, sizeof(pos));
        }
        g_havePose = true;
    }
}

void OpenXRBridge::SubmitFrame(IDirect3DDevice9* device, IDirect3DSurface9* left, IDirect3DSurface9* right)
{
    if (!g_session)
    {
        if (g_dead && GetTickCount64() - g_deadRetryTick > 5000)
            g_dead = false; // retry: runtime may have come up since launch
        if (!g_dead && !Init(device))
            return;
    }
    if (g_dead || !left || !right) return;

    PumpEvents();

    // Device Reset / resolution change: eye surfaces were recreated at a new
    // size by StereoRenderer - rebuild swapchains and submit resources to
    // match, or the copy paths would read/write out of bounds.
    {
        D3DSURFACE_DESC d{};
        if (SUCCEEDED(left->GetDesc(&d)) && (d.Width != g_eyeW || d.Height != g_eyeH))
        {
            LOG_INFO("OpenXRBridge: eye size changed %ux%u -> %ux%u, rebuilding swapchains",
                     g_eyeW, g_eyeH, d.Width, d.Height);
            CleanupPartial();
            g_dead = false; // Init re-runs below
            if (!Init(device)) return;
            PumpEvents();
        }
    }

    if (!g_running) return;

    // FramePacing=1 (free-run): never let xrWaitFrame clamp the game loop
    // longer than one display period. When the game presents faster than the
    // HMD refresh, skip XR submission for this frame entirely - every
    // submitted frame is still a spec-required wait/begin/end triple, and
    // the skipped Present is a normal monitor frame. Use when the runtime's
    // pacing fights the game's own frame limiter (periodic stutter).
    if (Config::Get().framePacing == 1 && g_haveFrameState)
    {
        const ULONGLONG periodMs =
            static_cast<ULONGLONG>(g_frameState.predictedDisplayPeriod / 1000000LL);
        if (periodMs > 0 && GetTickCount64() - g_lastSubmitTick < periodMs)
            return;
    }

    // Frame loop (game-thread coupled by default: xrWaitFrame paces the game
    // to the HMD; see [Output] FramePacing).
    XrFrameWaitInfo fwi{ XR_TYPE_FRAME_WAIT_INFO };
    g_frameState = XrFrameState{ XR_TYPE_FRAME_STATE };
    if (!XrOk(xrWaitFrame(g_session, &fwi, &g_frameState), "xrWaitFrame"))
        return;
    g_haveFrameState = true;

    XrFrameBeginInfo fbi{ XR_TYPE_FRAME_BEGIN_INFO };
    if (!XrOk(xrBeginFrame(g_session, &fbi), "xrBeginFrame"))
        return;

    // Cinema mode (D2R VR "diorama" lesson): instead of immersive per-eye
    // projection layers, submit a single fixed quad ~2.2 m ahead of the
    // recenter reference, showing the left eye. Comfort/menu mode - combine
    // with HeadTracking=0 so the view stays put.
    const bool cinema = Config::Get().cinemaMode;

    XrCompositionLayerProjectionView projViews[2]{};
    XrCompositionLayerProjection layer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    XrCompositionLayerQuad quad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
    // Both layer structs begin with the same header layout (type tag first),
    // so the base-header pointer is just a reinterpret of whichever is used.
    const XrCompositionLayerBaseHeader* layers[1] = {
        cinema ? reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad)
               : reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer) };

    bool submitted = false;
    XrView views[2] = { { XR_TYPE_VIEW }, { XR_TYPE_VIEW } };
    if (g_frameState.shouldRender)
    {
        SamplePose(g_frameState.predictedDisplayTime, views);

        ID3D11Texture2D* imgs[2] = {};
        bool submittedEye[2] = { false, false };
        SubmitEyes(device, left, right, imgs, submittedEye);

        if (cinema)
        {
            submitted = submittedEye[0]; // quad shows the left eye only
            quad.layerFlags = 0;
            quad.space = g_appSpace;
            quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            quad.pose.orientation.w = 1.0f;
            quad.pose.position.x = g_refPose.position.x;
            quad.pose.position.y = g_refPose.position.y;
            quad.pose.position.z = g_refPose.position.z - 2.2f; // -Z forward
            quad.size.width = 2.2f;
            quad.size.height = 2.2f * ((float)g_eyeH / (float)g_eyeW);
            quad.subImage.swapchain = g_chain[0].swapchain;
            quad.subImage.imageRect.offset = { 0, 0 };
            quad.subImage.imageRect.extent = { (int)g_chain[0].width, (int)g_chain[0].height };
            quad.subImage.imageArrayIndex = 0;
        }
        else
        {
            // Layer valid only if both eyes submitted AND the pose locate
            // worked (a zero pose is invalid input for the compositor).
            submitted = submittedEye[0] && submittedEye[1] && g_havePose;
            for (int eye = 0; eye < 2; ++eye)
            {
                projViews[eye] = XrCompositionLayerProjectionView{ XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
                projViews[eye].pose = views[eye].pose;
                projViews[eye].fov  = views[eye].fov;
                projViews[eye].subImage.swapchain = g_chain[eye].swapchain;
                projViews[eye].subImage.imageRect.offset = { 0, 0 };
                projViews[eye].subImage.imageRect.extent = { (int)g_chain[0].width, (int)g_chain[0].height };
                projViews[eye].subImage.imageArrayIndex = 0;
            }
            layer.space = g_appSpace;
            layer.viewCount = 2;
            layer.views = projViews;
        }
    }

    XrFrameEndInfo fei{ XR_TYPE_FRAME_END_INFO };
    fei.displayTime = g_frameState.predictedDisplayTime;
    fei.environmentBlendMode = g_blend;
    fei.layerCount = submitted ? 1u : 0u;
    fei.layers = submitted ? layers : nullptr;
    xrEndFrame(g_session, &fei);
    g_lastSubmitTick = GetTickCount64();

    // M5: action polling + input emulation/injection rides the frame cadence.
    MotionInput::OnFrame();
}

bool OpenXRBridge::GetHeadPoseDelta(float* outYawPitchRoll, float* outXYZ)
{
    if (!g_havePose) return false;
    memcpy(outYawPitchRoll, g_smoothYPR, sizeof(g_smoothYPR));
    memcpy(outXYZ, g_smoothPos, sizeof(g_smoothPos));
    return true;
}

bool OpenXRBridge::GetHeadPoseAbsolute(float* outXYZ)
{
    if (!g_havePose) return false;
    memcpy(outXYZ, g_headAbs, sizeof(g_headAbs));
    return true;
}

void OpenXRBridge::SnapTurn(float degrees)
{
    // Rotate the recenter REFERENCE around Y: the composed head delta shifts
    // by -degrees, turning the view without touching engine state. The
    // reference position re-anchors to the current head XZ so the rotation
    // pivots around the player, not the old anchor point.
    if (!g_havePose) return;
    float ypr[3];
    QuatToYPR(g_refPose.orientation.x, g_refPose.orientation.y,
              g_refPose.orientation.z, g_refPose.orientation.w, ypr);
    float rad = degrees * 0.01745329252f;
    float half = rad * 0.5f;
    XrQuaternionf qTurn{ 0.0f, sinf(half), 0.0f, cosf(half) }; // yaw +degrees
    g_refPose.orientation = QuatMul(qTurn, g_refPose.orientation);
    g_refPose.position.x = g_headAbs[0];
    g_refPose.position.z = g_headAbs[2];
    g_fYaw.Reset(); g_fPx.Reset(); g_fPz.Reset();
    LOG_INFO("OpenXRBridge: snap turn %.0f deg", degrees);
}

void OpenXRBridge::Recenter()
{
    g_haveRef = false; // next SamplePose captures the new reference
    g_fYaw.Reset(); g_fPitch.Reset(); g_fRoll.Reset();
    g_fPx.Reset(); g_fPy.Reset(); g_fPz.Reset();
    LOG_INFO("OpenXRBridge: recentered");
}

bool OpenXRBridge::SessionRunning() { return g_running; }
void* OpenXRBridge::XrInstanceHandle() { return reinterpret_cast<void*>(g_instance); }
void* OpenXRBridge::XrSessionHandle() { return reinterpret_cast<void*>(g_session); }
void* OpenXRBridge::XrAppSpaceHandle() { return reinterpret_cast<void*>(g_appSpace); }
long long OpenXRBridge::PredictedDisplayTime()
{
    return g_haveFrameState ? (long long)g_frameState.predictedDisplayTime : 0;
}

#else
// ============================================================================
// Stub implementation (default build, no OpenXR SDK required)
// ============================================================================

bool OpenXRBridge::Init(IDirect3DDevice9*)
{
    LOG_WARN("OpenXRBridge: built without WBVR_WITH_OPENXR - OpenXR unavailable");
    return false;
}

void OpenXRBridge::SubmitFrame(IDirect3DDevice9*, IDirect3DSurface9*, IDirect3DSurface9*)
{
    static int s_warned = 0;
    if (++s_warned == 1 || (s_warned % 600) == 0)
        LOG_WARN("OpenXRBridge::SubmitFrame called in stub build (x%d) - use OutputMode=0 or build Release-OpenXR",
                 s_warned);
}

void OpenXRBridge::Shutdown() {}

bool OpenXRBridge::GetHeadPoseDelta(float*, float*) { return false; }
bool OpenXRBridge::GetHeadPoseAbsolute(float*) { return false; }
void OpenXRBridge::SnapTurn(float) {}
void OpenXRBridge::Recenter() {}
bool OpenXRBridge::SessionRunning() { return false; }
void* OpenXRBridge::XrInstanceHandle() { return nullptr; }
void* OpenXRBridge::XrSessionHandle() { return nullptr; }
void* OpenXRBridge::XrAppSpaceHandle() { return nullptr; }
long long OpenXRBridge::PredictedDisplayTime() { return 0; }

#endif // WBVR_WITH_OPENXR
