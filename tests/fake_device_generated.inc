// SPDX-License-Identifier: GPL-3.0-only
// GENERATED trivial stubs - do not edit by hand.
    HRESULT __stdcall RegisterSoftwareDevice(void* pInitializeFunction p0) override { Record("RegisterSoftwareDevice"); return S_OK; }
    HRESULT __stdcall GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier p2) override { Record("GetAdapterIdentifier"); return S_OK; }
    HRESULT __stdcall EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode p3) override { Record("EnumAdapterModes"); return S_OK; }
    HRESULT __stdcall GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode p1) override { Record("GetAdapterDisplayMode"); return S_OK; }
    HRESULT __stdcall CheckDeviceType(UINT iAdapter, D3DDEVTYPE DevType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, WINBOOL bWindowed) override { Record("CheckDeviceType"); return S_OK; }
    HRESULT __stdcall CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override { Record("CheckDeviceFormat"); return S_OK; }
    HRESULT __stdcall CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, WINBOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels p5) override { Record("CheckDeviceMultiSampleType"); return S_OK; }
    HRESULT __stdcall CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) override { Record("CheckDepthStencilMatch"); return S_OK; }
    HRESULT __stdcall CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) override { Record("CheckDeviceFormatConversion"); return S_OK; }
    HRESULT __stdcall CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters p4, IDirect3DDevice9** ppReturnedDeviceInterface p5) override { Record("CreateDevice"); return S_OK; }
    HRESULT __stdcall TestCooperativeLevel() override { Record("TestCooperativeLevel"); return S_OK; }
    HRESULT __stdcall EvictManagedResources() override { Record("EvictManagedResources"); return S_OK; }
    HRESULT __stdcall GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode p1) override { Record("GetDisplayMode"); return S_OK; }
    HRESULT __stdcall GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters p0) override { Record("GetCreationParameters"); return S_OK; }
    HRESULT __stdcall SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap p2) override { Record("SetCursorProperties"); return S_OK; }
    HRESULT __stdcall CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters p0, IDirect3DSwapChain9** pSwapChain p1) override { Record("CreateAdditionalSwapChain"); return S_OK; }
    HRESULT __stdcall GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain p1) override { Record("GetSwapChain"); return S_OK; }
    HRESULT __stdcall GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus p1) override { Record("GetRasterStatus"); return S_OK; }
    HRESULT __stdcall SetDialogBoxMode(WINBOOL bEnableDialogs) override { Record("SetDialogBoxMode"); return S_OK; }
    HRESULT __stdcall CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture p7, HANDLE* pSharedHandle p8) override { Record("CreateVolumeTexture"); return S_OK; }
    HRESULT __stdcall CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture p5, HANDLE* pSharedHandle p6) override { Record("CreateCubeTexture"); return S_OK; }
    HRESULT __stdcall CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer p4, HANDLE* pSharedHandle p5) override { Record("CreateVertexBuffer"); return S_OK; }
    HRESULT __stdcall CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer p4, HANDLE* pSharedHandle p5) override { Record("CreateIndexBuffer"); return S_OK; }
    HRESULT __stdcall UpdateTexture(IDirect3DBaseTexture9* pSourceTexture p0, IDirect3DBaseTexture9* pDestinationTexture p1) override { Record("UpdateTexture"); return S_OK; }
    HRESULT __stdcall GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface p1) override { Record("GetFrontBufferData"); return S_OK; }
    HRESULT __stdcall ColorFill(IDirect3DSurface9 *surface p0, const RECT *rect p1, D3DCOLOR color) override { Record("ColorFill"); return S_OK; }
    HRESULT __stdcall MultiplyTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX *matrix p1) override { Record("MultiplyTransform"); return S_OK; }
    HRESULT __stdcall SetMaterial(const D3DMATERIAL9 *material p0) override { Record("SetMaterial"); return S_OK; }
    HRESULT __stdcall GetMaterial(D3DMATERIAL9* pMaterial p0) override { Record("GetMaterial"); return S_OK; }
    HRESULT __stdcall SetLight(DWORD index, const D3DLIGHT9 *light p1) override { Record("SetLight"); return S_OK; }
    HRESULT __stdcall GetLight(DWORD Index, D3DLIGHT9* p1) override { Record("GetLight"); return S_OK; }
    HRESULT __stdcall LightEnable(DWORD Index, WINBOOL Enable) override { Record("LightEnable"); return S_OK; }
    HRESULT __stdcall GetLightEnable(DWORD Index, WINBOOL* pEnable p1) override { Record("GetLightEnable"); return S_OK; }
    HRESULT __stdcall SetClipPlane(DWORD index, const float *plane p1) override { Record("SetClipPlane"); return S_OK; }
    HRESULT __stdcall GetClipPlane(DWORD Index, float* pPlane p1) override { Record("GetClipPlane"); return S_OK; }
    HRESULT __stdcall GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue p1) override { Record("GetRenderState"); return S_OK; }
    HRESULT __stdcall CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB p1) override { Record("CreateStateBlock"); return S_OK; }
    HRESULT __stdcall BeginStateBlock() override { Record("BeginStateBlock"); return S_OK; }
    HRESULT __stdcall EndStateBlock(IDirect3DStateBlock9** ppSB p0) override { Record("EndStateBlock"); return S_OK; }
    HRESULT __stdcall SetClipStatus(const D3DCLIPSTATUS9 *clip_status p0) override { Record("SetClipStatus"); return S_OK; }
    HRESULT __stdcall GetClipStatus(D3DCLIPSTATUS9* pClipStatus p0) override { Record("GetClipStatus"); return S_OK; }
    HRESULT __stdcall GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture p1) override { Record("GetTexture"); return S_OK; }
    HRESULT __stdcall GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue p2) override { Record("GetTextureStageState"); return S_OK; }
    HRESULT __stdcall SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override { Record("SetTextureStageState"); return S_OK; }
    HRESULT __stdcall GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue p2) override { Record("GetSamplerState"); return S_OK; }
    HRESULT __stdcall SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override { Record("SetSamplerState"); return S_OK; }
    HRESULT __stdcall ValidateDevice(DWORD* pNumPasses p0) override { Record("ValidateDevice"); return S_OK; }
    HRESULT __stdcall SetPaletteEntries(UINT palette_idx, const PALETTEENTRY *entries p1) override { Record("SetPaletteEntries"); return S_OK; }
    HRESULT __stdcall GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries p1) override { Record("GetPaletteEntries"); return S_OK; }
    HRESULT __stdcall SetCurrentTexturePalette(UINT PaletteNumber) override { Record("SetCurrentTexturePalette"); return S_OK; }
    HRESULT __stdcall GetCurrentTexturePalette(UINT *PaletteNumber p0) override { Record("GetCurrentTexturePalette"); return S_OK; }
    HRESULT __stdcall GetScissorRect(RECT* pRect p0) override { Record("GetScissorRect"); return S_OK; }
    HRESULT __stdcall SetSoftwareVertexProcessing(WINBOOL bSoftware) override { Record("SetSoftwareVertexProcessing"); return S_OK; }
    HRESULT __stdcall SetNPatchMode(float nSegments) override { Record("SetNPatchMode"); return S_OK; }
    HRESULT __stdcall ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer p3, IDirect3DVertexDeclaration9* pVertexDecl p4, DWORD Flags) override { Record("ProcessVertices"); return S_OK; }
    HRESULT __stdcall GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl p0) override { Record("GetVertexDeclaration"); return S_OK; }
    HRESULT __stdcall GetFVF(DWORD* pFVF p0) override { Record("GetFVF"); return S_OK; }
    HRESULT __stdcall CreateVertexShader(const DWORD *byte_code p0, IDirect3DVertexShader9 **shader p1) override { Record("CreateVertexShader"); return S_OK; }
    HRESULT __stdcall SetVertexShader(IDirect3DVertexShader9* pShader p0) override { Record("SetVertexShader"); return S_OK; }
    HRESULT __stdcall GetVertexShader(IDirect3DVertexShader9** ppShader p0) override { Record("GetVertexShader"); return S_OK; }
    HRESULT __stdcall SetVertexShaderConstantI(UINT reg_idx, const int *data p1, UINT count) override { Record("SetVertexShaderConstantI"); return S_OK; }
    HRESULT __stdcall GetVertexShaderConstantI(UINT StartRegister, int* pConstantData p1, UINT Vector4iCount) override { Record("GetVertexShaderConstantI"); return S_OK; }
    HRESULT __stdcall SetVertexShaderConstantB(UINT reg_idx, const WINBOOL *data p1, UINT count) override { Record("SetVertexShaderConstantB"); return S_OK; }
    HRESULT __stdcall GetVertexShaderConstantB(UINT StartRegister, WINBOOL* pConstantData p1, UINT BoolCount) override { Record("GetVertexShaderConstantB"); return S_OK; }
    HRESULT __stdcall GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData p1, UINT* OffsetInBytes p2, UINT* pStride p3) override { Record("GetStreamSource"); return S_OK; }
    HRESULT __stdcall SetStreamSourceFreq(UINT StreamNumber, UINT Divider) override { Record("SetStreamSourceFreq"); return S_OK; }
    HRESULT __stdcall GetStreamSourceFreq(UINT StreamNumber, UINT* Divider p1) override { Record("GetStreamSourceFreq"); return S_OK; }
    HRESULT __stdcall GetIndices(IDirect3DIndexBuffer9** ppIndexData p0) override { Record("GetIndices"); return S_OK; }
    HRESULT __stdcall CreatePixelShader(const DWORD *byte_code p0, IDirect3DPixelShader9 **shader p1) override { Record("CreatePixelShader"); return S_OK; }
    HRESULT __stdcall SetPixelShader(IDirect3DPixelShader9* pShader p0) override { Record("SetPixelShader"); return S_OK; }
    HRESULT __stdcall GetPixelShader(IDirect3DPixelShader9** ppShader p0) override { Record("GetPixelShader"); return S_OK; }
    HRESULT __stdcall SetPixelShaderConstantF(UINT reg_idx, const float *data p1, UINT count) override { Record("SetPixelShaderConstantF"); return S_OK; }
    HRESULT __stdcall GetPixelShaderConstantF(UINT StartRegister, float* pConstantData p1, UINT Vector4fCount) override { Record("GetPixelShaderConstantF"); return S_OK; }
    HRESULT __stdcall SetPixelShaderConstantI(UINT reg_idx, const int *data p1, UINT count) override { Record("SetPixelShaderConstantI"); return S_OK; }
    HRESULT __stdcall GetPixelShaderConstantI(UINT StartRegister, int* pConstantData p1, UINT Vector4iCount) override { Record("GetPixelShaderConstantI"); return S_OK; }
    HRESULT __stdcall SetPixelShaderConstantB(UINT reg_idx, const WINBOOL *data p1, UINT count) override { Record("SetPixelShaderConstantB"); return S_OK; }
    HRESULT __stdcall GetPixelShaderConstantB(UINT StartRegister, WINBOOL* pConstantData p1, UINT BoolCount) override { Record("GetPixelShaderConstantB"); return S_OK; }
    HRESULT __stdcall DeletePatch(UINT Handle) override { Record("DeletePatch"); return S_OK; }
    HRESULT __stdcall CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery p1) override { Record("CreateQuery"); return S_OK; }
    HRESULT __stdcall GetFrontBufferData(IDirect3DSurface9 *pDestSurface p0) override { Record("GetFrontBufferData"); return S_OK; }
    HRESULT __stdcall GetRasterStatus(D3DRASTER_STATUS *pRasterStatus p0) override { Record("GetRasterStatus"); return S_OK; }
    HRESULT __stdcall GetDisplayMode(D3DDISPLAYMODE *pMode p0) override { Record("GetDisplayMode"); return S_OK; }
    HRESULT __stdcall GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters p0) override { Record("GetPresentParameters"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data p1, DWORD data_size, DWORD flags) override { Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData p1, DWORD* pSizeOfData p2) override { Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID refguid) override { Record("FreePrivateData"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data p1, DWORD data_size, DWORD flags) override { Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData p1, DWORD* pSizeOfData p2) override { Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID refguid) override { Record("FreePrivateData"); return S_OK; }
    HRESULT __stdcall SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override { Record("SetAutoGenFilterType"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data p1, DWORD data_size, DWORD flags) override { Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData p1, DWORD* pSizeOfData p2) override { Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID refguid) override { Record("FreePrivateData"); return S_OK; }
    HRESULT __stdcall SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override { Record("SetAutoGenFilterType"); return S_OK; }
    HRESULT __stdcall GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc p1) override { Record("GetLevelDesc"); return S_OK; }
    HRESULT __stdcall GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel p1) override { Record("GetSurfaceLevel"); return S_OK; }
    HRESULT __stdcall LockRect(UINT level, D3DLOCKED_RECT *locked_rect p1, const RECT *rect p2, DWORD flags) override { Record("LockRect"); return S_OK; }
    HRESULT __stdcall UnlockRect(UINT Level) override { Record("UnlockRect"); return S_OK; }
    HRESULT __stdcall AddDirtyRect(const RECT *dirty_rect p0) override { Record("AddDirtyRect"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data p1, DWORD data_size, DWORD flags) override { Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData p1, DWORD* pSizeOfData p2) override { Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID refguid) override { Record("FreePrivateData"); return S_OK; }
    HRESULT __stdcall GetContainer(REFIID riid, void** ppContainer p1) override { Record("GetContainer"); return S_OK; }
    HRESULT __stdcall GetDesc(D3DSURFACE_DESC* pDesc p0) override { Record("GetDesc"); return S_OK; }
    HRESULT __stdcall LockRect(D3DLOCKED_RECT *locked_rect p0, const RECT *rect p1, DWORD flags) override { Record("LockRect"); return S_OK; }
    HRESULT __stdcall UnlockRect() override { Record("UnlockRect"); return S_OK; }
    HRESULT __stdcall GetDC(HDC* phdc p0) override { Record("GetDC"); return S_OK; }
    HRESULT __stdcall ReleaseDC(HDC hdc) override { Record("ReleaseDC"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data p1, DWORD data_size, DWORD flags) override { Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData p1, DWORD* pSizeOfData p2) override { Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID refguid) override { Record("FreePrivateData"); return S_OK; }
    HRESULT __stdcall GetContainer(REFIID riid, void** ppContainer p1) override { Record("GetContainer"); return S_OK; }
    HRESULT __stdcall GetDesc(D3DVOLUME_DESC* pDesc p0) override { Record("GetDesc"); return S_OK; }
    HRESULT __stdcall LockBox(D3DLOCKED_BOX *locked_box p0, const D3DBOX *box p1, DWORD flags) override { Record("LockBox"); return S_OK; }
    HRESULT __stdcall UnlockBox() override { Record("UnlockBox"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data p1, DWORD data_size, DWORD flags) override { Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData p1, DWORD* pSizeOfData p2) override { Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID refguid) override { Record("FreePrivateData"); return S_OK; }
    HRESULT __stdcall Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData p2, DWORD Flags) override { Record("Lock"); return S_OK; }
    HRESULT __stdcall Unlock() override { Record("Unlock"); return S_OK; }
    HRESULT __stdcall GetDesc(D3DVERTEXBUFFER_DESC* pDesc p0) override { Record("GetDesc"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID guid, const void *data p1, DWORD data_size, DWORD flags) override { Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID refguid, void* pData p1, DWORD* pSizeOfData p2) override { Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID refguid) override { Record("FreePrivateData"); return S_OK; }
    HRESULT __stdcall Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData p2, DWORD Flags) override { Record("Lock"); return S_OK; }
    HRESULT __stdcall Unlock() override { Record("Unlock"); return S_OK; }
    HRESULT __stdcall GetDesc(D3DINDEXBUFFER_DESC* pDesc p0) override { Record("GetDesc"); return S_OK; }
    HRESULT __stdcall GetFunction(void* p0, UINT* pSizeOfData p1) override { Record("GetFunction"); return S_OK; }
    HRESULT __stdcall GetFunction(void* p0, UINT* pSizeOfData p1) override { Record("GetFunction"); return S_OK; }
    HRESULT __stdcall GetDeclaration(D3DVERTEXELEMENT9* p0, UINT* pNumElements p1) override { Record("GetDeclaration"); return S_OK; }
    HRESULT __stdcall Capture() override { Record("Capture"); return S_OK; }
    HRESULT __stdcall Apply() override { Record("Apply"); return S_OK; }
    HRESULT __stdcall Issue(DWORD dwIssueFlags) override { Record("Issue"); return S_OK; }
    HRESULT __stdcall GetData(void* pData p0, DWORD dwSize, DWORD dwGetDataFlags) override { Record("GetData"); return S_OK; }
