// SPDX-License-Identifier: GPL-3.0-only
// GENERATED - trivial interface fakes.

class Fake9 : public IDirect3D9
{
public:
    HRESULT __stdcall QueryInterface(REFIID, void**) override { Fake::Record("QueryInterface"); return S_OK; }
    ULONG __stdcall AddRef() override { Fake::Record("AddRef"); return 1; }
    ULONG __stdcall Release() override { Fake::Record("Release"); return 1; }
    HRESULT __stdcall RegisterSoftwareDevice(void*) override { Fake::Record("RegisterSoftwareDevice"); return S_OK; }
    UINT __stdcall GetAdapterCount() override { Fake::Record("GetAdapterCount"); return 0; }
    HRESULT __stdcall GetAdapterIdentifier(UINT, DWORD, D3DADAPTER_IDENTIFIER9*) override { Fake::Record("GetAdapterIdentifier"); return S_OK; }
    UINT __stdcall GetAdapterModeCount(UINT, D3DFORMAT) override { Fake::Record("GetAdapterModeCount"); return 0; }
    HRESULT __stdcall EnumAdapterModes(UINT, D3DFORMAT, UINT, D3DDISPLAYMODE*) override { Fake::Record("EnumAdapterModes"); return S_OK; }
    HRESULT __stdcall GetAdapterDisplayMode(UINT, D3DDISPLAYMODE*) override { Fake::Record("GetAdapterDisplayMode"); return S_OK; }
    HRESULT __stdcall CheckDeviceType(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, WINBOOL) override { Fake::Record("CheckDeviceType"); return S_OK; }
    HRESULT __stdcall CheckDeviceFormat(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) override { Fake::Record("CheckDeviceFormat"); return S_OK; }
    HRESULT __stdcall CheckDeviceMultiSampleType(UINT, D3DDEVTYPE, D3DFORMAT, WINBOOL, D3DMULTISAMPLE_TYPE, DWORD*) override { Fake::Record("CheckDeviceMultiSampleType"); return S_OK; }
    HRESULT __stdcall CheckDepthStencilMatch(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) override { Fake::Record("CheckDepthStencilMatch"); return S_OK; }
    HRESULT __stdcall CheckDeviceFormatConversion(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT) override { Fake::Record("CheckDeviceFormatConversion"); return S_OK; }
    HRESULT __stdcall GetDeviceCaps(UINT, D3DDEVTYPE, D3DCAPS9*) override { Fake::Record("GetDeviceCaps"); return S_OK; }
    HMONITOR __stdcall GetAdapterMonitor(UINT) override { Fake::Record("GetAdapterMonitor"); return nullptr; }
    HRESULT __stdcall CreateDevice(UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**) override { Fake::Record("CreateDevice"); return S_OK; }
};

class FakeSwapChain9 : public IDirect3DSwapChain9
{
public:
    HRESULT __stdcall QueryInterface(REFIID, void **) override { Fake::Record("QueryInterface"); return S_OK; }
    ULONG __stdcall AddRef() override { Fake::Record("AddRef"); return 1; }
    ULONG __stdcall Release() override { Fake::Record("Release"); return 1; }
    HRESULT __stdcall Present(const RECT *, const RECT *, HWND, const RGNDATA *, DWORD) override { Fake::Record("Present"); return S_OK; }
    HRESULT __stdcall GetFrontBufferData(IDirect3DSurface9 *) override { Fake::Record("GetFrontBufferData"); return S_OK; }
    HRESULT __stdcall GetBackBuffer(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9 **) override { Fake::Record("GetBackBuffer"); return S_OK; }
    HRESULT __stdcall GetRasterStatus(D3DRASTER_STATUS *) override { Fake::Record("GetRasterStatus"); return S_OK; }
    HRESULT __stdcall GetDisplayMode(D3DDISPLAYMODE *) override { Fake::Record("GetDisplayMode"); return S_OK; }
    HRESULT __stdcall GetDevice(IDirect3DDevice9 **) override { Fake::Record("GetDevice"); return S_OK; }
    HRESULT __stdcall GetPresentParameters(D3DPRESENT_PARAMETERS *) override { Fake::Record("GetPresentParameters"); return S_OK; }
};

class FakeVolume9 : public IDirect3DVolume9
{
public:
    HRESULT __stdcall QueryInterface(REFIID, void**) override { Fake::Record("QueryInterface"); return S_OK; }
    ULONG __stdcall AddRef() override { Fake::Record("AddRef"); return 1; }
    ULONG __stdcall Release() override { Fake::Record("Release"); return 1; }
    HRESULT __stdcall GetDevice(IDirect3DDevice9**) override { Fake::Record("GetDevice"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { Fake::Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { Fake::Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID) override { Fake::Record("FreePrivateData"); return S_OK; }
    HRESULT __stdcall GetContainer(REFIID, void**) override { Fake::Record("GetContainer"); return S_OK; }
    HRESULT __stdcall GetDesc(D3DVOLUME_DESC*) override { Fake::Record("GetDesc"); return S_OK; }
    HRESULT __stdcall LockBox(D3DLOCKED_BOX *, const D3DBOX *, DWORD) override { Fake::Record("LockBox"); return S_OK; }
    HRESULT __stdcall UnlockBox() override { Fake::Record("UnlockBox"); return S_OK; }
};

class FakeVertexBuffer9 : public IDirect3DVertexBuffer9
{
public:
    HRESULT __stdcall QueryInterface(REFIID, void**) override { Fake::Record("QueryInterface"); return S_OK; }
    ULONG __stdcall AddRef() override { Fake::Record("AddRef"); return 1; }
    ULONG __stdcall Release() override { Fake::Record("Release"); return 1; }
    HRESULT __stdcall GetDevice(IDirect3DDevice9**) override { Fake::Record("GetDevice"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { Fake::Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { Fake::Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID) override { Fake::Record("FreePrivateData"); return S_OK; }
    DWORD __stdcall SetPriority(DWORD) override { Fake::Record("SetPriority"); return 0; }
    DWORD __stdcall GetPriority() override { Fake::Record("GetPriority"); return 0; }
    void __stdcall PreLoad() override { Fake::Record("PreLoad"); }
    D3DRESOURCETYPE __stdcall GetType() override { Fake::Record("GetType"); return D3DRTYPE_SURFACE; }
    HRESULT __stdcall Lock(UINT, UINT, void**, DWORD) override { Fake::Record("Lock"); return S_OK; }
    HRESULT __stdcall Unlock() override { Fake::Record("Unlock"); return S_OK; }
    HRESULT __stdcall GetDesc(D3DVERTEXBUFFER_DESC*) override { Fake::Record("GetDesc"); return S_OK; }
};

class FakeIndexBuffer9 : public IDirect3DIndexBuffer9
{
public:
    HRESULT __stdcall QueryInterface(REFIID, void**) override { Fake::Record("QueryInterface"); return S_OK; }
    ULONG __stdcall AddRef() override { Fake::Record("AddRef"); return 1; }
    ULONG __stdcall Release() override { Fake::Record("Release"); return 1; }
    HRESULT __stdcall GetDevice(IDirect3DDevice9**) override { Fake::Record("GetDevice"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { Fake::Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { Fake::Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID) override { Fake::Record("FreePrivateData"); return S_OK; }
    DWORD __stdcall SetPriority(DWORD) override { Fake::Record("SetPriority"); return 0; }
    DWORD __stdcall GetPriority() override { Fake::Record("GetPriority"); return 0; }
    void __stdcall PreLoad() override { Fake::Record("PreLoad"); }
    D3DRESOURCETYPE __stdcall GetType() override { Fake::Record("GetType"); return D3DRTYPE_SURFACE; }
    HRESULT __stdcall Lock(UINT, UINT, void**, DWORD) override { Fake::Record("Lock"); return S_OK; }
    HRESULT __stdcall Unlock() override { Fake::Record("Unlock"); return S_OK; }
    HRESULT __stdcall GetDesc(D3DINDEXBUFFER_DESC*) override { Fake::Record("GetDesc"); return S_OK; }
};

class FakeVertexShader9 : public IDirect3DVertexShader9
{
public:
    HRESULT __stdcall QueryInterface(REFIID, void**) override { Fake::Record("QueryInterface"); return S_OK; }
    ULONG __stdcall AddRef() override { Fake::Record("AddRef"); return 1; }
    ULONG __stdcall Release() override { Fake::Record("Release"); return 1; }
    HRESULT __stdcall GetDevice(IDirect3DDevice9**) override { Fake::Record("GetDevice"); return S_OK; }
    HRESULT __stdcall GetFunction(void*, UINT*) override { Fake::Record("GetFunction"); return S_OK; }
};

class FakePixelShader9 : public IDirect3DPixelShader9
{
public:
    HRESULT __stdcall QueryInterface(REFIID, void**) override { Fake::Record("QueryInterface"); return S_OK; }
    ULONG __stdcall AddRef() override { Fake::Record("AddRef"); return 1; }
    ULONG __stdcall Release() override { Fake::Record("Release"); return 1; }
    HRESULT __stdcall GetDevice(IDirect3DDevice9**) override { Fake::Record("GetDevice"); return S_OK; }
    HRESULT __stdcall GetFunction(void*, UINT*) override { Fake::Record("GetFunction"); return S_OK; }
};

class FakeVertexDeclaration9 : public IDirect3DVertexDeclaration9
{
public:
    HRESULT __stdcall QueryInterface(REFIID, void**) override { Fake::Record("QueryInterface"); return S_OK; }
    ULONG __stdcall AddRef() override { Fake::Record("AddRef"); return 1; }
    ULONG __stdcall Release() override { Fake::Record("Release"); return 1; }
    HRESULT __stdcall GetDevice(IDirect3DDevice9**) override { Fake::Record("GetDevice"); return S_OK; }
    HRESULT __stdcall GetDeclaration(D3DVERTEXELEMENT9*, UINT*) override { Fake::Record("GetDeclaration"); return S_OK; }
};

class FakeStateBlock9 : public IDirect3DStateBlock9
{
public:
    HRESULT __stdcall QueryInterface(REFIID, void**) override { Fake::Record("QueryInterface"); return S_OK; }
    ULONG __stdcall AddRef() override { Fake::Record("AddRef"); return 1; }
    ULONG __stdcall Release() override { Fake::Record("Release"); return 1; }
    HRESULT __stdcall GetDevice(IDirect3DDevice9**) override { Fake::Record("GetDevice"); return S_OK; }
    HRESULT __stdcall Capture() override { Fake::Record("Capture"); return S_OK; }
    HRESULT __stdcall Apply() override { Fake::Record("Apply"); return S_OK; }
};

class FakeQuery9 : public IDirect3DQuery9
{
public:
    HRESULT __stdcall QueryInterface(REFIID, void**) override { Fake::Record("QueryInterface"); return S_OK; }
    ULONG __stdcall AddRef() override { Fake::Record("AddRef"); return 1; }
    ULONG __stdcall Release() override { Fake::Record("Release"); return 1; }
    HRESULT __stdcall GetDevice(IDirect3DDevice9**) override { Fake::Record("GetDevice"); return S_OK; }
    D3DQUERYTYPE __stdcall GetType() override { Fake::Record("GetType"); return 0; }
    DWORD __stdcall GetDataSize() override { Fake::Record("GetDataSize"); return 0; }
    HRESULT __stdcall Issue(DWORD) override { Fake::Record("Issue"); return S_OK; }
    HRESULT __stdcall GetData(void*, DWORD, DWORD) override { Fake::Record("GetData"); return S_OK; }
};
