#pragma once

// StereoRenderer: owns the stereo render target and all per-eye state math.
//
// Model:
//  - The game believes it renders to its normal backbuffer.
//  - At BeginScene we quietly point RT0 at a double-width stereo target.
//  - Every draw call is executed twice, once per eye, each with the eye's
//    viewport, an eye-shifted view matrix, and (optionally) an off-center
//    projection matrix.
//  - At Present the two halves are composited side-by-side onto the real
//    backbuffer (M1 output) or handed to the OpenXR bridge (M3).
//  - Head-tracking (M4) is already wired through the eye-math: the pose
//    delta composes onto the captured view here, so M4 is "provide a pose
//    source + tune", not a re-architecture.
//
// Everything is feature-flagged through Config; with EnableStereo=0 this
// class touches nothing and the game is byte-for-byte stock.

#include <d3d9.h>
#include <windows.h>
#include <deque>
#include <utility>
#include <vector>

class StereoRenderer
{
public:
    static StereoRenderer& Get();

    void OnDeviceCreated(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS* pp);
    void OnReset(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS* pp); // BEFORE real Reset
    void Shutdown();

    // Frame lifecycle -------------------------------------------------------
    void OnBeginScene(IDirect3DDevice9* device);   // substitute RT, mark active
    void OnEndScene(IDirect3DDevice9* device);     // perf accounting
    void OnPresent(IDirect3DDevice9* device);      // composite SBS to backbuffer

    // Per-draw-call API -----------------------------------------------------
    bool IsActive() const { return m_active; }
    void ApplyEye(IDirect3DDevice9* device, int eye);   // viewport + matrices
    void RestoreGameState(IDirect3DDevice9* device);    // undo ApplyEye

    // Matrix capture (fed by the SetTransform / VS-constant hooks)
    void CaptureViewMatrix(const D3DMATRIX& m);
    void CaptureProjectionMatrix(const D3DMATRIX& m);
    const D3DMATRIX& BaseView() const { return m_baseView; }
    bool HasView() const { return m_hasView; }

    // Game viewport tracking (fed by hkSetViewport when not internal)
    void SetGameViewport(const D3DVIEWPORT9& vp);

    // Scissor tracking (M2): the game's scissor rect is stored here and
    // re-applied per eye with a horizontal offset when scissor test is on.
    void SetGameScissorRect(const RECT& r);
    void SetScissorTestEnabled(bool enabled);

    // Render-target routing (fed by hkSetRenderTarget /
    // hkSetDepthStencilSurface when stereo is active). These enforce the
    // substitution invariants: the main stereo target always pairs with the
    // double-width depth buffer, and (M2, Config::offscreenPasses) the game's
    // own mid-frame targets get per-eye shadow copies. The caller holds
    // m_internal, so device calls here route to the originals.
    HRESULT HandleSetRenderTarget(IDirect3DDevice9* device, DWORD idx, IDirect3DSurface9* surf);
    HRESULT HandleSetDepthStencil(IDirect3DDevice9* device, IDirect3DSurface9* ds);

    // Texture tracking (fed by hkSetTexture): learns surface->texture
    // mappings and which sampler stages currently bind a shadowed surface,
    // so ApplyEye can swap in the per-eye copy.
    void OnGameSetTexture(DWORD stage, IDirect3DBaseTexture9* tex);

    // Clear routing (fed by hkClear): while a shadow pass is active the
    // target is per-eye, so Clear must run once per eye instead of once
    // across the double-width target.
    bool ShadowActive() const { return m_shadowActive != nullptr; }
    HRESULT ClearBothEyes(IDirect3DDevice9* device, DWORD count, const D3DRECT* rects,
                          DWORD flags, D3DCOLOR color, float z, DWORD stencil);

    // Perf instrumentation: hooks report time spent duplicating draws.
    void AddDuplicationTime(double microseconds, int drawCalls);

    // The stereo surface the OpenXR bridge reads from (M3).
    IDirect3DSurface9* StereoSurface() const { return m_stereoSurf; }
    int StereoHalfWidth() const;             // for per-eye rect offsets
    // Horizontal offset between eyes on the CURRENT target: half width on
    // the main stereo target, 0 while a per-eye shadow pass is bound.
    int ActiveEyeOffset() const;

private:
    StereoRenderer() = default;
    void CreateResources(IDirect3DDevice9* device);
    void ReleaseResources();
    void CompositeToBackbuffer(IDirect3DDevice9* device);

    // Double-width depth buffer for the main stereo target. D3D9 requires
    // the depth-stencil to be at least as large as the render target, so
    // substituting the 2W x H target without one fails SetRenderTarget.
    bool CreateStereoDepth(IDirect3DDevice9* device);

    // --- Offscreen shadow passes (M2) --------------------------------------
    // When the game re-points RT0 at its own surface S mid-frame (shadow
    // maps, reflections), we substitute per-eye render-target textures and
    // swap them in per eye in ApplyEye; sampler stages binding S's texture
    // get the matching eye copy. The game's own surface/texture is left
    // untouched - with the flag off none of this runs (today's behavior).
    struct SurfaceShadow
    {
        IDirect3DSurface9* gameSurf = nullptr;       // key (addref'd)
        IDirect3DTexture9* gameTex  = nullptr;       // learned at bind time
        IDirect3DTexture9* eyeTex[2]  = { nullptr, nullptr };
        IDirect3DSurface9* eyeSurf[2] = { nullptr, nullptr };
        IDirect3DSurface9* depth      = nullptr;     // shared, game-sized
    };
    SurfaceShadow* FindShadow(IDirect3DSurface9* gameSurf);
    SurfaceShadow* BeginShadowPass(IDirect3DDevice9* device, IDirect3DSurface9* gameSurf);
    void EndShadowPass();
    void ReleaseShadow(SurfaceShadow& s);
    void ReleaseAllShadows();
    bool EnsureShadowDepth(IDirect3DDevice9* device, SurfaceShadow& s, IDirect3DSurface9* gameDS);

    // M4: compose head pose delta onto the captured view (identity if
    // tracking disabled, no pose source, or menus are locked).
    D3DMATRIX ApplyHeadDelta(const D3DMATRIX& view);
    void CheckRecenterKey();

    // Debug: dump stereo surface to BMP ([Debug] DumpFrames).
    void DumpStereoSurface(IDirect3DDevice9* device, int index);
    int  m_dumpedFrames = 0;
    unsigned m_presentCount = 0;

    IDirect3DDevice9*  m_device     = nullptr;
    IDirect3DSurface9* m_stereoSurf = nullptr;   // 2W x H render target
    IDirect3DSurface9* m_stereoDS   = nullptr;   // 2W x H depth-stencil
    IDirect3DSurface9* m_gameRT     = nullptr;   // whatever RT0 the game expects
    IDirect3DSurface9* m_gameDS     = nullptr;   // depth the game intends bound
    D3DPRESENT_PARAMETERS m_pp      = {};

    D3DMATRIX  m_baseView  = {};
    bool       m_hasView   = false;
    D3DMATRIX  m_baseProj  = {};
    bool       m_hasProj   = false;
    D3DVIEWPORT9 m_gameVp  = {};
    bool       m_hasGameVp = false;
    RECT       m_gameScissor    = {};
    bool       m_hasScissor     = false;
    bool       m_scissorEnabled = false;

    // deque: m_shadowBinds and m_shadowActive hold pointers into this
    // container, so elements must never move on insertion.
    std::deque<SurfaceShadow> m_shadows;
    std::vector<std::pair<DWORD, SurfaceShadow*>> m_shadowBinds; // stage -> entry
    SurfaceShadow* m_shadowActive = nullptr;

    bool       m_active    = false;  // stereo substitution currently in effect
    bool       m_inScene   = false;
    int        m_loggedViews = 0;    // M0 debugging: log first N view matrices
    unsigned   m_viewCaptures = 0;   // total view captures (periodic logging)
    int        m_loggedEyes  = 0;    // TEMP DIAG: log first 20 per-eye uploads

    // Menu-lock detection (M4): last time the captured view actually changed.
    unsigned long long m_lastViewChangeTick = 0;
    bool               m_recenterKeyDown    = false;

    // Perf accumulators (reported every 600 frames at LogLevel>=2)
    double             m_dupTimeUs   = 0.0;
    int                m_dupCalls    = 0;
    int                m_frameCount  = 0;
    LARGE_INTEGER      m_qpcFreq     = {};
};
