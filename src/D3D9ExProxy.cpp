#include "D3D9ExProxy.h"
#include "DeviceHooks.h"
#include "Logger.h"

D3D9ExProxy::D3D9ExProxy(IDirect3D9Ex* real)
    : m_real(real), m_refCount(1)
{
}

HRESULT __stdcall D3D9ExProxy::QueryInterface(REFIID riid, void** ppvObj)
{
    if (riid == IID_IUnknown || riid == IID_IDirect3D9 || riid == IID_IDirect3D9Ex)
    {
        *ppvObj = static_cast<IDirect3D9Ex*>(this);
        AddRef();
        return S_OK;
    }
    return m_real->QueryInterface(riid, ppvObj);
}

ULONG __stdcall D3D9ExProxy::AddRef()
{
    return ++m_refCount;
}

ULONG __stdcall D3D9ExProxy::Release()
{
    ULONG count = --m_refCount;
    if (count == 0)
    {
        m_real->Release();
        delete this;
    }
    return count;
}

HRESULT __stdcall D3D9ExProxy::RegisterSoftwareDevice(void* pInitializeFunction)
{
    return m_real->RegisterSoftwareDevice(pInitializeFunction);
}

UINT __stdcall D3D9ExProxy::GetAdapterCount()
{
    return m_real->GetAdapterCount();
}

HRESULT __stdcall D3D9ExProxy::GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier)
{
    return m_real->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
}

UINT __stdcall D3D9ExProxy::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format)
{
    return m_real->GetAdapterModeCount(Adapter, Format);
}

HRESULT __stdcall D3D9ExProxy::EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode)
{
    return m_real->EnumAdapterModes(Adapter, Format, Mode, pMode);
}

HRESULT __stdcall D3D9ExProxy::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode)
{
    return m_real->GetAdapterDisplayMode(Adapter, pMode);
}

HRESULT __stdcall D3D9ExProxy::CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed)
{
    return m_real->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);
}

HRESULT __stdcall D3D9ExProxy::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat)
{
    return m_real->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
}

HRESULT __stdcall D3D9ExProxy::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels)
{
    return m_real->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);
}

HRESULT __stdcall D3D9ExProxy::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat)
{
    return m_real->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
}

HRESULT __stdcall D3D9ExProxy::CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat)
{
    return m_real->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat);
}

HRESULT __stdcall D3D9ExProxy::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps)
{
    return m_real->GetDeviceCaps(Adapter, DeviceType, pCaps);
}

HMONITOR __stdcall D3D9ExProxy::GetAdapterMonitor(UINT Adapter)
{
    return m_real->GetAdapterMonitor(Adapter);
}

HRESULT __stdcall D3D9ExProxy::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                                            DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
                                            IDirect3DDevice9** ppReturnedDeviceInterface)
{
    LOG_INFO("ExProxy::CreateDevice(adapter=%u) - wrapping", Adapter);
    HRESULT hr = m_real->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                                      pPresentationParameters, ppReturnedDeviceInterface);
    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
        DeviceHooks::Get().OnDeviceCreated(*ppReturnedDeviceInterface, pPresentationParameters);
    return hr;
}

UINT __stdcall D3D9ExProxy::GetAdapterModeCountEx(UINT Adapter, const D3DDISPLAYMODEFILTER* pFilter)
{
    return m_real->GetAdapterModeCountEx(Adapter, pFilter);
}

HRESULT __stdcall D3D9ExProxy::EnumAdapterModesEx(UINT Adapter, const D3DDISPLAYMODEFILTER* pFilter, UINT Mode, D3DDISPLAYMODEEX* pMode)
{
    return m_real->EnumAdapterModesEx(Adapter, pFilter, Mode, pMode);
}

HRESULT __stdcall D3D9ExProxy::GetAdapterDisplayModeEx(UINT Adapter, D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation)
{
    return m_real->GetAdapterDisplayModeEx(Adapter, pMode, pRotation);
}

HRESULT __stdcall D3D9ExProxy::CreateDeviceEx(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                                              DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
                                              D3DDISPLAYMODEEX* pFullscreenDisplayMode,
                                              IDirect3DDevice9Ex** ppReturnedDeviceInterface)
{
    LOG_INFO("ExProxy::CreateDeviceEx(adapter=%u, %ux%u) - wrapping",
             Adapter,
             pPresentationParameters ? pPresentationParameters->BackBufferWidth : 0,
             pPresentationParameters ? pPresentationParameters->BackBufferHeight : 0);

    HRESULT hr = m_real->CreateDeviceEx(Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                                        pPresentationParameters, pFullscreenDisplayMode,
                                        ppReturnedDeviceInterface);
    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
    {
        // IDirect3DDevice9Ex's vtable begins with the identical IDirect3DDevice9
        // layout, so the same indices/hooks apply unchanged.
        DeviceHooks::Get().OnDeviceCreated(*ppReturnedDeviceInterface, pPresentationParameters);
    }
    return hr;
}

HRESULT __stdcall D3D9ExProxy::GetAdapterLUID(UINT Adapter, LUID* pLUID)
{
    return m_real->GetAdapterLUID(Adapter, pLUID);
}
