// SPDX-License-Identifier: GPL-3.0-only
// GENERATED - FakeDevice trivial methods.
    HRESULT __stdcall TestCooperativeLevel() override { Fake::Record("TestCooperativeLevel"); return S_OK; }
    UINT __stdcall GetAvailableTextureMem() override { Fake::Record("GetAvailableTextureMem"); return 0; }
    HRESULT __stdcall EvictManagedResources() override { Fake::Record("EvictManagedResources"); return S_OK; }
    HRESULT __stdcall GetDisplayMode(UINT, D3DDISPLAYMODE*) override { Fake::Record("GetDisplayMode"); return S_OK; }
    HRESULT __stdcall GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *) override { Fake::Record("GetCreationParameters"); return S_OK; }
    HRESULT __stdcall SetCursorProperties(UINT, UINT, IDirect3DSurface9*) override { Fake::Record("SetCursorProperties"); return S_OK; }
    void __stdcall SetCursorPosition(int, int, DWORD) override { Fake::Record("SetCursorPosition"); }
    WINBOOL __stdcall ShowCursor(WINBOOL) override { Fake::Record("ShowCursor"); return FALSE; }
    HRESULT __stdcall CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS*, IDirect3DSwapChain9**) override { Fake::Record("CreateAdditionalSwapChain"); return S_OK; }
    HRESULT __stdcall GetSwapChain(UINT, IDirect3DSwapChain9**) override { Fake::Record("GetSwapChain"); return S_OK; }
    UINT __stdcall GetNumberOfSwapChains() override { Fake::Record("GetNumberOfSwapChains"); return 0; }
    HRESULT __stdcall GetRasterStatus(UINT, D3DRASTER_STATUS*) override { Fake::Record("GetRasterStatus"); return S_OK; }
    HRESULT __stdcall SetDialogBoxMode(WINBOOL) override { Fake::Record("SetDialogBoxMode"); return S_OK; }
    void __stdcall SetGammaRamp(UINT, DWORD, const D3DGAMMARAMP *) override { Fake::Record("SetGammaRamp"); }
    void __stdcall GetGammaRamp(UINT, D3DGAMMARAMP*) override { Fake::Record("GetGammaRamp"); }
    HRESULT __stdcall CreateVolumeTexture(UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9**, HANDLE*) override { Fake::Record("CreateVolumeTexture"); return S_OK; }
    HRESULT __stdcall CreateCubeTexture(UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9**, HANDLE*) override { Fake::Record("CreateCubeTexture"); return S_OK; }
    HRESULT __stdcall CreateVertexBuffer(UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE*) override { Fake::Record("CreateVertexBuffer"); return S_OK; }
    HRESULT __stdcall CreateIndexBuffer(UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9**, HANDLE*) override { Fake::Record("CreateIndexBuffer"); return S_OK; }
    HRESULT __stdcall UpdateSurface(IDirect3DSurface9 *, const RECT *, IDirect3DSurface9 *, const POINT *) override { Fake::Record("UpdateSurface"); return S_OK; }
    HRESULT __stdcall UpdateTexture(IDirect3DBaseTexture9*, IDirect3DBaseTexture9*) override { Fake::Record("UpdateTexture"); return S_OK; }
    HRESULT __stdcall GetFrontBufferData(UINT, IDirect3DSurface9*) override { Fake::Record("GetFrontBufferData"); return S_OK; }
    HRESULT __stdcall ColorFill(IDirect3DSurface9 *, const RECT *, D3DCOLOR) override { Fake::Record("ColorFill"); return S_OK; }
    HRESULT __stdcall MultiplyTransform(D3DTRANSFORMSTATETYPE, const D3DMATRIX *) override { Fake::Record("MultiplyTransform"); return S_OK; }
    HRESULT __stdcall SetMaterial(const D3DMATERIAL9 *) override { Fake::Record("SetMaterial"); return S_OK; }
    HRESULT __stdcall GetMaterial(D3DMATERIAL9*) override { Fake::Record("GetMaterial"); return S_OK; }
    HRESULT __stdcall SetLight(DWORD, const D3DLIGHT9 *) override { Fake::Record("SetLight"); return S_OK; }
    HRESULT __stdcall GetLight(DWORD, D3DLIGHT9*) override { Fake::Record("GetLight"); return S_OK; }
    HRESULT __stdcall LightEnable(DWORD, WINBOOL) override { Fake::Record("LightEnable"); return S_OK; }
    HRESULT __stdcall GetLightEnable(DWORD, WINBOOL*) override { Fake::Record("GetLightEnable"); return S_OK; }
    HRESULT __stdcall SetClipPlane(DWORD, const float *) override { Fake::Record("SetClipPlane"); return S_OK; }
    HRESULT __stdcall GetClipPlane(DWORD, float*) override { Fake::Record("GetClipPlane"); return S_OK; }
    HRESULT __stdcall GetRenderState(D3DRENDERSTATETYPE state, DWORD* value) override { if (!value) return E_POINTER; *value = renderStates[(UINT)state & 0xFF]; return S_OK; }
    HRESULT __stdcall CreateStateBlock(D3DSTATEBLOCKTYPE, IDirect3DStateBlock9**) override { Fake::Record("CreateStateBlock"); return S_OK; }
    HRESULT __stdcall BeginStateBlock() override { Fake::Record("BeginStateBlock"); return S_OK; }
    HRESULT __stdcall EndStateBlock(IDirect3DStateBlock9**) override { Fake::Record("EndStateBlock"); return S_OK; }
    HRESULT __stdcall SetClipStatus(const D3DCLIPSTATUS9 *) override { Fake::Record("SetClipStatus"); return S_OK; }
    HRESULT __stdcall GetClipStatus(D3DCLIPSTATUS9*) override { Fake::Record("GetClipStatus"); return S_OK; }
    HRESULT __stdcall GetTexture(DWORD stage, IDirect3DBaseTexture9** tex) override { if (!tex) return E_POINTER; *tex = stage < 16 ? textures[stage] : nullptr; if (*tex) (*tex)->AddRef(); return S_OK; }
    HRESULT __stdcall GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD* value) override { if (!value) return E_POINTER; *value = stage < 16 ? stageStates[stage][(UINT)type & 0xF] : 0; return S_OK; }
    HRESULT __stdcall SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override { if (stage < 16) stageStates[stage][(UINT)type & 0xF] = value; Fake::Recordf("SetTSS %u,%u=%u", stage, (UINT)type, value); return S_OK; }
    HRESULT __stdcall GetSamplerState(DWORD stage, D3DSAMPLERSTATETYPE type, DWORD* value) override { if (!value) return E_POINTER; *value = stage < 16 ? samplerStates[stage][(UINT)type & 0xF] : 0; return S_OK; }
    HRESULT __stdcall SetSamplerState(DWORD stage, D3DSAMPLERSTATETYPE type, DWORD value) override { if (stage < 16) samplerStates[stage][(UINT)type & 0xF] = value; Fake::Recordf("SetSampler %u,%u=%u", stage, (UINT)type, value); return S_OK; }
    HRESULT __stdcall ValidateDevice(DWORD*) override { Fake::Record("ValidateDevice"); return S_OK; }
    HRESULT __stdcall SetPaletteEntries(UINT, const PALETTEENTRY *) override { Fake::Record("SetPaletteEntries"); return S_OK; }
    HRESULT __stdcall GetPaletteEntries(UINT, PALETTEENTRY*) override { Fake::Record("GetPaletteEntries"); return S_OK; }
    HRESULT __stdcall SetCurrentTexturePalette(UINT) override { Fake::Record("SetCurrentTexturePalette"); return S_OK; }
    HRESULT __stdcall GetCurrentTexturePalette(UINT *) override { Fake::Record("GetCurrentTexturePalette"); return S_OK; }
    HRESULT __stdcall GetScissorRect(RECT*) override { Fake::Record("GetScissorRect"); return S_OK; }
    HRESULT __stdcall SetSoftwareVertexProcessing(WINBOOL) override { Fake::Record("SetSoftwareVertexProcessing"); return S_OK; }
    WINBOOL __stdcall GetSoftwareVertexProcessing() override { Fake::Record("GetSoftwareVertexProcessing"); return FALSE; }
    HRESULT __stdcall SetNPatchMode(float) override { Fake::Record("SetNPatchMode"); return S_OK; }
    float __stdcall GetNPatchMode() override { Fake::Record("GetNPatchMode"); return 0.0f; }
    HRESULT __stdcall ProcessVertices(UINT, UINT, UINT, IDirect3DVertexBuffer9*, IDirect3DVertexDeclaration9*, DWORD) override { Fake::Record("ProcessVertices"); return S_OK; }
    HRESULT __stdcall CreateVertexDeclaration(const D3DVERTEXELEMENT9 *, IDirect3DVertexDeclaration9 **) override { Fake::Record("CreateVertexDeclaration"); return S_OK; }
    HRESULT __stdcall GetVertexDeclaration(IDirect3DVertexDeclaration9**) override { Fake::Record("GetVertexDeclaration"); return S_OK; }
    HRESULT __stdcall GetFVF(DWORD* value) override { if (!value) return E_POINTER; *value = fvf; return S_OK; }
    HRESULT __stdcall CreateVertexShader(const DWORD *, IDirect3DVertexShader9 **) override { Fake::Record("CreateVertexShader"); return S_OK; }
    HRESULT __stdcall SetVertexShader(IDirect3DVertexShader9* shader) override { vertexShader = shader; Fake::Recordf("SetVertexShader %p", (void*)shader); return S_OK; }
    HRESULT __stdcall GetVertexShader(IDirect3DVertexShader9** shader) override { if (!shader) return E_POINTER; *shader = vertexShader; if (*shader) (*shader)->AddRef(); return S_OK; }
    HRESULT __stdcall SetVertexShaderConstantI(UINT, const int *, UINT) override { Fake::Record("SetVertexShaderConstantI"); return S_OK; }
    HRESULT __stdcall GetVertexShaderConstantI(UINT, int*, UINT) override { Fake::Record("GetVertexShaderConstantI"); return S_OK; }
    HRESULT __stdcall SetVertexShaderConstantB(UINT, const WINBOOL *, UINT) override { Fake::Record("SetVertexShaderConstantB"); return S_OK; }
    HRESULT __stdcall GetVertexShaderConstantB(UINT, WINBOOL*, UINT) override { Fake::Record("GetVertexShaderConstantB"); return S_OK; }
    HRESULT __stdcall GetStreamSource(UINT, IDirect3DVertexBuffer9**, UINT*, UINT*) override { Fake::Record("GetStreamSource"); return S_OK; }
    HRESULT __stdcall SetStreamSourceFreq(UINT, UINT) override { Fake::Record("SetStreamSourceFreq"); return S_OK; }
    HRESULT __stdcall GetStreamSourceFreq(UINT, UINT*) override { Fake::Record("GetStreamSourceFreq"); return S_OK; }
    HRESULT __stdcall GetIndices(IDirect3DIndexBuffer9**) override { Fake::Record("GetIndices"); return S_OK; }
    HRESULT __stdcall CreatePixelShader(const DWORD *, IDirect3DPixelShader9 **) override { Fake::Record("CreatePixelShader"); return S_OK; }
    HRESULT __stdcall SetPixelShader(IDirect3DPixelShader9* shader) override { pixelShader = shader; Fake::Recordf("SetPixelShader %p", (void*)shader); return S_OK; }
    HRESULT __stdcall GetPixelShader(IDirect3DPixelShader9** shader) override { if (!shader) return E_POINTER; *shader = pixelShader; if (*shader) (*shader)->AddRef(); return S_OK; }
    HRESULT __stdcall SetPixelShaderConstantF(UINT, const float *, UINT) override { Fake::Record("SetPixelShaderConstantF"); return S_OK; }
    HRESULT __stdcall GetPixelShaderConstantF(UINT, float*, UINT) override { Fake::Record("GetPixelShaderConstantF"); return S_OK; }
    HRESULT __stdcall SetPixelShaderConstantI(UINT, const int *, UINT) override { Fake::Record("SetPixelShaderConstantI"); return S_OK; }
    HRESULT __stdcall GetPixelShaderConstantI(UINT, int*, UINT) override { Fake::Record("GetPixelShaderConstantI"); return S_OK; }
    HRESULT __stdcall SetPixelShaderConstantB(UINT, const WINBOOL *, UINT) override { Fake::Record("SetPixelShaderConstantB"); return S_OK; }
    HRESULT __stdcall GetPixelShaderConstantB(UINT, WINBOOL*, UINT) override { Fake::Record("GetPixelShaderConstantB"); return S_OK; }
    HRESULT __stdcall DeletePatch(UINT) override { Fake::Record("DeletePatch"); return S_OK; }
    HRESULT __stdcall CreateQuery(D3DQUERYTYPE, IDirect3DQuery9**) override { Fake::Record("CreateQuery"); return S_OK; }