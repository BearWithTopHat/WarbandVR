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

    // Perf instrumentation: hooks report time spent duplicating draws.
    void AddDuplicationTime(double microseconds, int drawCalls);

    // The stereo surface the OpenXR bridge reads from (M3).
    IDirect3DSurface9* StereoSurface() const { return m_stereoSurf; }
    int StereoHalfWidth() const;             // for per-eye rect offsets

private:
    StereoRenderer() = default;
    void CreateResources(IDirect3DDevice9* device);
    void ReleaseResources();
    void CompositeToBackbuffer(IDirect3DDevice9* device);

    // M4: compose head pose delta onto the captured view (identity if
    // tracking disabled, no pose source, or menus are locked).
    D3DMATRIX ApplyHeadDelta(const D3DMATRIX& view);
    void CheckRecenterKey();

    IDirect3DDevice9*  m_device     = nullptr;
    IDirect3DSurface9* m_stereoSurf = nullptr;   // 2W x H render target
    IDirect3DSurface9* m_gameRT     = nullptr;   // whatever RT0 the game expects
    D3DPRESENT_PARAMETERS m_pp      = {};

    D3DMATRIX  m_baseView  = {};
    bool       m_hasView   = false;
    D3DMATRIX  m_baseProj  = {};
    bool       m_hasProj   = false;
    D3DVIEWPORT9 m_gameVp  = {};
    bool       m_hasGameVp = false;

    bool       m_active    = false;  // stereo substitution currently in effect
    bool       m_inScene   = false;
    int        m_loggedViews = 0;    // M0 debugging: log first N view matrices

    // Menu-lock detection (M4): last time the captured view actually changed.
    unsigned long long m_lastViewChangeTick = 0;
    bool               m_recenterKeyDown    = false;

    // Perf accumulators (reported every 600 frames at LogLevel>=2)
    double             m_dupTimeUs   = 0.0;
    int                m_dupCalls    = 0;
    int                m_frameCount  = 0;
    LARGE_INTEGER      m_qpcFreq     = {};
};
