// SPDX-License-Identifier: GPL-3.0-only
// Implementations for the Win32 shim (host builds of the integration tests).
#include "windows.h"
#include <chrono>
#include <string>
#include <map>
#include <cctype>

namespace
{
    // Windows-style path ("Z:\dir\file") -> host path ("/dir/file")
    std::string HostPath(const char* p)
    {
        std::string s = p ? p : "";
        if (s.size() > 2 && s[1] == ':') s = s.substr(2);
        for (auto& c : s) if (c == '\\') c = '/';
        return s;
    }

    std::string IniFileDir()
    {
        const char* env = getenv("WBVR_TEST_DIR");
        return env ? env : "/tmp/wbvr_test";
    }

    // Tiny INI reader: [section] + key=value, ';' comments, case-insensitive.
    struct Ini { std::map<std::string, std::map<std::string, std::string>> sec; };
    bool LoadIni(const char* path, Ini& ini)
    {
        FILE* f = fopen(HostPath(path).c_str(), "r");
        if (!f) return false;
        char line[1024]; std::string cur;
        auto lower = [](std::string s){ for (auto& c : s) c = (char)tolower((unsigned char)c); return s; };
        auto trim = [](std::string s){
            size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
            return a == std::string::npos ? "" : s.substr(a, b - a + 1); };
        while (fgets(line, sizeof(line), f))
        {
            std::string l = trim(line);
            if (l.empty() || l[0] == ';') continue;
            if (l[0] == '[' && l.back() == ']') { cur = lower(l.substr(1, l.size() - 2)); continue; }
            size_t eq = l.find('=');
            if (eq != std::string::npos)
                ini.sec[cur][lower(trim(l.substr(0, eq)))] = trim(l.substr(eq + 1));
        }
        fclose(f);
        return true;
    }
    bool IniGet(const char* sec, const char* key, const char* path, std::string& out)
    {
        Ini ini;
        if (!LoadIni(path, ini)) return false;
        auto lower = [](std::string s){ for (auto& c : s) c = (char)tolower((unsigned char)c); return s; };
        auto si = ini.sec.find(lower(sec));
        if (si == ini.sec.end()) return false;
        auto ki = si->second.find(lower(key));
        if (ki == si->second.end()) return false;
        out = ki->second;
        return true;
    }
}

extern "C" {

static uint64_t g_fakeTickMs = 0;
static bool     g_fakeTickEnabled = false;

uint64_t GetTickCount64()
{
    if (g_fakeTickEnabled) return g_fakeTickMs;
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now() - t0).count();
}
void WinShimSetTickCount64(uint64_t ms)
{
    g_fakeTickMs = ms;
    g_fakeTickEnabled = true;
}
BOOL QueryPerformanceCounter(LARGE_INTEGER* c)
{
    if (g_fakeTickEnabled) { c->QuadPart = (int64_t)g_fakeTickMs * 1000000LL; return TRUE; }
    using namespace std::chrono;
    c->QuadPart = (int64_t)duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    return TRUE;
}
BOOL QueryPerformanceFrequency(LARGE_INTEGER* f) { f->QuadPart = 1000000000LL; return TRUE; }

BOOL GetModuleHandleExA(DWORD, LPCSTR, HMODULE* ph) { *ph = (HMODULE)0x1; return TRUE; }
DWORD GetModuleFileNameA(HMODULE, LPSTR out, DWORD size)
{
    std::string p = "Z:" + IniFileDir() + "\\d3d9.dll";
    strncpy(out, p.c_str(), size - 1); out[size - 1] = 0;
    return (DWORD)strlen(out);
}

UINT GetPrivateProfileIntA(LPCSTR sec, LPCSTR key, INT def, LPCSTR path)
{
    std::string v;
    return IniGet(sec, key, path, v) ? (UINT)strtol(v.c_str(), nullptr, 0) : (UINT)def;
}
DWORD GetPrivateProfileStringA(LPCSTR sec, LPCSTR key, LPCSTR def, LPSTR out, DWORD size, LPCSTR path)
{
    std::string v = def ? def : "";
    IniGet(sec, key, path, v);
    strncpy(out, v.c_str(), size - 1); out[size - 1] = 0;
    return (DWORD)strlen(out);
}

SHORT VkKeyScanA(char ch)
{
    if (ch >= 'a' && ch <= 'z') return (SHORT)(ch - 'a' + 'A');
    if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) return (SHORT)ch;
    return -1;
}

size_t VirtualQuery(const void*, MEMORY_BASIC_INFORMATION*, size_t) { return 0; }

errno_t fopen_s(FILE** pp, const char* name, const char* mode)
{
    *pp = fopen(HostPath(name).c_str(), mode);
    return *pp ? 0 : errno;
}
errno_t strcpy_s(char* d, size_t n, const char* s)
{
    if (!d || !s || strlen(s) >= n) return EINVAL;
    strcpy(d, s); return 0;
}
errno_t strcat_s(char* d, size_t n, const char* s)
{
    if (!d || !s || strlen(d) + strlen(s) >= n) return EINVAL;
    strcat(d, s); return 0;
}
errno_t sscanf_s(const char* buf, const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    // our single use: "%f , %f , %f" with float* args - glibc sscanf matches
    int r = vsscanf(buf, fmt, ap);
    va_end(ap);
    return r;
}

} // extern "C"
// critical section no-ops
extern "C" {
void InitializeCriticalSection(CRITICAL_SECTION*) {}
void DeleteCriticalSection(CRITICAL_SECTION*) {}
void EnterCriticalSection(CRITICAL_SECTION*) {}
void LeaveCriticalSection(CRITICAL_SECTION*) {}
}
#include <ctime>
extern "C" {
FILE* _fsopen(const char* name, const char* mode, int) { return fopen(HostPath(name).c_str(), mode); }
int localtime_s(struct tm* out, const time_t* t) { return localtime_r(t, out) ? 0 : 1; }
HMODULE GetModuleHandleA(LPCSTR) { return nullptr; }
}
extern const IID IID_IUnknown = {0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
extern const IID IID_IDirect3D9 = {0x81BDCBCA,0x64D4,0x426D,{0xAE,0x8D,0xAD,0x01,0x47,0xF4,0x27,0x5C}};
extern const IID IID_IDirect3D9Ex = {0x02177241,0x69FC,0x400C,{0x8F,0xF1,0x93,0xA4,0x4D,0xF6,0x86,0x1D}};
extern const IID IID_IDirect3DDevice9 = {0xD0223B96,0xBF7A,0x43FD,{0x92,0xBD,0xA4,0x3B,0x0D,0x82,0xB9,0xEB}};
extern const IID IID_IDirect3DTexture9 = {0x85C31227,0x3DE5,0x4F00,{0x9B,0x3A,0xF1,0x1A,0xC3,0x8C,0x18,0xB5}};
extern const IID IID_IDirect3DDevice9Ex = {0xB18B10CE,0x2649,0x405A,{0x87,0x0F,0x95,0xF7,0x77,0xD4,0x31,0x3A}};
extern "C" SHORT GetAsyncKeyState(int) { return 0; }
extern "C" UINT SendInput(UINT n, INPUT*, int) { return n; }

// VEH + file-move stubs: no real crash plumbing or rename semantics in tests.
extern "C" {
void* AddVectoredExceptionHandler(unsigned long, PVECTORED_EXCEPTION_HANDLER) { return (void*)0xCA55; }
unsigned long RemoveVectoredExceptionHandler(void*) { return 1; }
BOOL MoveFileExA(LPCSTR from, LPCSTR to, DWORD) {
    std::remove(HostPath(to).c_str());
    return std::rename(HostPath(from).c_str(), HostPath(to).c_str()) == 0 ? 1 : 0;
}
}

// MouseMap hook stubs: no real message pump in headless tests.
extern "C" {
HHOOK SetWindowsHookExA(int, LRESULT (__stdcall *)(int, WPARAM, LPARAM), HMODULE, DWORD) { return (HHOOK)0xBEEF; }
BOOL UnhookWindowsHookEx(HHOOK) { return 1; }
LRESULT CallNextHookEx(HHOOK, int, WPARAM, LPARAM) { return 0; }
DWORD GetWindowThreadProcessId(HWND, DWORD*) { return 1; }
BOOL GetClientRect(HWND, RECT* rc) { rc->left = rc->top = 0; rc->right = 1600; rc->bottom = 900; return 1; }
}
