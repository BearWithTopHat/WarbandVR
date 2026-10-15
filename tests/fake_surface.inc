// SPDX-License-Identifier: GPL-3.0-only
// GENERATED - FakeSurface trivial methods.
    HRESULT __stdcall GetDevice(IDirect3DDevice9**) override { Fake::Record("GetDevice"); return S_OK; }
    HRESULT __stdcall SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { Fake::Record("SetPrivateData"); return S_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { Fake::Record("GetPrivateData"); return S_OK; }
    HRESULT __stdcall FreePrivateData(REFGUID) override { Fake::Record("FreePrivateData"); return S_OK; }
    DWORD __stdcall SetPriority(DWORD) override { Fake::Record("SetPriority"); return 0; }
    DWORD __stdcall GetPriority() override { Fake::Record("GetPriority"); return 0; }
    void __stdcall PreLoad() override { Fake::Record("PreLoad"); }
    D3DRESOURCETYPE __stdcall GetType() override { Fake::Record("GetType"); return D3DRTYPE_SURFACE; }
    HRESULT __stdcall GetContainer(REFIID, void**) override { Fake::Record("GetContainer"); return S_OK; }
    HRESULT __stdcall GetDC(HDC*) override { Fake::Record("GetDC"); return S_OK; }
    HRESULT __stdcall ReleaseDC(HDC) override { Fake::Record("ReleaseDC"); return S_OK; }