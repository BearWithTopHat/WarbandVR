#include "D3D9Proxy.h"
#include "DeviceHooks.h"
#include "Logger.h"

D3D9Proxy::D3D9Proxy(IDirect3D9* real)
    : m_real(real), m_refCount(1)
{
}

HRESULT __stdcall D3D9Proxy::QueryInterface(REFIID riid, void** ppvObj)
{
    if (riid == IID_IUnknown || riid == IID_IDirect3D9)
    {
        *ppvObj = static_cast<IDirect3D9*>(this);
        AddRef();
        return S_OK;
    }
    // Anything else (e.g. IDirect3D9Ex) - ask the real object and return it
    // unwrapped. We log it because an Ex interface changes our device path.
    HRESULT hr = m_real->QueryInterface(riid, ppvObj);
    LOG_WARN("QueryInterface for non-IDirect3D9 IID (hr=0x%08X) - returning UNWRAPPED object", hr);
    return hr;
}

ULONG __stdcall D3D9Proxy::AddRef()
{
    return ++m_refCount;
}

ULONG __stdcall D3D9Proxy::Release()
{
    ULONG count = --m_refCount;
    if (count == 0)
    {
        m_real->Release();
        delete this;
    }
    return count;
}

HRESULT __stdcall D3D9Proxy::RegisterSoftwareDevice(void* pInitializeFunction)
{
    return m_real->RegisterSoftwareDevice(pInitializeFunction);
}

UINT __stdcall D3D9Proxy::GetAdapterCount()
{
    return m_real->GetAdapterCount();
}

HRESULT __stdcall D3D9Proxy::GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier)
{
    return m_real->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
}

UINT __stdcall D3D9Proxy::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format)
{
    return m_real->GetAdapterModeCount(Adapter, Format);
}

HRESULT __stdcall D3D9Proxy::EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode)
{
    return m_real->EnumAdapterModes(Adapter, Format, Mode, pMode);
}

HRESULT __stdcall D3D9Proxy::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode)
{
    return m_real->GetAdapterDisplayMode(Adapter, pMode);
}

HRESULT __stdcall D3D9Proxy::CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed)
{
    return m_real->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);
}

HRESULT __stdcall D3D9Proxy::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat)
{
    return m_real->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
}

HRESULT __stdcall D3D9Proxy::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels)
{
    return m_real->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);
}

HRESULT __stdcall D3D9Proxy::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat)
{
    return m_real->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
}

HRESULT __stdcall D3D9Proxy::CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat)
{
    return m_real->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat);
}

HRESULT __stdcall D3D9Proxy::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps)
{
    return m_real->GetDeviceCaps(Adapter, DeviceType, pCaps);
}

HMONITOR __stdcall D3D9Proxy::GetAdapterMonitor(UINT Adapter)
{
    return m_real->GetAdapterMonitor(Adapter);
}

HRESULT __stdcall D3D9Proxy::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                                          DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
                                          IDirect3DDevice9** ppReturnedDeviceInterface)
{
    LOG_INFO("CreateDevice(adapter=%u, type=%d, %ux%u, windowed=%d)",
             Adapter, DeviceType,
             pPresentationParameters ? pPresentationParameters->BackBufferWidth : 0,
             pPresentationParameters ? pPresentationParameters->BackBufferHeight : 0,
             pPresentationParameters ? pPresentationParameters->Windowed : 0);

    HRESULT hr = m_real->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                                      pPresentationParameters, ppReturnedDeviceInterface);
    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
    {
        LOG_INFO("Device created OK (%p) - installing hooks", *ppReturnedDeviceInterface);
        DeviceHooks::Get().OnDeviceCreated(*ppReturnedDeviceInterface, pPresentationParameters);
    }
    else
    {
        LOG_ERROR("CreateDevice failed: hr=0x%08X", hr);
    }
    return hr;
}
