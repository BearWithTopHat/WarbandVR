// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Minimal D3D9 shim for HOST (Linux) builds of the integration tests.
// Interfaces carry the FULL vtable (generated from mingw-w64 d3d9.h, order
// preserved) so the fake device satisfies everything DeviceProxy wraps.
// NOT used by production builds.

#include "windows.h"

typedef DWORD D3DCOLOR;

typedef enum D3DFORMAT { D3DFMT_UNKNOWN = 0, D3DFMT_X8R8G8B8 = 22, D3DFMT_A8R8G8B8 = 21,
                         D3DFMT_D24S8 = 75, D3DFMT_D32 = 71 } D3DFORMAT;
typedef enum D3DTRANSFORMSTATETYPE { D3DTS_VIEW = 2, D3DTS_PROJECTION = 3,
                                     D3DTS_WORLD = 256 } D3DTRANSFORMSTATETYPE;
typedef enum D3DPRIMITIVETYPE { D3DPT_POINTLIST = 1, D3DPT_TRIANGLELIST = 4,
                                D3DPT_TRIANGLESTRIP = 5 } D3DPRIMITIVETYPE;
typedef enum D3DPOOL { D3DPOOL_DEFAULT = 0, D3DPOOL_MANAGED = 1, D3DPOOL_SYSTEMMEM = 2 } D3DPOOL;
typedef enum D3DMULTISAMPLE_TYPE { D3DMULTISAMPLE_NONE = 0 } D3DMULTISAMPLE_TYPE;
typedef enum D3DBACKBUFFER_TYPE { D3DBACKBUFFER_TYPE_MONO = 0 } D3DBACKBUFFER_TYPE;
typedef enum D3DDEVTYPE { D3DDEVTYPE_HAL = 1 } D3DDEVTYPE;
typedef enum D3DRENDERSTATETYPE { D3DRS_ZENABLE = 7, D3DRS_SRCBLEND = 19, D3DRS_DESTBLEND = 20,
                                  D3DRS_ALPHABLENDENABLE = 27, D3DRS_SCISSORTESTENABLE = 174 } D3DRENDERSTATETYPE;
typedef enum D3DBLEND { D3DBLEND_ZERO = 1, D3DBLEND_ONE = 2, D3DBLEND_SRCALPHA = 5,
                        D3DBLEND_INVSRCALPHA = 6 } D3DBLEND;
typedef enum D3DSAMPLERSTATETYPE { D3DSAMP_ADDRESSU = 1, D3DSAMP_ADDRESSV = 2,
                                   D3DSAMP_MAGFILTER = 5, D3DSAMP_MINFILTER = 6 } D3DSAMPLERSTATETYPE;
typedef enum D3DTEXTUREFILTERTYPE { D3DTEXF_NONE = 0, D3DTEXF_POINT = 1, D3DTEXF_LINEAR = 2 } D3DTEXTUREFILTERTYPE;
typedef enum D3DTEXTUREADDRESS { D3DTADDRESS_WRAP = 1, D3DTADDRESS_CLAMP = 3 } D3DTEXTUREADDRESS;
typedef enum D3DTEXTURESTAGESTATETYPE { D3DTSS_COLOROP = 1, D3DTSS_COLORARG1 = 2, D3DTSS_COLORARG2 = 3,
                                        D3DTSS_ALPHAOP = 4, D3DTSS_ALPHAARG1 = 5, D3DTSS_ALPHAARG2 = 6 } D3DTEXTURESTAGESTATETYPE;
#define D3DTOP_MODULATE 4
#define D3DTA_TEXTURE 2
#define D3DTA_DIFFUSE 0
typedef enum D3DRESOURCETYPE { D3DRTYPE_SURFACE = 1, D3DRTYPE_TEXTURE = 3 } D3DRESOURCETYPE;
typedef enum D3DQUERYTYPE { D3DQUERYTYPE_EVENT = 8 } D3DQUERYTYPE;
typedef enum D3DSTATEBLOCKTYPE { D3DSBT_ALL = 1 } D3DSTATEBLOCKTYPE;
typedef enum D3DDECLUSAGE { D3DDECLUSAGE_POSITIONT = 9 } D3DDECLUSAGE;

#define D3DUSAGE_RENDERTARGET 0x00000001L
#define D3DLOCK_READONLY      0x00000010L
#define D3DISSUE_END          0x80000000L
#define D3DGETDATA_FLUSH      0x00000001L
#define D3DFVF_XYZRHW         0x004
#define D3DFVF_DIFFUSE        0x040
#define D3DFVF_TEX1           0x100
#define D3DCLEAR_TARGET       0x00000001L
#define D3DCLEAR_ZBUFFER      0x00000002L
#define D3DPRESENT_INTERVAL_IMMEDIATE 0x80000000L
#define D3DPRESENT_INTERVAL_DEFAULT   0x00000000L
#define D3DSWAPEFFECT_DISCARD         0x00000001L
#define D3D_OK S_OK

typedef struct _D3DMATRIX { float _11,_12,_13,_14,_21,_22,_23,_24,_31,_32,_33,_34,_41,_42,_43,_44; } D3DMATRIX;
typedef struct _D3DVIEWPORT9 { DWORD X, Y, Width, Height; float MinZ, MaxZ; } D3DVIEWPORT9;
typedef struct _D3DSURFACE_DESC {
    D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool;
    D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality;
    UINT Width; UINT Height;
} D3DSURFACE_DESC;
typedef struct _D3DVOLUME_DESC { D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool; UINT Width, Height, Depth; } D3DVOLUME_DESC;
typedef struct _D3DVERTEXBUFFER_DESC { D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool; UINT Size; DWORD FVF; } D3DVERTEXBUFFER_DESC;
typedef struct _D3DINDEXBUFFER_DESC { D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool; UINT Size; } D3DINDEXBUFFER_DESC;
typedef struct _D3DLOCKED_RECT { INT Pitch; void* pBits; } D3DLOCKED_RECT;
typedef struct _D3DLOCKED_BOX { INT RowPitch, SlicePitch; void* pBits; } D3DLOCKED_BOX;
typedef struct _D3DBOX { UINT Left, Top, Right, Bottom, Front, Back; } D3DBOX;
typedef struct _D3DRECT { LONG x1, y1, x2, y2; } D3DRECT;
typedef struct _D3DPRESENT_PARAMETERS {
    UINT BackBufferWidth, BackBufferHeight; D3DFORMAT BackBufferFormat; UINT BackBufferCount;
    D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality;
    DWORD SwapEffect; HWND hDeviceWindow; BOOL Windowed;
    BOOL EnableAutoDepthStencil; D3DFORMAT AutoDepthStencilFormat; DWORD Flags;
    UINT FullScreen_RefreshRateInHz; UINT PresentationInterval;
} D3DPRESENT_PARAMETERS;
typedef struct _D3DDISPLAYMODE { UINT Width, Height, RefreshRate; D3DFORMAT Format; } D3DDISPLAYMODE;
typedef enum D3DSCANLINEORDERING { D3DSCANLINEORDERING_UNKNOWN = 0, D3DSCANLINEORDERING_PROGRESSIVE = 1, D3DSCANLINEORDERING_INTERLACED = 2 } D3DSCANLINEORDERING;
typedef enum D3DDISPLAYROTATION { D3DDISPLAYROTATION_IDENTITY = 1 } D3DDISPLAYROTATION;
typedef struct _D3DDISPLAYMODEEX {
    UINT Size; UINT Width; UINT Height; UINT RefreshRate; D3DFORMAT Format; D3DSCANLINEORDERING ScanLineOrdering;
} D3DDISPLAYMODEEX;
typedef struct _D3DDISPLAYMODEFILTER {
    UINT Size; D3DFORMAT Format; D3DSCANLINEORDERING ScanLineOrdering;
} D3DDISPLAYMODEFILTER;
typedef enum D3DCOMPOSERECTSOP { D3DCOMPOSERECTS_COPY = 1, D3DCOMPOSERECTS_OR = 2, D3DCOMPOSERECTS_AND = 3, D3DCOMPOSERECTS_NEG = 4 } D3DCOMPOSERECTSOP;
typedef UINT UINT32;
#define D3DERR_INVALIDCALL E_FAIL
typedef struct _D3DADAPTER_IDENTIFIER9 { char Driver[512]; char Description[512]; char DeviceName[32]; } D3DADAPTER_IDENTIFIER9;
typedef struct _D3DCAPS9 { char opaque[304]; } D3DCAPS9;
typedef struct _D3DRASTER_STATUS { BOOL InVBlank; UINT ScanLine; } D3DRASTER_STATUS;
typedef struct _D3DDEVICE_CREATION_PARAMETERS { UINT AdapterOrdinal; D3DDEVTYPE DeviceType; HWND hFocusWindow; DWORD BehaviorFlags; } D3DDEVICE_CREATION_PARAMETERS;
typedef struct _D3DCLIPSTATUS9 { DWORD ClipUnion, ClipIntersection; } D3DCLIPSTATUS9;
typedef struct _D3DLIGHT9 { char opaque[104]; } D3DLIGHT9;
typedef struct _D3DMATERIAL9 { char opaque[68]; } D3DMATERIAL9;
typedef struct _D3DRECTPATCH_INFO { UINT StartVertexOffsetWidth, StartVertexOffsetHeight; UINT Width, Height; UINT Stride; DWORD Basis; DWORD Degree; } D3DRECTPATCH_INFO;
typedef struct _D3DTRIPATCH_INFO { UINT StartVertexOffset; UINT NumVertices; DWORD Basis; DWORD Degree; } D3DTRIPATCH_INFO;
typedef struct _D3DGAMMARAMP { WORD red[256], green[256], blue[256]; } D3DGAMMARAMP;
typedef struct _D3DVERTEXELEMENT9 { WORD Stream, Offset; BYTE Type, Method, Usage, UsageIndex; } D3DVERTEXELEMENT9;

typedef const wchar_t* LPCWSTR;
#ifndef WBVR_SHIM_NO_EXPORT_DECLS
inline void D3DPERF_BeginEvent(DWORD, LPCWSTR) {}
inline void D3DPERF_EndEvent() {}
inline INT  D3DPERF_GetStatus() { return 0; }
inline BOOL D3DPERF_QueryRepeatFrame() { return FALSE; }
inline void D3DPERF_SetMarker(DWORD, LPCWSTR) {}
inline void D3DPERF_SetOptions(DWORD) {}
inline void D3DPERF_SetRegion(DWORD, LPCWSTR) {}
#endif


struct IDirect3D9; struct IDirect3D9Ex; struct IDirect3DDevice9; struct IDirect3DDevice9Ex; struct IDirect3DSwapChain9;
struct IDirect3DResource9; struct IDirect3DBaseTexture9; struct IDirect3DTexture9;
struct IDirect3DCubeTexture9; struct IDirect3DVolumeTexture9; struct IDirect3DSurface9;
struct IDirect3DVolume9; struct IDirect3DVertexBuffer9; struct IDirect3DIndexBuffer9;
struct IDirect3DVertexShader9; struct IDirect3DPixelShader9; struct IDirect3DVertexDeclaration9;
struct IDirect3DStateBlock9; struct IDirect3DQuery9;

struct IUnknown {
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual ~IUnknown() {}
};

struct IDirect3D9 : public IUnknown
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall RegisterSoftwareDevice(void* pInitializeFunction) = 0;
    virtual UINT __stdcall GetAdapterCount() = 0;
    virtual HRESULT __stdcall GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) = 0;
    virtual UINT __stdcall GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) = 0;
    virtual HRESULT __stdcall EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) = 0;
    virtual HRESULT __stdcall GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode) = 0;
    virtual HRESULT __stdcall CheckDeviceType(UINT iAdapter, D3DDEVTYPE DevType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, WINBOOL bWindowed) = 0;
    virtual HRESULT __stdcall CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) = 0;
    virtual HRESULT __stdcall CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, WINBOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) = 0;
    virtual HRESULT __stdcall CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) = 0;
    virtual HRESULT __stdcall CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) = 0;
    virtual HRESULT __stdcall GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) = 0;
    virtual HMONITOR __stdcall GetAdapterMonitor(UINT Adapter) = 0;
    virtual HRESULT __stdcall CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) = 0;
};

struct IDirect3D9Ex : public IDirect3D9
{
    virtual UINT __stdcall GetAdapterModeCountEx(UINT Adapter, const D3DDISPLAYMODEFILTER* pFilter) = 0;
    virtual HRESULT __stdcall EnumAdapterModesEx(UINT Adapter, const D3DDISPLAYMODEFILTER* pFilter, UINT Mode, D3DDISPLAYMODEEX* pMode) = 0;
    virtual HRESULT __stdcall GetAdapterDisplayModeEx(UINT Adapter, D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation) = 0;
    virtual HRESULT __stdcall CreateDeviceEx(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface) = 0;
    virtual HRESULT __stdcall GetAdapterLUID(UINT Adapter, LUID* pLUID) = 0;
};

struct IDirect3DDevice9 : public IUnknown
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall TestCooperativeLevel() = 0;
    virtual UINT __stdcall GetAvailableTextureMem() = 0;
    virtual HRESULT __stdcall EvictManagedResources() = 0;
    virtual HRESULT __stdcall GetDirect3D(IDirect3D9** ppD3D9) = 0;
    virtual HRESULT __stdcall GetDeviceCaps(D3DCAPS9* pCaps) = 0;
    virtual HRESULT __stdcall GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) = 0;
    virtual HRESULT __stdcall GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) = 0;
    virtual HRESULT __stdcall SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) = 0;
    virtual void __stdcall SetCursorPosition(int X,int Y, DWORD Flags) = 0;
    virtual WINBOOL __stdcall ShowCursor(WINBOOL bShow) = 0;
    virtual HRESULT __stdcall CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain) = 0;
    virtual HRESULT __stdcall GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) = 0;
    virtual UINT __stdcall GetNumberOfSwapChains() = 0;
    virtual HRESULT __stdcall Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) = 0;
    virtual HRESULT __stdcall Present(const RECT *src_rect, const RECT *dst_rect, HWND dst_window_override, const RGNDATA *dirty_region) = 0;
    virtual HRESULT __stdcall GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) = 0;
    virtual HRESULT __stdcall GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) = 0;
    virtual HRESULT __stdcall SetDialogBoxMode(WINBOOL bEnableDialogs) = 0;
    virtual void __stdcall SetGammaRamp(UINT swapchain_idx, DWORD flags, const D3DGAMMARAMP *ramp) = 0;
    virtual void __stdcall GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) = 0;
    virtual HRESULT __stdcall CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) = 0;
    virtual HRESULT __stdcall CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) = 0;
    virtual HRESULT __stdcall CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) = 0;
    virtual HRESULT __stdcall CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) = 0;
    virtual HRESULT __stdcall CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) = 0;
    virtual HRESULT __stdcall CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, WINBOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) = 0;
    virtual HRESULT __stdcall CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, WINBOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) = 0;
    virtual HRESULT __stdcall UpdateSurface(IDirect3DSurface9 *src_surface, const RECT *src_rect, IDirect3DSurface9 *dst_surface, const POINT *dst_point) = 0;
    virtual HRESULT __stdcall UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) = 0;
    virtual HRESULT __stdcall GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) = 0;
    virtual HRESULT __stdcall GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) = 0;
    virtual HRESULT __stdcall StretchRect(IDirect3DSurface9 *src_surface, const RECT *src_rect, IDirect3DSurface9 *dst_surface, const RECT *dst_rect, D3DTEXTUREFILTERTYPE filter) = 0;
    virtual HRESULT __stdcall ColorFill(IDirect3DSurface9 *surface, const RECT *rect, D3DCOLOR color) = 0;
    virtual HRESULT __stdcall CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) = 0;
    virtual HRESULT __stdcall SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) = 0;
    virtual HRESULT __stdcall GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) = 0;
    virtual HRESULT __stdcall SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) = 0;
    virtual HRESULT __stdcall GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) = 0;
    virtual HRESULT __stdcall BeginScene() = 0;
    virtual HRESULT __stdcall EndScene() = 0;
    virtual HRESULT __stdcall Clear(DWORD rect_count, const D3DRECT *rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil) = 0;
    virtual HRESULT __stdcall SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX *matrix) = 0;
    virtual HRESULT __stdcall GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) = 0;
    virtual HRESULT __stdcall MultiplyTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX *matrix) = 0;
    virtual HRESULT __stdcall SetViewport(const D3DVIEWPORT9 *viewport) = 0;
    virtual HRESULT __stdcall GetViewport(D3DVIEWPORT9* pViewport) = 0;
    virtual HRESULT __stdcall SetMaterial(const D3DMATERIAL9 *material) = 0;
    virtual HRESULT __stdcall GetMaterial(D3DMATERIAL9* pMaterial) = 0;
    virtual HRESULT __stdcall SetLight(DWORD index, const D3DLIGHT9 *light) = 0;
    virtual HRESULT __stdcall GetLight(DWORD Index, D3DLIGHT9*) = 0;
    virtual HRESULT __stdcall LightEnable(DWORD Index, WINBOOL Enable) = 0;
    virtual HRESULT __stdcall GetLightEnable(DWORD Index, WINBOOL* pEnable) = 0;
    virtual HRESULT __stdcall SetClipPlane(DWORD index, const float *plane) = 0;
    virtual HRESULT __stdcall GetClipPlane(DWORD Index, float* pPlane) = 0;
    virtual HRESULT __stdcall SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) = 0;
    virtual HRESULT __stdcall GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) = 0;
    virtual HRESULT __stdcall CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) = 0;
    virtual HRESULT __stdcall BeginStateBlock() = 0;
    virtual HRESULT __stdcall EndStateBlock(IDirect3DStateBlock9** ppSB) = 0;
    virtual HRESULT __stdcall SetClipStatus(const D3DCLIPSTATUS9 *clip_status) = 0;
    virtual HRESULT __stdcall GetClipStatus(D3DCLIPSTATUS9* pClipStatus) = 0;
    virtual HRESULT __stdcall GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) = 0;
    virtual HRESULT __stdcall SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) = 0;
    virtual HRESULT __stdcall GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) = 0;
    virtual HRESULT __stdcall SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) = 0;
    virtual HRESULT __stdcall GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) = 0;
    virtual HRESULT __stdcall SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) = 0;
    virtual HRESULT __stdcall ValidateDevice(DWORD* pNumPasses) = 0;
    virtual HRESULT __stdcall SetPaletteEntries(UINT palette_idx, const PALETTEENTRY *entries) = 0;
    virtual HRESULT __stdcall GetPaletteEntries(UINT PaletteNumber,PALETTEENTRY* pEntries) = 0;
    virtual HRESULT __stdcall SetCurrentTexturePalette(UINT PaletteNumber) = 0;
    virtual HRESULT __stdcall GetCurrentTexturePalette(UINT *PaletteNumber) = 0;
    virtual HRESULT __stdcall SetScissorRect(const RECT *rect) = 0;
    virtual HRESULT __stdcall GetScissorRect(RECT* pRect) = 0;
    virtual HRESULT __stdcall SetSoftwareVertexProcessing(WINBOOL bSoftware) = 0;
    virtual WINBOOL __stdcall GetSoftwareVertexProcessing() = 0;
    virtual HRESULT __stdcall SetNPatchMode(float nSegments) = 0;
    virtual float __stdcall GetNPatchMode() = 0;
    virtual HRESULT __stdcall DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) = 0;
    virtual HRESULT __stdcall DrawIndexedPrimitive(D3DPRIMITIVETYPE, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) = 0;
    virtual HRESULT __stdcall DrawPrimitiveUP(D3DPRIMITIVETYPE primitive_type, UINT primitive_count, const void *data, UINT stride) = 0;
    virtual HRESULT __stdcall DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitive_type, UINT min_vertex_idx, UINT vertex_count, UINT primitive_count, const void *index_data, D3DFORMAT index_format, const void *data, UINT stride) = 0;
    virtual HRESULT __stdcall ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) = 0;
    virtual HRESULT __stdcall CreateVertexDeclaration(const D3DVERTEXELEMENT9 *elements, IDirect3DVertexDeclaration9 **declaration) = 0;
    virtual HRESULT __stdcall SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) = 0;
    virtual HRESULT __stdcall GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) = 0;
    virtual HRESULT __stdcall SetFVF(DWORD FVF) = 0;
    virtual HRESULT __stdcall GetFVF(DWORD* pFVF) = 0;
    virtual HRESULT __stdcall CreateVertexShader(const DWORD *byte_code, IDirect3DVertexShader9 **shader) = 0;
    virtual HRESULT __stdcall SetVertexShader(IDirect3DVertexShader9* pShader) = 0;
    virtual HRESULT __stdcall GetVertexShader(IDirect3DVertexShader9** ppShader) = 0;
    virtual HRESULT __stdcall SetVertexShaderConstantF(UINT reg_idx, const float *data, UINT count) = 0;
    virtual HRESULT __stdcall GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) = 0;
    virtual HRESULT __stdcall SetVertexShaderConstantI(UINT reg_idx, const int *data, UINT count) = 0;
    virtual HRESULT __stdcall GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) = 0;
    virtual HRESULT __stdcall SetVertexShaderConstantB(UINT reg_idx, const WINBOOL *data, UINT count) = 0;
    virtual HRESULT __stdcall GetVertexShaderConstantB(UINT StartRegister, WINBOOL* pConstantData, UINT BoolCount) = 0;
    virtual HRESULT __stdcall SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) = 0;
    virtual HRESULT __stdcall GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* OffsetInBytes, UINT* pStride) = 0;
    virtual HRESULT __stdcall SetStreamSourceFreq(UINT StreamNumber, UINT Divider) = 0;
    virtual HRESULT __stdcall GetStreamSourceFreq(UINT StreamNumber, UINT* Divider) = 0;
    virtual HRESULT __stdcall SetIndices(IDirect3DIndexBuffer9* pIndexData) = 0;
    virtual HRESULT __stdcall GetIndices(IDirect3DIndexBuffer9** ppIndexData) = 0;
    virtual HRESULT __stdcall CreatePixelShader(const DWORD *byte_code, IDirect3DPixelShader9 **shader) = 0;
    virtual HRESULT __stdcall SetPixelShader(IDirect3DPixelShader9* pShader) = 0;
    virtual HRESULT __stdcall GetPixelShader(IDirect3DPixelShader9** ppShader) = 0;
    virtual HRESULT __stdcall SetPixelShaderConstantF(UINT reg_idx, const float *data, UINT count) = 0;
    virtual HRESULT __stdcall GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) = 0;
    virtual HRESULT __stdcall SetPixelShaderConstantI(UINT reg_idx, const int *data, UINT count) = 0;
    virtual HRESULT __stdcall GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) = 0;
    virtual HRESULT __stdcall SetPixelShaderConstantB(UINT reg_idx, const WINBOOL *data, UINT count) = 0;
    virtual HRESULT __stdcall GetPixelShaderConstantB(UINT StartRegister, WINBOOL* pConstantData, UINT BoolCount) = 0;
    virtual HRESULT __stdcall DrawRectPatch(UINT handle, const float *segment_count, const D3DRECTPATCH_INFO *patch_info) = 0;
    virtual HRESULT __stdcall DrawTriPatch(UINT handle, const float *segment_count, const D3DTRIPATCH_INFO *patch_info) = 0;
    virtual HRESULT __stdcall DeletePatch(UINT Handle) = 0;
    virtual HRESULT __stdcall CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) = 0;
};

struct IDirect3DSwapChain9 : public IUnknown
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void **ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall Present(const RECT *src_rect, const RECT *dst_rect, HWND dst_window_override, const RGNDATA *dirty_region, DWORD flags) = 0;
    virtual HRESULT __stdcall GetFrontBufferData(IDirect3DSurface9 *pDestSurface) = 0;
    virtual HRESULT __stdcall GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) = 0;
    virtual HRESULT __stdcall GetRasterStatus(D3DRASTER_STATUS *pRasterStatus) = 0;
    virtual HRESULT __stdcall GetDisplayMode(D3DDISPLAYMODE *pMode) = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9 **ppDevice) = 0;
    virtual HRESULT __stdcall GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters) = 0;
};

struct IDirect3DResource9 : public IUnknown
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data, DWORD data_size, DWORD flags) = 0;
    virtual HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) = 0;
    virtual HRESULT __stdcall FreePrivateData(REFGUID refguid) = 0;
    virtual DWORD __stdcall SetPriority(DWORD PriorityNew) = 0;
    virtual DWORD __stdcall GetPriority() = 0;
    virtual void __stdcall PreLoad() = 0;
    virtual D3DRESOURCETYPE __stdcall GetType() = 0;
};

struct IDirect3DBaseTexture9 : public IDirect3DResource9
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data, DWORD data_size, DWORD flags) = 0;
    virtual HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) = 0;
    virtual HRESULT __stdcall FreePrivateData(REFGUID refguid) = 0;
    virtual DWORD __stdcall SetPriority(DWORD PriorityNew) = 0;
    virtual DWORD __stdcall GetPriority() = 0;
    virtual void __stdcall PreLoad() = 0;
    virtual D3DRESOURCETYPE __stdcall GetType() = 0;
    virtual DWORD __stdcall SetLOD(DWORD LODNew) = 0;
    virtual DWORD __stdcall GetLOD() = 0;
    virtual DWORD __stdcall GetLevelCount() = 0;
    virtual HRESULT __stdcall SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) = 0;
    virtual D3DTEXTUREFILTERTYPE __stdcall GetAutoGenFilterType() = 0;
    virtual void __stdcall GenerateMipSubLevels() = 0;
};

struct IDirect3DTexture9 : public IDirect3DBaseTexture9
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data, DWORD data_size, DWORD flags) = 0;
    virtual HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) = 0;
    virtual HRESULT __stdcall FreePrivateData(REFGUID refguid) = 0;
    virtual DWORD __stdcall SetPriority(DWORD PriorityNew) = 0;
    virtual DWORD __stdcall GetPriority() = 0;
    virtual void __stdcall PreLoad() = 0;
    virtual D3DRESOURCETYPE __stdcall GetType() = 0;
    virtual DWORD __stdcall SetLOD(DWORD LODNew) = 0;
    virtual DWORD __stdcall GetLOD() = 0;
    virtual DWORD __stdcall GetLevelCount() = 0;
    virtual HRESULT __stdcall SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) = 0;
    virtual D3DTEXTUREFILTERTYPE __stdcall GetAutoGenFilterType() = 0;
    virtual void __stdcall GenerateMipSubLevels() = 0;
    virtual HRESULT __stdcall GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) = 0;
    virtual HRESULT __stdcall GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) = 0;
    virtual HRESULT __stdcall LockRect(UINT level, D3DLOCKED_RECT *locked_rect, const RECT *rect, DWORD flags) = 0;
    virtual HRESULT __stdcall UnlockRect(UINT Level) = 0;
    virtual HRESULT __stdcall AddDirtyRect(const RECT *dirty_rect) = 0;
};

struct IDirect3DSurface9 : public IDirect3DResource9
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data, DWORD data_size, DWORD flags) = 0;
    virtual HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) = 0;
    virtual HRESULT __stdcall FreePrivateData(REFGUID refguid) = 0;
    virtual DWORD __stdcall SetPriority(DWORD PriorityNew) = 0;
    virtual DWORD __stdcall GetPriority() = 0;
    virtual void __stdcall PreLoad() = 0;
    virtual D3DRESOURCETYPE __stdcall GetType() = 0;
    virtual HRESULT __stdcall GetContainer(REFIID riid, void** ppContainer) = 0;
    virtual HRESULT __stdcall GetDesc(D3DSURFACE_DESC* pDesc) = 0;
    virtual HRESULT __stdcall LockRect(D3DLOCKED_RECT *locked_rect, const RECT *rect, DWORD flags) = 0;
    virtual HRESULT __stdcall UnlockRect() = 0;
    virtual HRESULT __stdcall GetDC(HDC* phdc) = 0;
    virtual HRESULT __stdcall ReleaseDC(HDC hdc) = 0;
};

struct IDirect3DVolume9 : public IUnknown
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data, DWORD data_size, DWORD flags) = 0;
    virtual HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) = 0;
    virtual HRESULT __stdcall FreePrivateData(REFGUID refguid) = 0;
    virtual HRESULT __stdcall GetContainer(REFIID riid, void** ppContainer) = 0;
    virtual HRESULT __stdcall GetDesc(D3DVOLUME_DESC* pDesc) = 0;
    virtual HRESULT __stdcall LockBox(D3DLOCKED_BOX *locked_box, const D3DBOX *box, DWORD flags) = 0;
    virtual HRESULT __stdcall UnlockBox() = 0;
};

struct IDirect3DVertexBuffer9 : public IDirect3DResource9
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data, DWORD data_size, DWORD flags) = 0;
    virtual HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) = 0;
    virtual HRESULT __stdcall FreePrivateData(REFGUID refguid) = 0;
    virtual DWORD __stdcall SetPriority(DWORD PriorityNew) = 0;
    virtual DWORD __stdcall GetPriority() = 0;
    virtual void __stdcall PreLoad() = 0;
    virtual D3DRESOURCETYPE __stdcall GetType() = 0;
    virtual HRESULT __stdcall Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, DWORD Flags) = 0;
    virtual HRESULT __stdcall Unlock() = 0;
    virtual HRESULT __stdcall GetDesc(D3DVERTEXBUFFER_DESC* pDesc) = 0;
};

struct IDirect3DIndexBuffer9 : public IDirect3DResource9
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data, DWORD data_size, DWORD flags) = 0;
    virtual HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) = 0;
    virtual HRESULT __stdcall FreePrivateData(REFGUID refguid) = 0;
    virtual DWORD __stdcall SetPriority(DWORD PriorityNew) = 0;
    virtual DWORD __stdcall GetPriority() = 0;
    virtual void __stdcall PreLoad() = 0;
    virtual D3DRESOURCETYPE __stdcall GetType() = 0;
    virtual HRESULT __stdcall Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, DWORD Flags) = 0;
    virtual HRESULT __stdcall Unlock() = 0;
    virtual HRESULT __stdcall GetDesc(D3DINDEXBUFFER_DESC* pDesc) = 0;
};

struct IDirect3DVertexShader9 : public IUnknown
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall GetFunction(void*, UINT* pSizeOfData) = 0;
};

struct IDirect3DPixelShader9 : public IUnknown
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall GetFunction(void*, UINT* pSizeOfData) = 0;
};

struct IDirect3DVertexDeclaration9 : public IUnknown
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall GetDeclaration(D3DVERTEXELEMENT9*, UINT* pNumElements) = 0;
};

struct IDirect3DStateBlock9 : public IUnknown
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT __stdcall Capture() = 0;
    virtual HRESULT __stdcall Apply() = 0;
};

struct IDirect3DQuery9 : public IUnknown
{
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual D3DQUERYTYPE __stdcall GetType() = 0;
    virtual DWORD __stdcall GetDataSize() = 0;
    virtual HRESULT __stdcall Issue(DWORD dwIssueFlags) = 0;
    virtual HRESULT __stdcall GetData(void* pData, DWORD dwSize, DWORD dwGetDataFlags) = 0;
};

struct IDirect3DDevice9Ex : public IDirect3DDevice9
{
    virtual HRESULT __stdcall SetConvolutionMonoKernel(UINT width, UINT height, float* rows, float* columns) = 0;
    virtual HRESULT __stdcall ComposeRects(IDirect3DSurface9* pSrc, IDirect3DSurface9* pDst, IDirect3DVertexBuffer9* pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9* pDstRectDescs, D3DCOMPOSERECTSOP Operation, int Xoffset, int Yoffset) = 0;
    virtual HRESULT __stdcall PresentEx(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags) = 0;
    virtual HRESULT __stdcall GetGPUThreadPriority(INT* pPriority) = 0;
    virtual HRESULT __stdcall SetGPUThreadPriority(INT Priority) = 0;
    virtual HRESULT __stdcall WaitForVBlank(UINT iSwapChain) = 0;
    virtual HRESULT __stdcall CheckResourceResidency(IDirect3DResource9** pResourceArray, UINT32 NumResources) = 0;
    virtual HRESULT __stdcall SetMaximumFrameLatency(UINT MaxLatency) = 0;
    virtual HRESULT __stdcall GetMaximumFrameLatency(UINT* pMaxLatency) = 0;
    virtual HRESULT __stdcall CheckDeviceState(HWND hDestinationWindow) = 0;
    virtual HRESULT __stdcall CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, WINBOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage) = 0;
    virtual HRESULT __stdcall CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage) = 0;
    virtual HRESULT __stdcall CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, WINBOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage) = 0;
    virtual HRESULT __stdcall ResetEx(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode) = 0;
    virtual HRESULT __stdcall GetDisplayModeEx(UINT iSwapChain, D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation) = 0;
};

extern const IID IID_IUnknown;
extern const IID IID_IDirect3D9;
extern const IID IID_IDirect3D9Ex;
extern const IID IID_IDirect3DDevice9;
extern const IID IID_IDirect3DTexture9;
extern const IID IID_IDirect3DDevice9Ex;

typedef IDirect3D9* (__stdcall *PFN_Direct3DCreate9)(UINT SDKVersion);
#ifndef WBVR_SHIM_NO_EXPORT_DECLS
IDirect3D9* __stdcall Direct3DCreate9(UINT SDKVersion);
#endif
#define D3D_SDK_VERSION 32
#define D3D9b_SDK_VERSION 31
