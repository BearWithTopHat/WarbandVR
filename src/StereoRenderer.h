#pragma once

// StereoRenderer: owns the per-eye render targets and all per-eye state math.
//
// Model (2026-10-01 iteration 6 - "per-eye surfaces"):
//  - Each eye gets a GAME-SIZED render target and depth buffer. Every
//    intercepted main-frame draw call executes twice: once into the left
//    eye's target, once into the right's, each with an eye-shifted view
//    matrix (and optionally an off-center projection).
//  - Game-sized targets are the whole point: the game's own coordinate
//    space (viewports, scissor, RHW/pre-transformed vertices, Z-prepass +
//    EQUAL color pass, offscreen passes) works per eye exactly as it does
//    mono. The previous single double-width target broke all of those -
//    most subtly the Z-prepass, which is mono and only ever fed the left
//    half's depth, killing every right-eye draw that tested EQUAL.
//  - Scenes whose RT0 is NOT main-sized (the 256x256 shadow map etc.) pass
//    through untouched: they are the game's own business, and their
//    textures keep their content for the game's lighting pass.
//  - Draws that sample a "diverted" main-sized surface (one whose scene we
//    rerouted to the eye targets, e.g. the world texture feeding the final
//    composite blit) are SKIPPED - the content they would blit is already
//    in the eye targets; drawing it would paint an empty surface over the
//    frame.
//  - At Present the two eye targets are composited side-by-side onto the
//    real backbuffer (M1) or handed to the OpenXR bridge (M3).
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
    void OnBeginScene(IDirect3DDevice9* device);   // clear eye targets once/frame
    void OnEndScene(IDirect3DDevice9* device);     // perf accounting
    void OnPresent(IDirect3DDevice9* device);      // composite SBS to backbuffer

    // Per-draw-call API -----------------------------------------------------
    bool IsActive() const { return m_active; }
    void ApplyEye(IDirect3DDevice9* device, int eye);   // target + viewport + matrices
    void RestoreGameState(IDirect3DDevice9* device);    // undo viewport/matrices

    // RT0 tracking (fed by hkSetRenderTarget): we need the current target's
    // size to decide whether this scene is main-frame (duplicate per eye)
    // or the game's own offscreen business (pass through).
    void SetCurrentRT(IDirect3DSurface9* surf);
    bool CurrentRTIsMain() const { return m_curRTIsMain; }

    // "Diverted" surfaces: main-sized game surfaces that were RT0 while
    // stereo was active, so their scenes went to the eye targets and the
    // surface itself stayed empty. Draws sampling them must be skipped.
    bool IsDiverted(IDirect3DSurface9* surf) const;

    // Texture tracking (fed by hkSetTexture): notes sampler stages binding a
    // diverted surface, and (M2 shadow passes) per-eye shadow copies.
    void OnGameSetTexture(DWORD stage, IDirect3DBaseTexture9* tex);
    bool DivertedTextureBound() const { return !m_divertedBinds.empty(); }

    // Matrix capture (fed by the SetTransform / VS-constant hooks)
    void CaptureViewMatrix(const D3DMATRIX& m);
    void CaptureProjectionMatrix(const D3DMATRIX& m);
    const D3DMATRIX& BaseView() const { return m_baseView; }
    bool HasView() const { return m_hasView; }

    // Game viewport tracking (fed by hkSetViewport when not internal)
    void SetGameViewport(const D3DVIEWPORT9& vp);

    // Scissor tracking: the game's scissor rect is stored and re-applied per
    // eye (each eye target is game-sized, so it is used as-is).
    void SetGameScissorRect(const RECT& r);
    void SetScissorTestEnabled(bool enabled);

    // Perf instrumentation: hooks report time spent duplicating draws.
    void AddDuplicationTime(double microseconds, int drawCalls);

    // The eye surfaces (M3 reads these; index 0 = left, 1 = right).
    IDirect3DSurface9* EyeSurface(int eye) const { return (eye >= 0 && eye < 2) ? m_eyeSurf[eye] : nullptr; }

    // --- Offscreen shadow passes (M2, Config::offscreenPasses) -------------
    // When the game re-points RT0 at its own surface S mid-frame, we
    // substitute per-eye render-target textures and swap them in per eye in
    // ApplyEye; sampler stages binding S's texture get the matching eye
    // copy. Disabled by default.
    struct SurfaceShadow
    {
        IDirect3DSurface9* gameSurf = nullptr;
        IDirect3DTexture9* gameTex  = nullptr;
        IDirect3DTexture9* eyeTex[2]  = { nullptr, nullptr };
        IDirect3DSurface9* eyeSurf[2] = { nullptr, nullptr };
        IDirect3DSurface9* depth      = nullptr;
    };
    SurfaceShadow* FindShadow(IDirect3DSurface9* gameSurf);
    SurfaceShadow* BeginShadowPass(IDirect3DDevice9* device, IDirect3DSurface9* gameSurf);
    void EndShadowPass();
    void ReleaseShadow(SurfaceShadow& s);
    void ReleaseAllShadows();
    bool EnsureShadowDepth(IDirect3DDevice9* device, SurfaceShadow& s, IDirect3DSurface9* gameDS);
    bool ShadowActive() const { return m_shadowActive != nullptr; }
    HRESULT ClearBothEyes(IDirect3DDevice9* device, DWORD count, const D3DRECT* rects,
                          DWORD flags, D3DCOLOR color, float z, DWORD stencil);

private:
    StereoRenderer() = default;
    void CreateResources(IDirect3DDevice9* device);
    void ReleaseResources();
    void CompositeToBackbuffer(IDirect3DDevice9* device);
    void ClearEyeTargets(IDirect3DDevice9* device);

    // M4: compose head pose delta onto the captured view (identity if
    // tracking disabled, no pose source, or menus are locked).
    D3DMATRIX ApplyHeadDelta(const D3DMATRIX& view);
    void CheckRecenterKey();

    // Debug: dump eye surfaces to BMPs ([Debug] DumpFrames).
    void DumpStereoSurface(IDirect3DDevice9* device, int index);
    int  m_dumpedFrames = 0;
    unsigned m_presentCount = 0;

    IDirect3DDevice9*  m_device     = nullptr;
    IDirect3DSurface9* m_eyeSurf[2] = { nullptr, nullptr };  // per-eye game-sized RTs
    IDirect3DSurface9* m_eyeDS[2]   = { nullptr, nullptr };  // per-eye game-sized depth
    D3DPRESENT_PARAMETERS m_pp      = {};
    D3DFORMAT   m_dsFormat          = D3DFMT_D24S8;
    UINT        m_eyeW              = 0;      // eye target dims (= game backbuffer dims)
    UINT        m_eyeH              = 0;
    bool        m_needClear         = true;   // eye targets need clearing at next frame

    // RT0 state tracking (no substitution anymore - just knowledge)
    IDirect3DSurface9* m_curRT      = nullptr; // weak ref; not addref'd
    bool        m_curRTIsMain       = false;
    std::vector<IDirect3DSurface9*> m_diverted;    // weak refs; cleared on Reset
    std::vector<DWORD>              m_divertedBinds; // sampler stages binding diverted surfaces

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

    bool       m_active    = false;  // stereo duplication currently in effect
    bool       m_inScene   = false;
    int        m_loggedViews = 0;    // M0 debugging: log first N view matrices
    unsigned   m_viewCaptures = 0;   // total view captures (periodic logging)
    int        m_loggedEyes  = 0;    // TEMP DIAG: log first 40 per-eye uploads

    // Menu-lock detection (M4): last time the captured view actually changed.
    unsigned long long m_lastViewChangeTick = 0;
    bool               m_recenterKeyDown    = false;

    // Perf accumulators (reported every 600 frames at LogLevel>=2)
    double             m_dupTimeUs   = 0.0;
    int                m_dupCalls    = 0;
    int                m_frameCount  = 0;
    LARGE_INTEGER      m_qpcFreq     = {};
};
