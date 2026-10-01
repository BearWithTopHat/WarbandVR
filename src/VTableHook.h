#pragma once

// VTableHook: swap individual entries in a COM object's vtable.
//
// Why vtable swapping instead of an API hooker (MinHook/Detours)?
//  - COM methods are reached through a per-class function table; patching the
//    table for the ONE device instance the game creates is precise, cheap and
//    100% user-mode - no trampolines, no instruction decoding.
//  - Caveat: it only affects objects whose vtable pointer we swapped. That's
//    exactly what we want: we hook the device the game gets, nothing else.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#include <vector>

class VTableHook
{
public:
    // obj: pointer to a COM object (i.e. pointer to a vtable pointer).
    explicit VTableHook(void* obj)
        : m_obj(obj), m_vtable(*reinterpret_cast<void***>(obj))
    {
    }

    // Replace vtable[index] with detour; original is stored to *originalOut.
    bool Hook(int index, void* detour, void** originalOut)
    {
        void** entry = m_vtable + index;
        if (originalOut) *originalOut = *entry;

        DWORD oldProtect = 0;
        if (!VirtualProtect(entry, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;
        *entry = detour;
        VirtualProtect(entry, sizeof(void*), oldProtect, &oldProtect);

        m_entries.push_back({ index, originalOut ? *originalOut : nullptr });
        return true;
    }

    // Restore everything we changed.
    void UnhookAll()
    {
        for (auto& e : m_entries)
        {
            void** entry = m_vtable + e.index;
            DWORD oldProtect = 0;
            if (VirtualProtect(entry, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
            {
                *entry = e.original;
                VirtualProtect(entry, sizeof(void*), oldProtect, &oldProtect);
            }
        }
        m_entries.clear();
    }

private:
    struct Entry { int index; void* original; };

    void*  m_obj;
    void** m_vtable;
    std::vector<Entry> m_entries;
};
