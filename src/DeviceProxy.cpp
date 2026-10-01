#include "DeviceProxy.h"
#include "DeviceHooks.h"
#include "Logger.h"

DeviceProxy::DeviceProxy(IDirect3DDevice9* real)
    : m_real(real), m_refCount(1)
{
    m_real->QueryInterface(IID_IDirect3DDevice9Ex, reinterpret_cast<void**>(&m_realEx));
}

DeviceProxy::~DeviceProxy()
{
    if (m_realEx) m_realEx->Release();
    m_real->Release();
}

HRESULT __stdcall DeviceProxy::QueryInterface(REFIID riid, void** ppvObj)
{
    if (riid == IID_IUnknown || riid == IID_IDirect3DDevice9)
    {
        *ppvObj = static_cast<IDirect3DDevice9*>(this);
        AddRef();
        return S_OK;
    }
    if (riid == IID_IDirect3DDevice9Ex && m_realEx)
    {
        *ppvObj = static_cast<IDirect3DDevice9Ex*>(this);
        AddRef();
        return S_OK;
    }
    // Anything else: ask the real object and return it unwrapped.
    // Calls through such interfaces bypass us.
    HRESULT hr = m_real->QueryInterface(riid, ppvObj);
    if (SUCCEEDED(hr))
        LOG_WARN("DeviceProxy: QueryInterface for unknown IID - returning UNWRAPPED object");
    return hr;
}

ULONG __stdcall DeviceProxy::AddRef()
{
    return ++m_refCount;
}

ULONG __stdcall DeviceProxy::Release()
{
    ULONG count = --m_refCount;
    if (count == 0)
        delete this;
    return count;
}

UINT __stdcall DeviceProxy::GetAvailableTextureMem()
{
    return m_real->GetAvailableTextureMem();
}

void __stdcall DeviceProxy::SetCursorPosition(int X, int Y, DWORD Flags)
{
    m_real->SetCursorPosition(X, Y, Flags);
}

BOOL __stdcall DeviceProxy::ShowCursor(BOOL bShow)
{
    return m_real->ShowCursor(bShow);
}

UINT __stdcall DeviceProxy::GetNumberOfSwapChains()
{
    return m_real->GetNumberOfSwapChains();
}

void __stdcall DeviceProxy::SetGammaRamp(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp)
{
    m_real->SetGammaRamp(iSwapChain, Flags, pRamp);
}

void __stdcall DeviceProxy::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp)
{
    m_real->GetGammaRamp(iSwapChain, pRamp);
}

BOOL __stdcall DeviceProxy::GetSoftwareVertexProcessing()
{
    return m_real->GetSoftwareVertexProcessing();
}

float __stdcall DeviceProxy::GetNPatchMode()
{
    return m_real->GetNPatchMode();
}

HRESULT __stdcall DeviceProxy::TestCooperativeLevel()
{
    return m_real->TestCooperativeLevel();
}

HRESULT __stdcall DeviceProxy::EvictManagedResources()
{
    return m_real->EvictManagedResources();
}

HRESULT __stdcall DeviceProxy::GetDirect3D(IDirect3D9** ppD3D9)
{
    return m_real->GetDirect3D(ppD3D9);
}

HRESULT __stdcall DeviceProxy::GetDeviceCaps(D3DCAPS9* pCaps)
{
    return m_real->GetDeviceCaps(pCaps);
}

HRESULT __stdcall DeviceProxy::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode)
{
    return m_real->GetDisplayMode(iSwapChain, pMode);
}

HRESULT __stdcall DeviceProxy::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
    return m_real->GetCreationParameters(pParameters);
}

HRESULT __stdcall DeviceProxy::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap)
{
    return m_real->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}

HRESULT __stdcall DeviceProxy::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain)
{
    return m_real->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain);
}

HRESULT __stdcall DeviceProxy::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain)
{
    return m_real->GetSwapChain(iSwapChain, pSwapChain);
}

HRESULT __stdcall DeviceProxy::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    return hkReset(m_real, pPresentationParameters);
}

HRESULT __stdcall DeviceProxy::Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
    return hkPresent(m_real, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT __stdcall DeviceProxy::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer)
{
    return m_real->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
}

HRESULT __stdcall DeviceProxy::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus)
{
    return m_real->GetRasterStatus(iSwapChain, pRasterStatus);
}

HRESULT __stdcall DeviceProxy::SetDialogBoxMode(BOOL bEnableDialogs)
{
    return m_real->SetDialogBoxMode(bEnableDialogs);
}

HRESULT __stdcall DeviceProxy::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle)
{
    return m_real->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
}

HRESULT __stdcall DeviceProxy::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle)
{
    return m_real->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
}

HRESULT __stdcall DeviceProxy::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle)
{
    return m_real->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
}

HRESULT __stdcall DeviceProxy::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle)
{
    return m_real->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
}

HRESULT __stdcall DeviceProxy::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle)
{
    return m_real->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
}

HRESULT __stdcall DeviceProxy::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{
    return m_real->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
}

HRESULT __stdcall DeviceProxy::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{
    return m_real->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
}

HRESULT __stdcall DeviceProxy::UpdateSurface(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint)
{
    return m_real->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
}

HRESULT __stdcall DeviceProxy::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture)
{
    return m_real->UpdateTexture(pSourceTexture, pDestinationTexture);
}

HRESULT __stdcall DeviceProxy::GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface)
{
    return m_real->GetRenderTargetData(pRenderTarget, pDestSurface);
}

HRESULT __stdcall DeviceProxy::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface)
{
    return m_real->GetFrontBufferData(iSwapChain, pDestSurface);
}

HRESULT __stdcall DeviceProxy::StretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter)
{
    return m_real->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
}

HRESULT __stdcall DeviceProxy::ColorFill(IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color)
{
    return m_real->ColorFill(pSurface, pRect, color);
}

HRESULT __stdcall DeviceProxy::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{
    return m_real->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
}

HRESULT __stdcall DeviceProxy::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget)
{
    return hkSetRenderTarget(m_real, RenderTargetIndex, pRenderTarget);
}

HRESULT __stdcall DeviceProxy::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget)
{
    return m_real->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}

HRESULT __stdcall DeviceProxy::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
{
    return hkSetDepthStencilSurface(m_real, pNewZStencil);
}

HRESULT __stdcall DeviceProxy::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
{
    return m_real->GetDepthStencilSurface(ppZStencilSurface);
}

HRESULT __stdcall DeviceProxy::BeginScene()
{
    return hkBeginScene(m_real);
}

HRESULT __stdcall DeviceProxy::EndScene()
{
    return hkEndScene(m_real);
}

HRESULT __stdcall DeviceProxy::Clear(DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
    return hkClear(m_real, Count, pRects, Flags, Color, Z, Stencil);
}

HRESULT __stdcall DeviceProxy::SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix)
{
    return hkSetTransform(m_real, State, pMatrix);
}

HRESULT __stdcall DeviceProxy::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix)
{
    return m_real->GetTransform(State, pMatrix);
}

HRESULT __stdcall DeviceProxy::MultiplyTransform(D3DTRANSFORMSTATETYPE arg0, CONST D3DMATRIX* arg1)
{
    return m_real->MultiplyTransform(arg0, arg1);
}

HRESULT __stdcall DeviceProxy::SetViewport(CONST D3DVIEWPORT9* pViewport)
{
    return hkSetViewport(m_real, pViewport);
}

HRESULT __stdcall DeviceProxy::GetViewport(D3DVIEWPORT9* pViewport)
{
    return m_real->GetViewport(pViewport);
}

HRESULT __stdcall DeviceProxy::SetMaterial(CONST D3DMATERIAL9* pMaterial)
{
    return m_real->SetMaterial(pMaterial);
}

HRESULT __stdcall DeviceProxy::GetMaterial(D3DMATERIAL9* pMaterial)
{
    return m_real->GetMaterial(pMaterial);
}

HRESULT __stdcall DeviceProxy::SetLight(DWORD Index, CONST D3DLIGHT9* arg1)
{
    return m_real->SetLight(Index, arg1);
}

HRESULT __stdcall DeviceProxy::GetLight(DWORD Index, D3DLIGHT9* arg1)
{
    return m_real->GetLight(Index, arg1);
}

HRESULT __stdcall DeviceProxy::LightEnable(DWORD Index, BOOL Enable)
{
    return m_real->LightEnable(Index, Enable);
}

HRESULT __stdcall DeviceProxy::GetLightEnable(DWORD Index, BOOL* pEnable)
{
    return m_real->GetLightEnable(Index, pEnable);
}

HRESULT __stdcall DeviceProxy::SetClipPlane(DWORD Index, CONST float* pPlane)
{
    return m_real->SetClipPlane(Index, pPlane);
}

HRESULT __stdcall DeviceProxy::GetClipPlane(DWORD Index, float* pPlane)
{
    return m_real->GetClipPlane(Index, pPlane);
}

HRESULT __stdcall DeviceProxy::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
    return hkSetRenderState(m_real, State, Value);
}

HRESULT __stdcall DeviceProxy::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue)
{
    return m_real->GetRenderState(State, pValue);
}

HRESULT __stdcall DeviceProxy::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB)
{
    return m_real->CreateStateBlock(Type, ppSB);
}

HRESULT __stdcall DeviceProxy::BeginStateBlock()
{
    return m_real->BeginStateBlock();
}

HRESULT __stdcall DeviceProxy::EndStateBlock(IDirect3DStateBlock9** ppSB)
{
    return m_real->EndStateBlock(ppSB);
}

HRESULT __stdcall DeviceProxy::SetClipStatus(CONST D3DCLIPSTATUS9* pClipStatus)
{
    return m_real->SetClipStatus(pClipStatus);
}

HRESULT __stdcall DeviceProxy::GetClipStatus(D3DCLIPSTATUS9* pClipStatus)
{
    return m_real->GetClipStatus(pClipStatus);
}

HRESULT __stdcall DeviceProxy::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture)
{
    return m_real->GetTexture(Stage, ppTexture);
}

HRESULT __stdcall DeviceProxy::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture)
{
    return hkSetTexture(m_real, Stage, pTexture);
}

HRESULT __stdcall DeviceProxy::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue)
{
    return m_real->GetTextureStageState(Stage, Type, pValue);
}

HRESULT __stdcall DeviceProxy::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
    return m_real->SetTextureStageState(Stage, Type, Value);
}

HRESULT __stdcall DeviceProxy::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue)
{
    return m_real->GetSamplerState(Sampler, Type, pValue);
}

HRESULT __stdcall DeviceProxy::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
    return m_real->SetSamplerState(Sampler, Type, Value);
}

HRESULT __stdcall DeviceProxy::ValidateDevice(DWORD* pNumPasses)
{
    return m_real->ValidateDevice(pNumPasses);
}

HRESULT __stdcall DeviceProxy::SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY* pEntries)
{
    return m_real->SetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT __stdcall DeviceProxy::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries)
{
    return m_real->GetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT __stdcall DeviceProxy::SetCurrentTexturePalette(UINT PaletteNumber)
{
    return m_real->SetCurrentTexturePalette(PaletteNumber);
}

HRESULT __stdcall DeviceProxy::GetCurrentTexturePalette(UINT *PaletteNumber)
{
    return m_real->GetCurrentTexturePalette(PaletteNumber);
}

HRESULT __stdcall DeviceProxy::SetScissorRect(CONST RECT* pRect)
{
    return hkSetScissorRect(m_real, pRect);
}

HRESULT __stdcall DeviceProxy::GetScissorRect(RECT* pRect)
{
    return m_real->GetScissorRect(pRect);
}

HRESULT __stdcall DeviceProxy::SetSoftwareVertexProcessing(BOOL bSoftware)
{
    return m_real->SetSoftwareVertexProcessing(bSoftware);
}

HRESULT __stdcall DeviceProxy::SetNPatchMode(float nSegments)
{
    return m_real->SetNPatchMode(nSegments);
}

HRESULT __stdcall DeviceProxy::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
    return hkDrawPrimitive(m_real, PrimitiveType, StartVertex, PrimitiveCount);
}

HRESULT __stdcall DeviceProxy::DrawIndexedPrimitive(D3DPRIMITIVETYPE arg0, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
    return hkDrawIndexedPrimitive(m_real, arg0, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT __stdcall DeviceProxy::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
    return hkDrawPrimitiveUP(m_real, PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT __stdcall DeviceProxy::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
    return hkDrawIndexedPrimitiveUP(m_real, PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT __stdcall DeviceProxy::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags)
{
    return m_real->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
}

HRESULT __stdcall DeviceProxy::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl)
{
    return m_real->CreateVertexDeclaration(pVertexElements, ppDecl);
}

HRESULT __stdcall DeviceProxy::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
{
    return m_real->SetVertexDeclaration(pDecl);
}

HRESULT __stdcall DeviceProxy::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl)
{
    return m_real->GetVertexDeclaration(ppDecl);
}

HRESULT __stdcall DeviceProxy::SetFVF(DWORD FVF)
{
    return m_real->SetFVF(FVF);
}

HRESULT __stdcall DeviceProxy::GetFVF(DWORD* pFVF)
{
    return m_real->GetFVF(pFVF);
}

HRESULT __stdcall DeviceProxy::CreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader)
{
    return m_real->CreateVertexShader(pFunction, ppShader);
}

HRESULT __stdcall DeviceProxy::SetVertexShader(IDirect3DVertexShader9* pShader)
{
    return m_real->SetVertexShader(pShader);
}

HRESULT __stdcall DeviceProxy::GetVertexShader(IDirect3DVertexShader9** ppShader)
{
    return m_real->GetVertexShader(ppShader);
}

HRESULT __stdcall DeviceProxy::SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount)
{
    return hkSetVertexShaderConstantF(m_real, StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall DeviceProxy::GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount)
{
    return m_real->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall DeviceProxy::SetVertexShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount)
{
    return m_real->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall DeviceProxy::GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount)
{
    return m_real->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall DeviceProxy::SetVertexShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount)
{
    return m_real->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall DeviceProxy::GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount)
{
    return m_real->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall DeviceProxy::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride)
{
    return m_real->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
}

HRESULT __stdcall DeviceProxy::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride)
{
    return m_real->GetStreamSource(StreamNumber, ppStreamData, pOffsetInBytes, pStride);
}

HRESULT __stdcall DeviceProxy::SetStreamSourceFreq(UINT StreamNumber, UINT Setting)
{
    return m_real->SetStreamSourceFreq(StreamNumber, Setting);
}

HRESULT __stdcall DeviceProxy::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting)
{
    return m_real->GetStreamSourceFreq(StreamNumber, pSetting);
}

HRESULT __stdcall DeviceProxy::SetIndices(IDirect3DIndexBuffer9* pIndexData)
{
    return m_real->SetIndices(pIndexData);
}

HRESULT __stdcall DeviceProxy::GetIndices(IDirect3DIndexBuffer9** ppIndexData)
{
    return m_real->GetIndices(ppIndexData);
}

HRESULT __stdcall DeviceProxy::CreatePixelShader(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader)
{
    return m_real->CreatePixelShader(pFunction, ppShader);
}

HRESULT __stdcall DeviceProxy::SetPixelShader(IDirect3DPixelShader9* pShader)
{
    return m_real->SetPixelShader(pShader);
}

HRESULT __stdcall DeviceProxy::GetPixelShader(IDirect3DPixelShader9** ppShader)
{
    return m_real->GetPixelShader(ppShader);
}

HRESULT __stdcall DeviceProxy::SetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount)
{
    return m_real->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall DeviceProxy::GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount)
{
    return m_real->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall DeviceProxy::SetPixelShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount)
{
    return m_real->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall DeviceProxy::GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount)
{
    return m_real->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall DeviceProxy::SetPixelShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount)
{
    return m_real->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall DeviceProxy::GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount)
{
    return m_real->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall DeviceProxy::DrawRectPatch(UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* pRectPatchInfo)
{
    return hkDrawRectPatch(m_real, Handle, pNumSegs, pRectPatchInfo);
}

HRESULT __stdcall DeviceProxy::DrawTriPatch(UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo)
{
    return m_real->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}

HRESULT __stdcall DeviceProxy::DeletePatch(UINT Handle)
{
    return m_real->DeletePatch(Handle);
}

HRESULT __stdcall DeviceProxy::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery)
{
    return m_real->CreateQuery(Type, ppQuery);
}

// --- IDirect3DDevice9Ex additions (forward to m_realEx) -----------------

HRESULT __stdcall DeviceProxy::SetConvolutionMonoKernel(UINT width, UINT height, float* rows, float* columns)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->SetConvolutionMonoKernel(width, height, rows, columns);
}

HRESULT __stdcall DeviceProxy::ComposeRects(IDirect3DSurface9* pSrc, IDirect3DSurface9* pDst, IDirect3DVertexBuffer9* pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9* pDstRectDescs, D3DCOMPOSERECTSOP Operation, int Xoffset, int Yoffset)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->ComposeRects(pSrc, pDst, pSrcRectDescs, NumRects, pDstRectDescs, Operation, Xoffset, Yoffset);
}

HRESULT __stdcall DeviceProxy::PresentEx(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

HRESULT __stdcall DeviceProxy::GetGPUThreadPriority(INT* pPriority)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->GetGPUThreadPriority(pPriority);
}

HRESULT __stdcall DeviceProxy::SetGPUThreadPriority(INT Priority)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->SetGPUThreadPriority(Priority);
}

HRESULT __stdcall DeviceProxy::WaitForVBlank(UINT iSwapChain)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->WaitForVBlank(iSwapChain);
}

HRESULT __stdcall DeviceProxy::CheckResourceResidency(IDirect3DResource9** pResourceArray, UINT32 NumResources)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->CheckResourceResidency(pResourceArray, NumResources);
}

HRESULT __stdcall DeviceProxy::SetMaximumFrameLatency(UINT MaxLatency)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->SetMaximumFrameLatency(MaxLatency);
}

HRESULT __stdcall DeviceProxy::GetMaximumFrameLatency(UINT* pMaxLatency)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->GetMaximumFrameLatency(pMaxLatency);
}

HRESULT __stdcall DeviceProxy::CheckDeviceState(HWND hDestinationWindow)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->CheckDeviceState(hDestinationWindow);
}

HRESULT __stdcall DeviceProxy::CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->CreateRenderTargetEx(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle, Usage);
}

HRESULT __stdcall DeviceProxy::CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->CreateOffscreenPlainSurfaceEx(Width, Height, Format, Pool, ppSurface, pSharedHandle, Usage);
}

HRESULT __stdcall DeviceProxy::CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->CreateDepthStencilSurfaceEx(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle, Usage);
}

HRESULT __stdcall DeviceProxy::ResetEx(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->ResetEx(pPresentationParameters, pFullscreenDisplayMode);
}

HRESULT __stdcall DeviceProxy::GetDisplayModeEx(UINT iSwapChain, D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation)
{
    if (!m_realEx) return D3DERR_INVALIDCALL;
    return m_realEx->GetDisplayModeEx(iSwapChain, pMode, pRotation);
}

