// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Minimal Win32 shim for HOST (Linux) builds of the integration tests.
// Only what the compiled TUs use. NOT used by production builds.

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cerrno>

typedef long        HRESULT;
typedef int32_t     LONG;
typedef uint32_t    DWORD;
typedef uint32_t    ULONG;
typedef uint64_t    ULONGLONG;
typedef uint32_t    UINT;
typedef int32_t     INT;
typedef int         WINBOOL;
typedef uint16_t    WORD;
typedef int16_t     SHORT;
typedef uint8_t     BYTE;
typedef int         BOOL;
typedef void*       HANDLE;
typedef void*       HMODULE;
typedef void*       HWND;
typedef void*       HDC;
typedef void*       HMONITOR;
typedef void*       HINSTANCE;
typedef void*       LPVOID;
typedef char*       LPSTR;
typedef const char* LPCSTR;
typedef uint32_t    UINT_PTR;
typedef int32_t     errno_t;

#define TRUE  1
#define FALSE 0

#define S_OK       ((HRESULT)0L)
// HRESULT constants must be sign-extended through int32_t on LP64 hosts;
// a raw 0x80004005L is positive there and would defeat FAILED().
#define E_FAIL     ((HRESULT)(int32_t)0x80004005u)
#define E_POINTER  ((HRESULT)(int32_t)0x80004003u)
#define E_NOTIMPL  ((HRESULT)(int32_t)0x80004001u)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr)    (((HRESULT)(hr)) < 0)

#define CONST const
#define __stdcall
#define __cdecl
#define WINAPI __stdcall
#define CALLBACK __stdcall
#define __forceinline inline
#define WIN32_LEAN_AND_MEAN
// Host shim: use an object pointer so production reinterpret_casts to each
// exported function type do not trip GCC's -Wcast-function-type.
typedef void* FARPROC;

typedef struct _GUID {
    uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8];
} GUID;
typedef const GUID& REFGUID;
typedef const GUID& REFIID;
typedef GUID IID;
inline bool operator==(REFGUID a, REFGUID b) { return memcmp(&a, &b, sizeof(GUID)) == 0; }
inline bool operator!=(REFGUID a, REFGUID b) { return !(a == b); }

typedef struct tagRECT { LONG left, top, right, bottom; } RECT;
typedef struct tagPOINT { LONG x, y; } POINT;
typedef struct tagPALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; } PALETTEENTRY;

// Minimal SendInput surface used by MotionInput's production sink.
typedef struct tagMOUSEINPUT {
    LONG dx; LONG dy; DWORD mouseData; DWORD dwFlags; DWORD time; UINT_PTR dwExtraInfo;
} MOUSEINPUT;
typedef struct tagKEYBDINPUT {
    WORD wVk; WORD wScan; DWORD dwFlags; DWORD time; UINT_PTR dwExtraInfo;
} KEYBDINPUT;
typedef struct tagINPUT {
    DWORD type;
    union { MOUSEINPUT mi; KEYBDINPUT ki; };
} INPUT;
#define INPUT_KEYBOARD 1
#define INPUT_MOUSE    0
#define KEYEVENTF_KEYUP 0x0002
#define MOUSEEVENTF_MOVE       0x0001
#define MOUSEEVENTF_LEFTDOWN   0x0002
#define MOUSEEVENTF_LEFTUP     0x0004
#define MOUSEEVENTF_RIGHTDOWN  0x0008
#define MOUSEEVENTF_RIGHTUP    0x0010
extern "C" UINT SendInput(UINT nInputs, INPUT* pInputs, int cbSize);
typedef struct _RGNDATA { char opaque[16]; } RGNDATA;

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    int64_t QuadPart;
} LARGE_INTEGER;
typedef struct _FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; } FILETIME;
typedef struct _LUID { DWORD LowPart; LONG HighPart; } LUID;

#define MAX_PATH 260
#define GENERIC_READ          0x80000000u
#define FILE_SHARE_READ       0x00000001u
#define OPEN_EXISTING         3u
#define FILE_ATTRIBUTE_NORMAL 0x00000080u
#define INVALID_HANDLE_VALUE  ((HANDLE)(intptr_t)-1)
#define DLL_PROCESS_ATTACH    1u
#define DLL_PROCESS_DETACH    0u

// process/PE inspection stubs (CameraScan, Logger, Config IniPath)
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS        0x00000004
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT  0x00000002

typedef struct _MEMORY_BASIC_INFORMATION {
    void*  BaseAddress;
    void*  AllocationBase;
    DWORD  AllocationProtect;
    size_t RegionSize;
    DWORD  State;
    DWORD  Protect;
    DWORD  Type;
} MEMORY_BASIC_INFORMATION;

#define PAGE_NOACCESS  0x01
#define MEM_COMMIT     0x1000

// ---- function declarations (implemented in winshim_impl.cpp) ----
extern "C" {
uint64_t GetTickCount64();
void     WinShimSetTickCount64(uint64_t ms); // tests only: deterministic clock
BOOL     QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
BOOL     QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);
BOOL     GetModuleHandleExA(DWORD dwFlags, LPCSTR lpModuleName, HMODULE* phModule);
DWORD    GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
UINT     GetPrivateProfileIntA(LPCSTR lpAppName, LPCSTR lpKeyName, INT nDefault, LPCSTR lpFileName);
DWORD    GetPrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpDefault,
                                  LPSTR lpReturnedString, DWORD nSize, LPCSTR lpFileName);
SHORT    VkKeyScanA(char ch);
size_t   VirtualQuery(const void* lpAddress, MEMORY_BASIC_INFORMATION* lpBuffer, size_t dwLength);
errno_t  fopen_s(FILE** pFile, const char* filename, const char* mode);
errno_t  strcpy_s(char* dest, size_t destSize, const char* src);
errno_t  strcat_s(char* dest, size_t destSize, const char* src);
errno_t  sscanf_s(const char* buffer, const char* format, ...);
}

// critical sections (no-op in single-threaded tests)
typedef struct _CRITICAL_SECTION { char opaque[40]; } CRITICAL_SECTION;
extern "C" {
void InitializeCriticalSection(CRITICAL_SECTION* cs);
void DeleteCriticalSection(CRITICAL_SECTION* cs);
void EnterCriticalSection(CRITICAL_SECTION* cs);
void LeaveCriticalSection(CRITICAL_SECTION* cs);
}

// VK codes used by Config
// vectored exception handling (Logger crash handler; no-op in tests)
typedef struct _EXCEPTION_RECORD {
    DWORD ExceptionCode;
    DWORD ExceptionFlags;
    struct _EXCEPTION_RECORD* ExceptionRecord;
    void* ExceptionAddress;
    DWORD NumberParameters;
    uintptr_t ExceptionInformation[15];
} EXCEPTION_RECORD, *PEXCEPTION_RECORD;
typedef struct _CONTEXT { char opaque[256]; } CONTEXT, *PCONTEXT;
typedef struct _EXCEPTION_POINTERS {
    PEXCEPTION_RECORD ExceptionRecord;
    PCONTEXT ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;
typedef long (__stdcall *PVECTORED_EXCEPTION_HANDLER)(PEXCEPTION_POINTERS);
#define EXCEPTION_ACCESS_VIOLATION    0xC0000005u
#define EXCEPTION_ILLEGAL_INSTRUCTION 0xC000001Du
#define EXCEPTION_STACK_OVERFLOW      0xC00000FDu
#define EXCEPTION_INT_DIVIDE_BY_ZERO  0xC0000094u
#define EXCEPTION_PRIV_INSTRUCTION    0xC0000096u
#define EXCEPTION_CONTINUE_SEARCH     0
#define MOVEFILE_REPLACE_EXISTING     0x1u
#define MOUSEEVENTF_ABSOLUTE          0x8000u
extern "C" {
void* AddVectoredExceptionHandler(unsigned long first, PVECTORED_EXCEPTION_HANDLER handler);
unsigned long RemoveVectoredExceptionHandler(void* handle);
BOOL MoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags);
}

// VK codes used by Config
#define VK_SPACE   0x20
#define VK_SHIFT   0x10
#define VK_CONTROL 0x11
#define VK_MENU    0x12
#define VK_TAB     0x09
#define VK_ESCAPE  0x1B
#define VK_RETURN  0x0D
#define VK_F1      0x70

// array-size-deducing overloads (MSVC-compatible templates)
template <size_t N> errno_t strcpy_s(char (&d)[N], const char* s) { return strcpy_s(d, N, s); }
template <size_t N> errno_t strcat_s(char (&d)[N], const char* s) { return strcat_s(d, N, s); }

extern "C" {
FILE*  _fsopen(const char* filename, const char* mode, int shflag);
int    localtime_s(struct tm* out, const time_t* t);
HMODULE GetModuleHandleA(LPCSTR lpModuleName);
UINT    GetSystemDirectoryA(LPSTR lpBuffer, UINT uSize);
HMODULE LoadLibraryA(LPCSTR lpLibFileName);
FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
DWORD   GetLastError();
HANDLE  CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                    void* lpSecurityAttributes, DWORD dwCreationDisposition,
                    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL    GetFileSizeEx(HANDLE hFile, LARGE_INTEGER* lpFileSize);
BOOL    GetFileTime(HANDLE hFile, FILETIME* lpCreationTime, FILETIME* lpLastAccessTime,
                    FILETIME* lpLastWriteTime);
BOOL    CloseHandle(HANDLE hObject);
BOOL    DisableThreadLibraryCalls(HMODULE hLibModule);
}

// minimal PE structs for CameraScan's .data probe
typedef struct _IMAGE_DOS_HEADER { WORD e_magic; uint8_t opaque[58]; LONG e_lfanew; } IMAGE_DOS_HEADER;
typedef struct _IMAGE_SECTION_HEADER {
    char Name[8]; union { uint32_t PhysicalAddress; uint32_t VirtualSize; } Misc; uint32_t VirtualAddress; uint32_t SizeOfRawData;
    uint32_t PointerToRawData; uint8_t rest[16];
} IMAGE_SECTION_HEADER;
typedef struct _IMAGE_NT_HEADERS {
    uint32_t Signature; struct { uint16_t Machine; uint16_t NumberOfSections; uint8_t rest[16]; } FileHeader; uint8_t optionalHeader[224];
    IMAGE_SECTION_HEADER sections[16]; // shim: inline storage
} IMAGE_NT_HEADERS;
#define IMAGE_FIRST_SECTION(h) ((h)->sections)
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x40

// message-hook surface for MouseMap
typedef void* HHOOK;
typedef uintptr_t WPARAM;
typedef intptr_t  LPARAM;
typedef intptr_t  LRESULT;
typedef struct tagMSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG;
#define WH_GETMESSAGE 3
#define HC_ACTION 0
#define WM_MOUSEMOVE     0x0200
#define WM_LBUTTONDOWN   0x0201
#define WM_LBUTTONUP     0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN   0x0204
#define WM_RBUTTONUP     0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN   0x0207
#define WM_MBUTTONUP     0x0208
#define WM_MBUTTONDBLCLK 0x0209
#define LOWORD(l) ((WORD)((uintptr_t)(l) & 0xffff))
#define HIWORD(l) ((WORD)(((uintptr_t)(l) >> 16) & 0xffff))
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
extern "C" {
HHOOK SetWindowsHookExA(int, LRESULT (__stdcall *)(int, WPARAM, LPARAM), HMODULE, DWORD);
BOOL UnhookWindowsHookEx(HHOOK);
LRESULT CallNextHookEx(HHOOK, int, WPARAM, LPARAM);
DWORD GetWindowThreadProcessId(HWND, DWORD*);
BOOL GetClientRect(HWND, RECT*);
}

#define IMAGE_DOS_SIGNATURE 0x5A4D
typedef IMAGE_NT_HEADERS IMAGE_NT_HEADERS32;

#define IMAGE_NT_SIGNATURE 0x00004550
#define PAGE_READWRITE 0x04
extern "C" SHORT GetAsyncKeyState(int vKey);
#define PAGE_READONLY 0x02
#define PAGE_WRITECOPY 0x08
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_GUARD 0x100
