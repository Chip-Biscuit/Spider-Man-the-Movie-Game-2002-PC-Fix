/**
* Copyright (C) 2020 Elisha Riedlinger
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*/
// Chip spiderman 2002 patches + fullscreen Bink overlay (CopyToBuffer path)

#define NOMINMAX
#include "d3d8.h"
#include <d3dx8.h>
#include "iathook.h"
#include "helpers.h"

#include <windows.h>
#include <Psapi.h>
#include <Xinput.h>
#include <imagehlp.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <list>
#include <string>
#include <vector>
#include "CharacterSwap.h"

#pragma comment(lib, "imagehlp.lib")
#pragma comment(lib, "Xinput9_1_0.lib")
#pragma comment(lib, "d3dx8.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "libMinHook-x86.lib")

// Debug macros
#define DX_PRINT(x)   do { std::cout << x << std::endl; } while(0)
#define DX_ERROR(x)   do { std::cerr << x << std::endl; } while(0)
#define DX_MBPRINT(x) MessageBoxA(NULL, x, "d3d8 Wrapper", MB_OK)
#define DX_MBERROR(x) MessageBoxA(NULL, x, "d3d8 Wrapper - Error", MB_ICONERROR | MB_OK)

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// ============================================================================
// Global / D3D state
// ============================================================================

Direct3D8EnableMaximizedWindowedModeShimProc m_pDirect3D8EnableMaximizedWindowedModeShim;
ValidatePixelShaderProc                      m_pValidatePixelShader;
ValidateVertexShaderProc                     m_pValidateVertexShader;
DebugSetMuteProc                             m_pDebugSetMute;
Direct3DCreate8Proc                          m_pDirect3DCreate8;

HWND    g_hFocusWindow = NULL;
HMODULE g_hWrapperModule = NULL;
HMODULE d3d8dll = NULL;

bool  bForceWindowedMode = 0;
bool  bDirect3D8DisableMaximizedWindowedModeShim = 0;
bool  bUsePrimaryMonitor = 0;
bool  bCenterWindow = 0;
bool  bBorderlessFullscreen = 0;
bool  bAlwaysOnTop = 0;
bool  bDoNotNotifyOnTaskSwitch = 0;
bool  bDisplayFPSCounter = 0;
bool  bCustomControllerSuppport = 0;
float fFPSLimit = 0.0f;
int   nFullScreenRefreshRateInHz = 0;

char WinDir[MAX_PATH + 1] = { 0 };


// ============================================================================
// --- FOV --------------------------------------------------------------------
const std::vector<BYTE> commonHexEdit = { 0x35, 0xFA, 0x8E, 0x3C };

struct HexEdit3 {
    std::vector<BYTE> modified;
    size_t offset;
};

HexEdit3 CreateHexEditFromFOV(int aspectIndex3) {
    HexEdit3 edit{};
    switch (aspectIndex3) {
    case 1: edit.modified = { 0x00, 0x00, 0xAB, 0x3C }; break;
    case 2: edit.modified = { 0x00, 0x00, 0xBE, 0x3C }; break;
    case 3: edit.modified = { 0x00, 0x00, 0xFF, 0x3C }; break;
    case 4: edit.modified = { 0x00, 0x00, 0x3F, 0x3D }; break;
    default: DX_ERROR("[HEX] Invalid FOV index"); break;
    }
    edit.offset = 0;
    return edit;
}

void PerformHexEdit3(LPBYTE lpAddress, DWORD moduleSize, const std::vector<BYTE>& commonEdit, const std::vector<BYTE>& modifiedEdit, size_t offset) {
    for (DWORD i = 0; i < moduleSize - modifiedEdit.size(); ++i) {
        if (memcmp(lpAddress + i, commonEdit.data(), commonEdit.size()) == 0) {
            DX_PRINT("[HEX] FOV pattern found");
            LPVOID lpAddressToWrite = lpAddress + i + offset;
            DWORD oldProtection;
            if (!VirtualProtect(lpAddressToWrite, modifiedEdit.size(), PAGE_EXECUTE_READWRITE, &oldProtection)) {
                DX_ERROR("[HEX] FOV VirtualProtect failed");
                return;
            }
            memcpy(lpAddressToWrite, modifiedEdit.data(), modifiedEdit.size());
            DWORD dummy;
            VirtualProtect(lpAddressToWrite, modifiedEdit.size(), oldProtection, &dummy);
            DX_PRINT("[HEX] FOV hex edit applied");
            return;
        }
    }
    DX_PRINT("[HEX] FOV pattern not found");
}

void PerformHexEdits3() {
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) { DX_ERROR("[HEX] FOV GetModuleHandle failed"); return; }

    MODULEINFO moduleInfo{};
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo))) {
        DX_ERROR("[HEX] FOV GetModuleInformation failed");
        return;
    }

    DWORD  moduleSize = moduleInfo.SizeOfImage;
    LPBYTE lpAddress = reinterpret_cast<LPBYTE>(moduleInfo.lpBaseOfDll);

    char iniPath[MAX_PATH] = {};
    HMODULE hm = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&Direct3DCreate8, &hm);
    GetModuleFileNameA(hm, iniPath, sizeof(iniPath));
    strcpy(strrchr(iniPath, '\\'), "\\d3d8.ini");

    int aspectIndex3 = GetPrivateProfileIntA("FOV", "fov", 0, iniPath);
    DX_PRINT("[INI] FOV.fov = " << aspectIndex3);
    if (aspectIndex3 == 0) return;

    HexEdit3 edit = CreateHexEditFromFOV(aspectIndex3);
    if (edit.modified.empty()) return;

    PerformHexEdit3(lpAddress, moduleSize, commonHexEdit, edit.modified, edit.offset);
}

// --- Resolution --------------------------------------------------------------
const std::vector<BYTE> commonHexEdit1 = { 0x80, 0x02, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00 };
const std::vector<BYTE> commonHexEdit2 = { 0x80, 0x02, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x20, 0x03, 0x00, 0x00, 0x58, 0x02, 0x00, 0x00 };

struct HexEdit {
    std::vector<BYTE> modified;
    size_t offset;
};

HexEdit CreateHexEdit1(int width, int height, size_t offset) {
    HexEdit edit;
    edit.offset = offset;
    BYTE widthLow = static_cast<BYTE>(width & 0xFF);
    BYTE widthHigh = static_cast<BYTE>((width >> 8) & 0xFF);
    BYTE heightLow = static_cast<BYTE>(height & 0xFF);
    BYTE heightHigh = static_cast<BYTE>((height >> 8) & 0xFF);
    edit.modified = { widthLow, widthHigh, 0x00, 0x00, heightLow, heightHigh, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00 };
    return edit;
}

HexEdit CreateHexEdit2(int width, int height, size_t offset) {
    HexEdit edit;
    edit.offset = offset;
    BYTE widthLow = static_cast<BYTE>(width & 0xFF);
    BYTE widthHigh = static_cast<BYTE>((width >> 8) & 0xFF);
    BYTE heightLow = static_cast<BYTE>(height & 0xFF);
    BYTE heightHigh = static_cast<BYTE>((height >> 8) & 0xFF);
    edit.modified = { widthLow, widthHigh, 0x00, 0x00, heightLow, heightHigh, 0x00, 0x00, 0x20, 0x03, 0x00, 0x00, 0x58, 0x02, 0x00, 0x00 };
    return edit;
}

void PerformHexEdit(LPBYTE lpAddress, DWORD moduleSize, const std::vector<BYTE>& pattern, const HexEdit& edit) {
    for (DWORD i = 0; i < moduleSize - pattern.size(); ++i) {
        if (memcmp(lpAddress + i, pattern.data(), pattern.size()) == 0) {
            LPVOID lpAddressToWrite = lpAddress + i + edit.offset;
            DWORD oldProtect;
            if (!VirtualProtect(lpAddressToWrite, edit.modified.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                DX_ERROR("[HEX] RES VirtualProtect failed");
                return;
            }
            SIZE_T wrote = 0;
            BOOL ok = WriteProcessMemory(GetCurrentProcess(), lpAddressToWrite, edit.modified.data(), edit.modified.size(), &wrote);
            VirtualProtect(lpAddressToWrite, edit.modified.size(), oldProtect, &oldProtect);
            if (!ok || wrote != edit.modified.size()) {
                DX_ERROR("[HEX] RES WriteProcessMemory failed");
                return;
            }
            DX_PRINT("[HEX] RES pattern applied");
            return;
        }
    }
    DX_PRINT("[HEX] RES pattern not found");
}

void PerformHexEdits() {
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) { DX_ERROR("[HEX] RES GetModuleHandle failed"); return; }

    LPBYTE lpAddress = reinterpret_cast<LPBYTE>(hModule);
    DWORD moduleSize = 0;
    TCHAR szFileName[MAX_PATH];

    if (GetModuleFileNameEx(GetCurrentProcess(), hModule, szFileName, MAX_PATH)) {
        HANDLE hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            moduleSize = GetFileSize(hFile, NULL);
            CloseHandle(hFile);
        }
    }
    if (moduleSize == 0) { DX_ERROR("[HEX] RES module size failed"); return; }

    // ini path
    char path[MAX_PATH];
    HMODULE hm = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&PerformHexEdits, &hm);
    GetModuleFileNameA(hm, path, sizeof(path));
    strcpy(strrchr(path, '\\'), "\\d3d8.ini");

    int width = GetPrivateProfileIntA("RESOLUTION", "width", 0, path);
    int height = GetPrivateProfileIntA("RESOLUTION", "height", 0, path);
    DX_PRINT("[INI] RESOLUTION.width=" << width << " height=" << height);
    if (width == 0 || height == 0) return;

    PerformHexEdit(lpAddress, moduleSize, commonHexEdit1, CreateHexEdit1(width, height, 0));
    PerformHexEdit(lpAddress, moduleSize, commonHexEdit2, CreateHexEdit2(width, height, 0));
}

// --- Misc nop patch (as in your code) ---------------------------------------
void PerformHexEdit7(LPBYTE lpAddress, DWORD moduleSize) {
    struct HexEditLocal { std::vector<BYTE> pattern; std::vector<BYTE> newValue; size_t offset; };
    std::vector<HexEditLocal> edits = {
        { { 0xE8, 0x7F, 0x24, 0x07, 0x00 }, { 0x90, 0x90, 0x90, 0x90, 0x90 }, 0 }
    };

    for (const auto& edit : edits) {
        for (DWORD i = 0; i < moduleSize - edit.pattern.size(); ++i) {
            if (memcmp(lpAddress + i, edit.pattern.data(), edit.pattern.size()) == 0) {
                DX_PRINT("[HEX] Misc pattern found");
                LPVOID addr = lpAddress + i + edit.offset;
                DWORD oldProtect;
                if (!VirtualProtect(addr, edit.newValue.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    DX_ERROR("[HEX] Misc VirtualProtect failed");
                    return;
                }
                SIZE_T wrote = 0;
                BOOL ok = WriteProcessMemory(GetCurrentProcess(), addr, edit.newValue.data(), edit.newValue.size(), &wrote);
                VirtualProtect(addr, edit.newValue.size(), oldProtect, &oldProtect);
                if (!ok || wrote != edit.newValue.size()) {
                    DX_ERROR("[HEX] Misc WriteProcessMemory failed");
                    return;
                }
                DX_PRINT("[HEX] Misc hex edit applied");
                break;
            }
        }
    }
}

void PerformHexEdits7() {
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) { DX_ERROR("[HEX] Misc GetModuleHandle failed"); return; }

    LPBYTE lpAddress = reinterpret_cast<LPBYTE>(hModule);
    DWORD moduleSize = 0;
    TCHAR szFileName[MAX_PATH];
    if (GetModuleFileNameEx(GetCurrentProcess(), hModule, szFileName, MAX_PATH)) {
        HANDLE hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            moduleSize = GetFileSize(hFile, NULL);
            CloseHandle(hFile);
        }
    }
    if (moduleSize == 0) { DX_ERROR("[HEX] Misc module size failed"); return; }

    PerformHexEdit7(lpAddress, moduleSize);
}

// ============================================================================
// Window proc hooks (unchanged behavior)
// ============================================================================

std::vector<std::pair<WORD, ULONG_PTR>> WndProcList;

LRESULT WINAPI CustomWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, int idx)
{
    if (hWnd == g_hFocusWindow || _fnIsTopLevelWindow(hWnd))
    {
        if (bAlwaysOnTop) {
            if ((GetWindowLong(hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0)
                SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
        }
        switch (uMsg)
        {
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                if ((HWND)lParam == NULL) return 0;
                DWORD dwPID = 0;
                GetWindowThreadProcessId((HWND)lParam, &dwPID);
                if (dwPID != GetCurrentProcessId()) return 0;
            }
            break;
        case WM_NCACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) return 0;
            break;
        case WM_ACTIVATEAPP:
            if (wParam == FALSE) return 0;
            break;
        case WM_KILLFOCUS:
        {
            if ((HWND)wParam == NULL) return 0;
            DWORD dwPID = 0;
            GetWindowThreadProcessId((HWND)wParam, &dwPID);
            if (dwPID != GetCurrentProcessId()) return 0;
        } break;
        default: break;
        }
    }
    WNDPROC OrigProc = WNDPROC(WndProcList[idx].second);
    return OrigProc(hWnd, uMsg, wParam, lParam);
}

LRESULT WINAPI CustomWndProcA(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WORD wClassAtom = GetClassWord(hWnd, GCW_ATOM);
    if (wClassAtom) {
        for (unsigned int i = 0; i < WndProcList.size(); i++) {
            if (WndProcList[i].first == wClassAtom) return CustomWndProc(hWnd, uMsg, wParam, lParam, i);
        }
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

LRESULT WINAPI CustomWndProcW(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WORD wClassAtom = GetClassWord(hWnd, GCW_ATOM);
    if (wClassAtom) {
        for (unsigned int i = 0; i < WndProcList.size(); i++) {
            if (WndProcList[i].first == wClassAtom) return CustomWndProc(hWnd, uMsg, wParam, lParam, i);
        }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

typedef ATOM(__stdcall* RegisterClassA_fn)(const WNDCLASSA*);
typedef ATOM(__stdcall* RegisterClassW_fn)(const WNDCLASSW*);
typedef ATOM(__stdcall* RegisterClassExA_fn)(const WNDCLASSEXA*);
typedef ATOM(__stdcall* RegisterClassExW_fn)(const WNDCLASSEXW*);
RegisterClassA_fn  oRegisterClassA = NULL;
RegisterClassW_fn  oRegisterClassW = NULL;
RegisterClassExA_fn oRegisterClassExA = NULL;
RegisterClassExW_fn oRegisterClassExW = NULL;

ATOM __stdcall hk_RegisterClassA(WNDCLASSA* lpWndClass)
{
    if (!IsValueIntAtom(DWORD(lpWndClass->lpszClassName))) {
        if (IsSystemClassNameA(lpWndClass->lpszClassName)) return oRegisterClassA(lpWndClass);
    }
    ULONG_PTR pWndProc = ULONG_PTR(lpWndClass->lpfnWndProc);
    lpWndClass->lpfnWndProc = CustomWndProcA;
    WORD wClassAtom = oRegisterClassA(lpWndClass);
    if (wClassAtom != 0) WndProcList.emplace_back(wClassAtom, pWndProc);
    return wClassAtom;
}

ATOM __stdcall hk_RegisterClassW(WNDCLASSW* lpWndClass)
{
    if (!IsValueIntAtom(DWORD(lpWndClass->lpszClassName))) {
        if (IsSystemClassNameW(lpWndClass->lpszClassName)) return oRegisterClassW(lpWndClass);
    }
    ULONG_PTR pWndProc = ULONG_PTR(lpWndClass->lpfnWndProc);
    lpWndClass->lpfnWndProc = CustomWndProcW;
    WORD wClassAtom = oRegisterClassW(lpWndClass);
    if (wClassAtom != 0) WndProcList.emplace_back(wClassAtom, pWndProc);
    return wClassAtom;
}

ATOM __stdcall hk_RegisterClassExA(WNDCLASSEXA* lpWndClass)
{
    if (!IsValueIntAtom(DWORD(lpWndClass->lpszClassName))) {
        if (IsSystemClassNameA(lpWndClass->lpszClassName)) return oRegisterClassExA(lpWndClass);
    }
    ULONG_PTR pWndProc = ULONG_PTR(lpWndClass->lpfnWndProc);
    lpWndClass->lpfnWndProc = CustomWndProcA;
    WORD wClassAtom = oRegisterClassExA(lpWndClass);
    if (wClassAtom != 0) WndProcList.emplace_back(wClassAtom, pWndProc);
    return wClassAtom;
}

ATOM __stdcall hk_RegisterClassExW(WNDCLASSEXW* lpWndClass)
{
    if (!IsValueIntAtom(DWORD(lpWndClass->lpszClassName))) {
        if (IsSystemClassNameW(lpWndClass->lpszClassName)) return oRegisterClassExW(lpWndClass);
    }
    ULONG_PTR pWndProc = ULONG_PTR(lpWndClass->lpfnWndProc);
    lpWndClass->lpfnWndProc = CustomWndProcW;
    WORD wClassAtom = oRegisterClassExW(lpWndClass);
    if (wClassAtom != 0) WndProcList.emplace_back(wClassAtom, pWndProc);
    return wClassAtom;
}

typedef HWND(__stdcall* GetForegroundWindow_fn)(void);
GetForegroundWindow_fn oGetForegroundWindow = NULL;
HWND __stdcall hk_GetForegroundWindow()
{
    if (g_hFocusWindow && IsWindow(g_hFocusWindow)) return g_hFocusWindow;
    return oGetForegroundWindow();
}

typedef HWND(__stdcall* GetActiveWindow_fn)(void);
GetActiveWindow_fn oGetActiveWindow = NULL;
HWND __stdcall hk_GetActiveWindow(void)
{
    HWND hWndActive = oGetActiveWindow();
    if (g_hFocusWindow && hWndActive == NULL && IsWindow(g_hFocusWindow)) {
        if (GetCurrentThreadId() == GetWindowThreadProcessId(g_hFocusWindow, NULL)) return g_hFocusWindow;
    }
    return hWndActive;
}

typedef HWND(__stdcall* GetFocus_fn)(void);
GetFocus_fn oGetFocus = NULL;
HWND __stdcall hk_GetFocus(void)
{
    HWND hWndFocus = oGetFocus();
    if (g_hFocusWindow && hWndFocus == NULL && IsWindow(g_hFocusWindow)) {
        if (GetCurrentThreadId() == GetWindowThreadProcessId(g_hFocusWindow, NULL)) return g_hFocusWindow;
    }
    return hWndFocus;
}

// ============================================================================
// Kernel32 loader hooks -> also trigger Bink patching for new modules
// ============================================================================

typedef HMODULE(__stdcall* LoadLibraryA_fn)(LPCSTR lpLibFileName);
typedef HMODULE(__stdcall* LoadLibraryW_fn)(LPCWSTR lpLibFileName);
typedef HMODULE(__stdcall* LoadLibraryExA_fn)(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
typedef HMODULE(__stdcall* LoadLibraryExW_fn)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
typedef BOOL(__stdcall* FreeLibrary_fn)(HMODULE hLibModule);

LoadLibraryA_fn  oLoadLibraryA = nullptr;
LoadLibraryW_fn  oLoadLibraryW = nullptr;
LoadLibraryExA_fn oLoadLibraryExA = nullptr;
LoadLibraryExW_fn oLoadLibraryExW = nullptr;
FreeLibrary_fn    oFreeLibrary = nullptr;

void HookModule(HMODULE hmod); // forward

// ---- Forward declarations for loader hooks (needed before use)
HMODULE __stdcall hk_LoadLibraryA(LPCSTR lpLibFileName);
HMODULE __stdcall hk_LoadLibraryW(LPCWSTR lpLibFileName);
HMODULE __stdcall hk_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
HMODULE __stdcall hk_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
BOOL    __stdcall hk_FreeLibrary(HMODULE hLibModule);

// NEW: loaders-only variant (used for unconditional Bink readiness)
static void HookModuleLoadersOnly(HMODULE hmod)
{
    char modpath[MAX_PATH + 1]{};
    if (!hmod || hmod == g_hWrapperModule) return;
    if (GetModuleFileNameA(hmod, modpath, MAX_PATH)) {
        if (!_strnicmp(modpath, WinDir, strlen(WinDir))) return; // skip system
    }

    if (!oLoadLibraryA)  oLoadLibraryA = (LoadLibraryA_fn)Iat_hook::detour_iat_ptr("LoadLibraryA", (void*)hk_LoadLibraryA, hmod);
    else                 Iat_hook::detour_iat_ptr("LoadLibraryA", (void*)hk_LoadLibraryA, hmod);

    if (!oLoadLibraryW)  oLoadLibraryW = (LoadLibraryW_fn)Iat_hook::detour_iat_ptr("LoadLibraryW", (void*)hk_LoadLibraryW, hmod);
    else                 Iat_hook::detour_iat_ptr("LoadLibraryW", (void*)hk_LoadLibraryW, hmod);

    if (!oLoadLibraryExA)oLoadLibraryExA = (LoadLibraryExA_fn)Iat_hook::detour_iat_ptr("LoadLibraryExA", (void*)hk_LoadLibraryExA, hmod);
    else                 Iat_hook::detour_iat_ptr("LoadLibraryExA", (void*)hk_LoadLibraryExA, hmod);

    if (!oLoadLibraryExW)oLoadLibraryExW = (LoadLibraryExW_fn)Iat_hook::detour_iat_ptr("LoadLibraryExW", (void*)hk_LoadLibraryExW, hmod);
    else                 Iat_hook::detour_iat_ptr("LoadLibraryExW", (void*)hk_LoadLibraryExW, hmod);

    if (!oFreeLibrary)   oFreeLibrary = (FreeLibrary_fn)Iat_hook::detour_iat_ptr("FreeLibrary", (void*)hk_FreeLibrary, hmod);
    else                 Iat_hook::detour_iat_ptr("FreeLibrary", (void*)hk_FreeLibrary, hmod);
}

static void HookImportedModules_LoadersOnly()
{
    HMODULE hModule = GetModuleHandle(0);
    if (!hModule) return;

    PIMAGE_DOS_HEADER img_dos_headers = (PIMAGE_DOS_HEADER)hModule;
    if (img_dos_headers->e_magic != IMAGE_DOS_SIGNATURE) return;

    PIMAGE_NT_HEADERS img_nt_headers = (PIMAGE_NT_HEADERS)((BYTE*)img_dos_headers + img_dos_headers->e_lfanew);
    PIMAGE_IMPORT_DESCRIPTOR img_import_desc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)img_dos_headers + img_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    for (IMAGE_IMPORT_DESCRIPTOR* iid = img_import_desc; iid->Name != 0; iid++) {
        char* mod_name = (char*)((size_t*)(iid->Name + (size_t)hModule));
        HMODULE hm = GetModuleHandleA(mod_name);
        if (hm) {
            HookModuleLoadersOnly(hm);
        }
    }
}

HMODULE __stdcall hk_LoadLibraryA(LPCSTR lpLibFileName)
{
    HMODULE hmod = oLoadLibraryA(lpLibFileName);
    if (hmod) {
        HookModule(hmod);                 // may do extra hooks (only if enabled)
        HookModuleLoadersOnly(hmod);      // always keep loaders hooked
        BnkPatchImportsInOne(hmod);
        if (lpLibFileName) {
            const char* base = BnkBaseNameA(lpLibFileName);
            if (_stricmp(base, "binkw32.dll") == 0) {
                std::cout << "[BINK] binkw32.dll loaded (A) -> patch all callers" << std::endl;
                BnkPatchAllModules();
            }
        }
    }
    return hmod;
}

HMODULE __stdcall hk_LoadLibraryW(LPCWSTR lpLibFileName)
{
    HMODULE hmod = oLoadLibraryW(lpLibFileName);
    if (hmod) {
        HookModule(hmod);
        HookModuleLoadersOnly(hmod);
        BnkPatchImportsInOne(hmod);
        if (lpLibFileName) {
            char ansi[MAX_PATH]{};
            WideCharToMultiByte(CP_ACP, 0, lpLibFileName, -1, ansi, MAX_PATH, NULL, NULL);
            const char* base = BnkBaseNameA(ansi);
            if (_stricmp(base, "binkw32.dll") == 0) {
                std::cout << "[BINK] binkw32.dll loaded (W) -> patch all callers" << std::endl;
                BnkPatchAllModules();
            }
        }
    }
    return hmod;
}

HMODULE __stdcall hk_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    HMODULE hmod = oLoadLibraryExA(lpLibFileName, hFile, dwFlags);
    if (hmod && ((dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)) == 0)) {
        HookModule(hmod);
        HookModuleLoadersOnly(hmod);
        BnkPatchImportsInOne(hmod);
        if (lpLibFileName) {
            const char* base = BnkBaseNameA(lpLibFileName);
            if (_stricmp(base, "binkw32.dll") == 0) {
                std::cout << "[BINK] binkw32.dll loaded (ExA) -> patch all callers" << std::endl;
                BnkPatchAllModules();
            }
        }
    }
    return hmod;
}

HMODULE __stdcall hk_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    HMODULE hmod = oLoadLibraryExW(lpLibFileName, hFile, dwFlags);
    if (hmod && ((dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)) == 0)) {
        HookModule(hmod);
        HookModuleLoadersOnly(hmod);
        BnkPatchImportsInOne(hmod);
        if (lpLibFileName) {
            char ansi[MAX_PATH]{};
            WideCharToMultiByte(CP_ACP, 0, lpLibFileName, -1, ansi, MAX_PATH, NULL, NULL);
            const char* base = BnkBaseNameA(ansi);
            if (_stricmp(base, "binkw32.dll") == 0) {
                std::cout << "[BINK] binkw32.dll loaded (ExW) -> patch all callers" << std::endl;
                BnkPatchAllModules();
            }
        }
    }
    return hmod;
}

BOOL __stdcall hk_FreeLibrary(HMODULE hLibModule)
{
    if (hLibModule == g_hWrapperModule) return TRUE; // stay resident
    return oFreeLibrary(hLibModule);
}

// Keep existing GetProcAddress trampoline for your window hooks + Bink
FARPROC __stdcall hk_GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    __try
    {
        // ---- Bink: also catch dynamic resolves (decorated or not)
        if (BnkNameMatches(lpProcName, "BinkCopyToBuffer"))
        {
            FARPROC real = GetProcAddress(hModule, lpProcName);
            if (real && !gReal_BinkCopyToBuffer) gReal_BinkCopyToBuffer = (PFN_BinkCopyToBuffer)real;

            return (FARPROC)hk_BinkCopyToBuffer;
        }
        if (BnkNameMatches(lpProcName, "BinkCopyToBufferRect"))
        {
            FARPROC real = GetProcAddress(hModule, lpProcName);
            if (real && !gReal_BinkCopyToBufferRect) gReal_BinkCopyToBufferRect = (PFN_BinkCopyToBufferRect)real;

            return (FARPROC)hk_BinkCopyToBufferRect;
        }

        // Ordinal requests: try to detect if they point at BinkCopyToBuffer(_Rect)
        if (HIWORD(lpProcName) == 0)
        {
            FARPROC real = GetProcAddress(hModule, lpProcName);
            if (real)
            {
                FARPROC ctb1 = GetProcAddress(hModule, "BinkCopyToBuffer");
                FARPROC ctb2 = GetProcAddress(hModule, "BinkCopyToBuffer@24");
                FARPROC rect1 = GetProcAddress(hModule, "BinkCopyToBufferRect");
                FARPROC rect2 = GetProcAddress(hModule, "BinkCopyToBufferRect@44");
                if (real == ctb1 || real == ctb2)
                {
                    if (!gReal_BinkCopyToBuffer) gReal_BinkCopyToBuffer = (PFN_BinkCopyToBuffer)real;

                    return (FARPROC)hk_BinkCopyToBuffer;
                }
                if (real == rect1 || real == rect2)
                {
                    if (!gReal_BinkCopyToBufferRect) gReal_BinkCopyToBufferRect = (PFN_BinkCopyToBufferRect)real;

                    return (FARPROC)hk_BinkCopyToBufferRect;
                }
                return real; // some other ordinal
            }
        }

        // ---- your existing window/loader forwards
        if (!lstrcmpA(lpProcName, "RegisterClassA")) { if (!oRegisterClassA)  oRegisterClassA = (RegisterClassA_fn)GetProcAddress(hModule, lpProcName);  return (FARPROC)hk_RegisterClassA; }
        if (!lstrcmpA(lpProcName, "RegisterClassW")) { if (!oRegisterClassW)  oRegisterClassW = (RegisterClassW_fn)GetProcAddress(hModule, lpProcName);  return (FARPROC)hk_RegisterClassW; }
        if (!lstrcmpA(lpProcName, "RegisterClassExA")) { if (!oRegisterClassExA)oRegisterClassExA = (RegisterClassExA_fn)GetProcAddress(hModule, lpProcName);  return (FARPROC)hk_RegisterClassExA; }
        if (!lstrcmpA(lpProcName, "RegisterClassExW")) { if (!oRegisterClassExW)oRegisterClassExW = (RegisterClassExW_fn)GetProcAddress(hModule, lpProcName);  return (FARPROC)hk_RegisterClassExW; }
        if (!lstrcmpA(lpProcName, "GetForegroundWindow")) { if (!oGetForegroundWindow) oGetForegroundWindow = (GetForegroundWindow_fn)GetProcAddress(hModule, lpProcName); return (FARPROC)hk_GetForegroundWindow; }
        if (!lstrcmpA(lpProcName, "GetActiveWindow")) { if (!oGetActiveWindow)  oGetActiveWindow = (GetActiveWindow_fn)GetProcAddress(hModule, lpProcName); return (FARPROC)hk_GetActiveWindow; }
        if (!lstrcmpA(lpProcName, "GetFocus")) { if (!oGetFocus)         oGetFocus = (GetFocus_fn)GetProcAddress(hModule, lpProcName); return (FARPROC)hk_GetFocus; }
        if (!lstrcmpA(lpProcName, "LoadLibraryA")) { if (!oLoadLibraryA)     oLoadLibraryA = (LoadLibraryA_fn)GetProcAddress(hModule, lpProcName); return (FARPROC)hk_LoadLibraryA; }
        if (!lstrcmpA(lpProcName, "LoadLibraryW")) { if (!oLoadLibraryW)     oLoadLibraryW = (LoadLibraryW_fn)GetProcAddress(hModule, lpProcName); return (FARPROC)hk_LoadLibraryW; }
        if (!lstrcmpA(lpProcName, "LoadLibraryExA")) { if (!oLoadLibraryExA)   oLoadLibraryExA = (LoadLibraryExA_fn)GetProcAddress(hModule, lpProcName); return (FARPROC)hk_LoadLibraryExA; }
        if (!lstrcmpA(lpProcName, "LoadLibraryExW")) { if (!oLoadLibraryExW)   oLoadLibraryExW = (LoadLibraryExW_fn)GetProcAddress(hModule, lpProcName); return (FARPROC)hk_LoadLibraryExW; }
        if (!lstrcmpA(lpProcName, "FreeLibrary")) { if (!oFreeLibrary)      oFreeLibrary = (FreeLibrary_fn)GetProcAddress(hModule, lpProcName); return (FARPROC)hk_FreeLibrary; }
    }
    __except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
    }

    return GetProcAddress(hModule, lpProcName);
}

void HookModule(HMODULE hmod)
{
    char modpath[MAX_PATH + 1]{};
    if (hmod == g_hWrapperModule) return;
    if (GetModuleFileNameA(hmod, modpath, MAX_PATH)) {
        if (!_strnicmp(modpath, WinDir, strlen(WinDir))) return; // skip system
    }

    // NOTE: these will only be detoured if GetProcAddress is also hooked or the module imports them.
    if (!oRegisterClassA)  oRegisterClassA = (RegisterClassA_fn)Iat_hook::detour_iat_ptr("RegisterClassA", (void*)hk_RegisterClassA, hmod);
    else                   Iat_hook::detour_iat_ptr("RegisterClassA", (void*)hk_RegisterClassA, hmod);

    if (!oRegisterClassW)  oRegisterClassW = (RegisterClassW_fn)Iat_hook::detour_iat_ptr("RegisterClassW", (void*)hk_RegisterClassW, hmod);
    else                   Iat_hook::detour_iat_ptr("RegisterClassW", (void*)hk_RegisterClassW, hmod);

    if (!oRegisterClassExA)oRegisterClassExA = (RegisterClassExA_fn)Iat_hook::detour_iat_ptr("RegisterClassExA", (void*)hk_RegisterClassExA, hmod);
    else                   Iat_hook::detour_iat_ptr("RegisterClassExA", (void*)hk_RegisterClassExA, hmod);

    if (!oRegisterClassExW)oRegisterClassExW = (RegisterClassExW_fn)Iat_hook::detour_iat_ptr("RegisterClassExW", (void*)hk_RegisterClassExW, hmod);
    else                   Iat_hook::detour_iat_ptr("RegisterClassExW", (void*)hk_RegisterClassExW, hmod);

    if (!oGetForegroundWindow) oGetForegroundWindow = (GetForegroundWindow_fn)Iat_hook::detour_iat_ptr("GetForegroundWindow", (void*)hk_GetForegroundWindow, hmod);
    else                       Iat_hook::detour_iat_ptr("GetForegroundWindow", (void*)hk_GetForegroundWindow, hmod);

    if (!oGetActiveWindow) oGetActiveWindow = (GetActiveWindow_fn)Iat_hook::detour_iat_ptr("GetActiveWindow", (void*)hk_GetActiveWindow, hmod);
    else                   Iat_hook::detour_iat_ptr("GetActiveWindow", (void*)hk_GetActiveWindow, hmod);

    if (!oGetFocus) oGetFocus = (GetFocus_fn)Iat_hook::detour_iat_ptr("GetFocus", (void*)hk_GetFocus, hmod);
    else            Iat_hook::detour_iat_ptr("GetFocus", (void*)hk_GetFocus, hmod);

    if (!oLoadLibraryA)  oLoadLibraryA = (LoadLibraryA_fn)Iat_hook::detour_iat_ptr("LoadLibraryA", (void*)hk_LoadLibraryA, hmod);
    else                 Iat_hook::detour_iat_ptr("LoadLibraryA", (void*)hk_LoadLibraryA, hmod);

    if (!oLoadLibraryW)  oLoadLibraryW = (LoadLibraryW_fn)Iat_hook::detour_iat_ptr("LoadLibraryW", (void*)hk_LoadLibraryW, hmod);
    else                 Iat_hook::detour_iat_ptr("LoadLibraryW", (void*)hk_LoadLibraryW, hmod);

    if (!oLoadLibraryExA) oLoadLibraryExA = (LoadLibraryExA_fn)Iat_hook::detour_iat_ptr("LoadLibraryExA", (void*)hk_LoadLibraryExA, hmod);
    else                  Iat_hook::detour_iat_ptr("LoadLibraryExA", (void*)hk_LoadLibraryExA, hmod);

    if (!oLoadLibraryExW) oLoadLibraryExW = (LoadLibraryExW_fn)Iat_hook::detour_iat_ptr("LoadLibraryExW", (void*)hk_LoadLibraryExW, hmod);
    else                  Iat_hook::detour_iat_ptr("LoadLibraryExW", (void*)hk_LoadLibraryExW, hmod);

    if (!oFreeLibrary) oFreeLibrary = (FreeLibrary_fn)Iat_hook::detour_iat_ptr("FreeLibrary", (void*)hk_FreeLibrary, hmod);
    else               Iat_hook::detour_iat_ptr("FreeLibrary", (void*)hk_FreeLibrary, hmod);

    Iat_hook::detour_iat_ptr("GetProcAddress", (void*)hk_GetProcAddress, hmod);
}

void HookImportedModules()
{
    HMODULE hModule = GetModuleHandle(0);
    if (!hModule) return;

    PIMAGE_DOS_HEADER img_dos_headers = (PIMAGE_DOS_HEADER)hModule;
    if (img_dos_headers->e_magic != IMAGE_DOS_SIGNATURE) return;

    PIMAGE_NT_HEADERS img_nt_headers = (PIMAGE_NT_HEADERS)((BYTE*)img_dos_headers + img_dos_headers->e_lfanew);
    PIMAGE_IMPORT_DESCRIPTOR img_import_desc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)img_dos_headers + img_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    for (IMAGE_IMPORT_DESCRIPTOR* iid = img_import_desc; iid->Name != 0; iid++) {
        char* mod_name = (char*)((size_t*)(iid->Name + (size_t)hModule));
        HMODULE hm = GetModuleHandleA(mod_name);
        if (hm && !(GetProcAddress(hm, "DirectInput8Create") != NULL && GetProcAddress(hm, "DirectSoundCreate8") != NULL && GetProcAddress(hm, "InternetOpenA") != NULL)) {
            HookModule(hm);
        }
    }
}

// ============================================================================
// Frame limiter (unchanged)
// ============================================================================

class FrameLimiter
{
private:
    static inline double TIME_Frequency = 0.0;
    static inline double TIME_Ticks = 0.0;
    static inline double TIME_Frametime = 0.0;

public:
    static inline ID3DXFont* pFPSFont = nullptr;
    static inline ID3DXFont* pTimeFont = nullptr;

public:
    enum FPSLimitMode { FPS_NONE, FPS_REALTIME, FPS_ACCURATE };
    static void Init(FPSLimitMode mode)
    {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        static constexpr auto TICKS_PER_FRAME = 1;
        auto TICKS_PER_SECOND = (TICKS_PER_FRAME * fFPSLimit);
        if (mode == FPS_ACCURATE) {
            TIME_Frametime = 1000.0 / (double)fFPSLimit;
            TIME_Frequency = (double)frequency.QuadPart / 1000.0;
        }
        else {
            TIME_Frequency = (double)frequency.QuadPart / (double)TICKS_PER_SECOND;
        }
        Ticks();
        DX_PRINT("[FPS] Init mode=" << (mode == FPS_ACCURATE ? "ACCURATE" : "REALTIME") << " limit=" << fFPSLimit);
    }
    static DWORD Sync_RT()
    {
        DWORD lastTicks, currentTicks;
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        lastTicks = (DWORD)TIME_Ticks;
        TIME_Ticks = (double)counter.QuadPart / TIME_Frequency;
        currentTicks = (DWORD)TIME_Ticks;
        return (currentTicks > lastTicks) ? currentTicks - lastTicks : 0;
    }
    static DWORD Sync_SLP()
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        double millis_current = (double)counter.QuadPart / TIME_Frequency;
        double millis_delta = millis_current - TIME_Ticks;
        if (TIME_Frametime <= millis_delta) {
            TIME_Ticks = millis_current;
            return 1;
        }
        else if (TIME_Frametime - millis_delta > 2.0)
            Sleep(1);
        else
            Sleep(0);
        return 0;
    }
    static void ShowFPS(LPDIRECT3DDEVICE8 device)
    {
        static std::list<int> m_times;
        LARGE_INTEGER frequency, time;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&time);
        if (m_times.size() == 50) m_times.pop_front();
        m_times.push_back(static_cast<int>(time.QuadPart));

        uint32_t fps = 0;
        if (m_times.size() >= 2)
            fps = static_cast<uint32_t>(0.5f + (static_cast<float>(m_times.size() - 1) * static_cast<float>(frequency.QuadPart)) / static_cast<float>(m_times.back() - m_times.front()));

        static int space = 0;
        if (!pFPSFont || !pTimeFont)
        {
            D3DDEVICE_CREATION_PARAMETERS cparams;
            RECT rect;
            device->GetCreationParameters(&cparams);
            GetClientRect(cparams.hFocusWindow, &rect);

            LOGFONT fps_font = { rect.bottom / 20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Arial" };
            LOGFONT time_font = { rect.bottom / 35, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Arial" };
            space = rect.bottom / 20 + 5;

            if (D3DXCreateFontIndirect(device, &fps_font, &pFPSFont) != D3D_OK) return;
            if (D3DXCreateFontIndirect(device, &time_font, &pTimeFont) != D3D_OK) return;
        }
        else
        {
            auto DrawTextOutline = [](ID3DXFont* pFont, FLOAT X, FLOAT Y, D3DXCOLOR dColor, CONST PCHAR cString, ...)
                {
                    const D3DXCOLOR BLACK(D3DCOLOR_XRGB(0, 0, 0));
                    CHAR cBuffer[101] = "";
                    va_list oArgs;
                    va_start(oArgs, cString);
                    _vsnprintf((cBuffer + strlen(cBuffer)), (sizeof(cBuffer) - strlen(cBuffer)), cString, oArgs);
                    va_end(oArgs);

                    RECT Rect[5] =
                    {
                        { LONG(X - 1), LONG(Y),     LONG(X + 500.0f), LONG(Y + 50.0f) },
                        { LONG(X),     LONG(Y - 1), LONG(X + 500.0f), LONG(Y + 50.0f) },
                        { LONG(X + 1), LONG(Y),     LONG(X + 500.0f), LONG(Y + 50.0f) },
                        { LONG(X),     LONG(Y + 1), LONG(X + 500.0f), LONG(Y + 50.0f) },
                        { LONG(X),     LONG(Y),     LONG(X + 500.0f), LONG(Y + 50.0f) },
                    };

                    pFont->Begin();
                    if (dColor != BLACK) {
                        for (auto i = 0; i < 4; i++)
                            pFont->DrawText(cBuffer, -1, &Rect[i], DT_NOCLIP, BLACK);
                    }
                    pFont->DrawText(cBuffer, -1, &Rect[4], DT_NOCLIP, dColor);
                    pFont->End();
                };

            static char str_format_fps[] = "%02d";
            static char str_format_time[] = "%.01f ms";
            static const D3DXCOLOR YELLOW(D3DCOLOR_XRGB(0xF7, 0xF7, 0));
            DrawTextOutline(pFPSFont, 10, 10, YELLOW, str_format_fps, (int)fps);
            DrawTextOutline(pTimeFont, 10, float(space), YELLOW, str_format_time, (fps ? (1.0f / fps) * 1000.0f : 0.0f));
        }
    }

private:
    static void Ticks()
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        TIME_Ticks = (double)counter.QuadPart / TIME_Frequency;
    }
};

FrameLimiter::FPSLimitMode mFPSLimitMode = FrameLimiter::FPSLimitMode::FPS_NONE;

// in the big cpp (where your FPS limiter lives)
extern UINT gOverlayActiveFrames;

HRESULT m_IDirect3DDevice8::Present(const RECT* pSourceRect,
    const RECT* pDestRect,
    HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion)
{
    const RECT* src = pSourceRect;
    const RECT* dest = pDestRect;
    HWND        hwnd = hDestWindowOverride;
    const RGNDATA* dirty = pDirtyRegion;

    if (gFmvHookEnabled && gOverlayActiveFrames > 0)
    {
        IDirect3DSurface8* bb = nullptr;
        if (SUCCEEDED(ProxyInterface->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb)
        {
            BinkOverlay_DrawToSurface(ProxyInterface, bb);

            if (gFmvDebug) {
                static int count = 0;
                char buf[96];
                _snprintf(buf, sizeof(buf), "[D3D8] Present overlay draw #%d (N=%d)\n", ++count, gUploadEveryN);
                OutputDebugStringA(buf);
            }

            bb->Release();
            gOverlayDrawnThisFrame = true;
        }

        if (gForceFullWindowPresent) {
            src = nullptr; dest = nullptr; hwnd = nullptr; dirty = nullptr;
        }
    }

    return ProxyInterface->Present(src, dest, hwnd, dirty);
}



HRESULT m_IDirect3DDevice8::EndScene()
{
    if (bDisplayFPSCounter)
        FrameLimiter::ShowFPS(ProxyInterface);

    return ProxyInterface->EndScene();
}

HRESULT m_IDirect3DSwapChain8::Present(
    const RECT* pSourceRect,
    const RECT* pDestRect,
    HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion)
{
    const RECT* src = pSourceRect;
    const RECT* dest = pDestRect;
    HWND        hwnd = hDestWindowOverride;
    const RGNDATA* dirty = pDirtyRegion;

    if (gOverlayActiveFrames > 0)
    {
        IDirect3DSurface8* bb = nullptr;
        if (SUCCEEDED(ProxyInterface->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb)
        {
            // pass the REAL D3D8 device to the overlay
            BinkOverlay_DrawToSurface(m_pDevice->GetProxyInterface(), bb);
            bb->Release();
            gOverlayDrawnThisFrame = true;
        }

        // force a full-window flip to kill the tiny box
        src = nullptr; dest = nullptr; hwnd = nullptr; dirty = nullptr;
    }
    else if (src)
    {
        const LONG w = src->right - src->left;
        const LONG h = src->bottom - src->top;
        if (w > 0 && h > 0 && (w <= 800 || h <= 600))
        {
            src = nullptr; dest = nullptr; hwnd = nullptr; dirty = nullptr;
        }
    }

    return ProxyInterface->Present(src, dest, hwnd, dirty);
}

// ============================================================================
// Windowing helpers
// ============================================================================

void ForceWindowed(D3DPRESENT_PARAMETERS* p)
{
    HWND hwnd = p->hDeviceWindow ? p->hDeviceWindow : g_hFocusWindow;
    HMONITOR monitor = MonitorFromWindow((!bUsePrimaryMonitor && hwnd) ? hwnd : GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO info; info.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(monitor, &info);
    int DesktopResX = info.rcMonitor.right - info.rcMonitor.left;
    int DesktopResY = info.rcMonitor.bottom - info.rcMonitor.top;

    int left = (int)info.rcMonitor.left;
    int top = (int)info.rcMonitor.top;

    if (!bBorderlessFullscreen) {
        left += (int)(((float)DesktopResX / 2.0f) - ((float)p->BackBufferWidth / 2.0f));
        top += (int)(((float)DesktopResY / 2.0f) - ((float)p->BackBufferHeight / 2.0f));
    }

    p->Windowed = 1;
    p->FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    p->FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    if (hwnd)
    {
        UINT uFlags = SWP_SHOWWINDOW;
        if (bBorderlessFullscreen)
        {
            LONG lOldStyle = GetWindowLong(hwnd, GWL_STYLE);
            LONG lOldExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            LONG lNewStyle = lOldStyle & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_DLGFRAME);
            lNewStyle |= (lOldStyle & WS_CHILD) ? 0 : WS_POPUP;
            LONG lNewExStyle = lOldExStyle & ~(WS_EX_CONTEXTHELP | WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW);
            lNewExStyle |= WS_EX_APPWINDOW;

            if (lNewStyle != lOldStyle) { SetWindowLong(hwnd, GWL_STYLE, lNewStyle);   uFlags |= SWP_FRAMECHANGED; }
            if (lNewExStyle != lOldExStyle) { SetWindowLong(hwnd, GWL_EXSTYLE, lNewExStyle); uFlags |= SWP_FRAMECHANGED; }
            SetWindowPos(hwnd, bAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, left, top, DesktopResX, DesktopResY, uFlags);
        }
        else
        {
            if (!bCenterWindow) uFlags |= SWP_NOMOVE;
            SetWindowPos(hwnd, bAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, left, top, p->BackBufferWidth, p->BackBufferHeight, uFlags);
        }
    }
}

void ForceFullScreenRefreshRateInHz(D3DPRESENT_PARAMETERS* p)
{
    if (!p->Windowed)
    {
        std::vector<int> list;
        DISPLAY_DEVICE dd; dd.cb = sizeof(DISPLAY_DEVICE);
        for (DWORD deviceNum = 0; EnumDisplayDevices(NULL, deviceNum, &dd, 0); ++deviceNum)
        {
            DISPLAY_DEVICE newdd = {}; newdd.cb = sizeof(DISPLAY_DEVICE);
            for (DWORD monitorNum = 0; EnumDisplayDevices(dd.DeviceName, monitorNum, &newdd, 0); ++monitorNum)
            {
                DEVMODE dm = {};
                for (auto iModeNum = 0; EnumDisplaySettings(NULL, iModeNum, &dm) != 0; iModeNum++)
                    list.emplace_back(dm.dmDisplayFrequency);
            }
        }
        std::sort(list.begin(), list.end());
        if (list.empty()) return;
        if (nFullScreenRefreshRateInHz > list.back() || nFullScreenRefreshRateInHz < list.front() || nFullScreenRefreshRateInHz < 0)
            p->FullScreen_RefreshRateInHz = list.back();
        else
            p->FullScreen_RefreshRateInHz = nFullScreenRefreshRateInHz;
    }
}

// ============================================================================
// D3D wrappers
// ============================================================================

HRESULT m_IDirect3D8::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* p, IDirect3DDevice8** ppReturnedDeviceInterface)
{
    g_hFocusWindow = hFocusWindow ? hFocusWindow : p->hDeviceWindow;
    if (bForceWindowedMode) ForceWindowed(p);
    if (nFullScreenRefreshRateInHz) ForceFullScreenRefreshRateInHz(p);

    if (bDisplayFPSCounter)
    {
        if (FrameLimiter::pFPSFont)  FrameLimiter::pFPSFont->Release();
        if (FrameLimiter::pTimeFont) FrameLimiter::pTimeFont->Release();
        FrameLimiter::pFPSFont = nullptr;
        FrameLimiter::pTimeFont = nullptr;
    }

    gTargetW = p->BackBufferWidth;
    gTargetH = p->BackBufferHeight;

    HRESULT hr = ProxyInterface->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, p, ppReturnedDeviceInterface);
    if (SUCCEEDED(hr) && ppReturnedDeviceInterface)
        *ppReturnedDeviceInterface = new m_IDirect3DDevice8(*ppReturnedDeviceInterface, this);
    return hr;
}

HRESULT m_IDirect3DDevice8::Reset(D3DPRESENT_PARAMETERS* p)
{
    // --- FMV overlay: release default-pool resources and clear stabilizer ---
    if (gOverlayTex) { gOverlayTex->Release(); gOverlayTex = nullptr; gOverlayTexW = gOverlayTexH = 0; }
    gOverlayFrame.clear();
    gOverlayDirty = false;
    gOverlayActiveFrames = 0;
    gCropLocked = false;
    gHasRectHint = false;
    gLockSrcW = gLockSrcH = 0;
    gStable = {}; gPrevStable = {}; gDetected = {};

    if (bForceWindowedMode) ForceWindowed(p);
    gTargetW = p->BackBufferWidth;
    gTargetH = p->BackBufferHeight;

    if (nFullScreenRefreshRateInHz) ForceFullScreenRefreshRateInHz(p);

    if (bDisplayFPSCounter)
    {
        if (FrameLimiter::pFPSFont)  FrameLimiter::pFPSFont->Release();
        if (FrameLimiter::pTimeFont) FrameLimiter::pTimeFont->Release();
        FrameLimiter::pFPSFont = nullptr;
        FrameLimiter::pTimeFont = nullptr;
    }

    return ProxyInterface->Reset(p);
}



// ============================================================================
// DllMain
// ============================================================================

bool WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID)
{
    static HMODULE sD3D8 = nullptr;

    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        g_hWrapperModule = hModule;
        DX_PRINT("[INIT] DLL_PROCESS_ATTACH");

        // System dir for skip checks
        GetSystemWindowsDirectoryA(WinDir, MAX_PATH);

        // Load real d3d8.dll
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);
        strcat_s(path, "\\d3d8.dll");
        sD3D8 = LoadLibraryA(path);
        d3d8dll = sD3D8;
        if (!sD3D8) { DX_MBERROR("Failed to load system d3d8.dll"); return false; }
        DX_PRINT("[INIT] Loaded system d3d8.dll from: " << path);

        // Perform your existing hex edits (optional)
        PerformHexEdits7();
        PerformHexEdits();
        PerformHexEdits3();
        CharSwap::Init();

        // Get function addresses
        m_pDirect3D8EnableMaximizedWindowedModeShim = (Direct3D8EnableMaximizedWindowedModeShimProc)GetProcAddress(sD3D8, "Direct3D8EnableMaximizedWindowedModeShim");
        m_pValidatePixelShader = (ValidatePixelShaderProc)GetProcAddress(sD3D8, "ValidatePixelShader");
        m_pValidateVertexShader = (ValidateVertexShaderProc)GetProcAddress(sD3D8, "ValidateVertexShader");
        m_pDebugSetMute = (DebugSetMuteProc)GetProcAddress(sD3D8, "DebugSetMute");
        m_pDirect3DCreate8 = (Direct3DCreate8Proc)GetProcAddress(sD3D8, "Direct3DCreate8");

        // INI
        HMODULE hm = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Direct3DCreate8, &hm);
        GetModuleFileNameA(hm, path, sizeof(path));
        strcpy(strrchr(path, '\\'), "\\d3d8.ini");
        DX_PRINT("[INI] Path: " << path);

        bForceWindowedMode = 1;
        bDirect3D8DisableMaximizedWindowedModeShim = 0;
        fFPSLimit = (float)GetPrivateProfileIntA("MAIN", "FPSLimit", 0, path);
        nFullScreenRefreshRateInHz = GetPrivateProfileIntA("MAIN", "FullScreenRefreshRateInHz", 0, path);
        bDisplayFPSCounter = GetPrivateProfileIntA("MAIN", "DisplayFPSCounter", 0, path);
        bUsePrimaryMonitor = GetPrivateProfileIntA("FORCEWINDOWED", "UsePrimaryMonitor", 0, path) != 0;
        bCenterWindow = GetPrivateProfileIntA("FORCEWINDOWED", "CenterWindow", 1, path) != 0;
        bBorderlessFullscreen = 1;
        bAlwaysOnTop = GetPrivateProfileIntA("FORCEWINDOWED", "AlwaysOnTop", 0, path) != 0;
        bDoNotNotifyOnTaskSwitch = GetPrivateProfileIntA("FORCEWINDOWED", "DoNotNotifyOnTaskSwitch", 0, path) != 0;

        // --- FMV master toggle
        gFmvHookEnabled = (GetPrivateProfileIntA("FMV", "Hook", 1, path) != 0);

        // --- Read FMV settings (with defaults matching previous hard-coded behaviour)
        gCfgFmvStretch = (GetPrivateProfileIntA("FMV", "Stretch", gCfgFmvStretch ? 1 : 0, path) != 0);
        gCfgFmvKeepAspect = (GetPrivateProfileIntA("FMV", "KeepAspect", gCfgFmvKeepAspect ? 1 : 0, path) != 0);
        // gCfgFmvDetectCrop = (GetPrivateProfileIntA("FMV", "DetectCrop", gCfgFmvDetectCrop ? 1 : 0, path) != 0);
        gCfgFmvUploadEveryN = GetPrivateProfileIntA("FMV", "UploadEveryN", gCfgFmvUploadEveryN, path);
        // gCfgFmvForceFullWindowPresent = (GetPrivateProfileIntA("FMV", "ForceFullWindowPresent", gCfgFmvForceFullWindowPresent ? 1 : 0, path) != 0);
        // gCfgFmvDebug = (GetPrivateProfileIntA("FMV", "Debug", gCfgFmvDebug ? 1 : 0, path) != 0);

        // Clamp / sanitize
        if (gCfgFmvUploadEveryN < 1) gCfgFmvUploadEveryN = 1;

        // --- Read optional fixed movie size and offsets
        gMovieW = (unsigned)GetPrivateProfileIntA("MOVIE_SIZE", "Width", 0, path);
        gMovieH = (unsigned)GetPrivateProfileIntA("MOVIE_SIZE", "Height", 0, path);
        gCfgMovieOffsetX = GetPrivateProfileIntA("MOVIE_SIZE", "OffsetX", 0, path);
        gCfgMovieOffsetY = GetPrivateProfileIntA("MOVIE_SIZE", "OffsetY", 0, path);

        // NOTE: For compatibility, allow [MOVIE_SIZE] Stretch to force stretching as well.
        // If either [FMV].Stretch=1 or [MOVIE_SIZE].Stretch=1, we stretch.
        const bool movieSizeStretch = (GetPrivateProfileIntA("MOVIE_SIZE", "Stretch", 0, path) != 0);

        // --- Apply to runtime flags the overlay actually uses
        gMovieStretch = (gCfgFmvStretch || movieSizeStretch);
        gKeepAspect = gCfgFmvKeepAspect;
        gDetectCrop = kHardDetectCrop;
        gForceFullWindowPresent = kHardForceFullWindowPresent;
        gUploadEveryN = gCfgFmvUploadEveryN;
        gFmvDebug = kHardDebug;
        gMovieOffsetX = gCfgMovieOffsetX;
        gMovieOffsetY = gCfgMovieOffsetY;

        // Optional: brief log
        {
            char buf[256];
            _snprintf(buf, sizeof(buf),
                "[INI][FMV] Hook=%d Stretch=%d KeepAspect=%d DetectCrop=%d UploadEveryN=%d ForceFullWindowPresent=%d Debug=%d | [MOVIE_SIZE] W=%u H=%u Off=(%d,%d) MS_Stretch=%d\n",
                gFmvHookEnabled ? 1 : 0, gMovieStretch ? 1 : 0, gKeepAspect ? 1 : 0, gDetectCrop ? 1 : 0,
                gUploadEveryN, gForceFullWindowPresent ? 1 : 0, gFmvDebug ? 1 : 0,
                gMovieW, gMovieH, gMovieOffsetX, gMovieOffsetY, movieSizeStretch ? 1 : 0);
            OutputDebugStringA(buf);
        }

        if (bDirect3D8DisableMaximizedWindowedModeShim)
        {
            auto addr = (uintptr_t)GetProcAddress(sD3D8, "Direct3D8EnableMaximizedWindowedModeShim");
            if (addr) {
                DWORD Protect;
                VirtualProtect((LPVOID)(addr + 6), 4, PAGE_EXECUTE_READWRITE, &Protect);
                *(unsigned*)(addr + 6) = 0;
                *(unsigned*)(*(unsigned*)(addr + 2)) = 0;
                VirtualProtect((LPVOID)(addr + 6), 4, Protect, &Protect);
                bForceWindowedMode = false;
            }
        }

        // ---------------- UNCONDITIONAL: keep loader hooks active everywhere
        HookModuleLoadersOnly(GetModuleHandle(NULL));  // the EXE
        HookModuleLoadersOnly(sD3D8);                  // real d3d8.dll
        HookImportedModules_LoadersOnly();             // all already-imported modules

        // ---------------- Optional window/activation hooks (respect INI)
        if (bDoNotNotifyOnTaskSwitch)
        {
            oRegisterClassA = (RegisterClassA_fn)Iat_hook::detour_iat_ptr("RegisterClassA", (void*)hk_RegisterClassA);
            oRegisterClassW = (RegisterClassW_fn)Iat_hook::detour_iat_ptr("RegisterClassW", (void*)hk_RegisterClassW);
            oRegisterClassExA = (RegisterClassExA_fn)Iat_hook::detour_iat_ptr("RegisterClassExA", (void*)hk_RegisterClassExA);
            oRegisterClassExW = (RegisterClassExW_fn)Iat_hook::detour_iat_ptr("RegisterClassExW", (void*)hk_RegisterClassExW);
            oGetForegroundWindow = (GetForegroundWindow_fn)Iat_hook::detour_iat_ptr("GetForegroundWindow", (void*)hk_GetForegroundWindow);
            oGetActiveWindow = (GetActiveWindow_fn)Iat_hook::detour_iat_ptr("GetActiveWindow", (void*)hk_GetActiveWindow);
            oGetFocus = (GetFocus_fn)Iat_hook::detour_iat_ptr("GetFocus", (void*)hk_GetFocus);

            // Also hook GetProcAddress to catch modules that resolve these dynamically
            Iat_hook::detour_iat_ptr("GetProcAddress", (void*)hk_GetProcAddress);
            Iat_hook::detour_iat_ptr("GetProcAddress", (void*)hk_GetProcAddress, sD3D8);

            if (!oGetForegroundWindow)
                oGetForegroundWindow = (GetForegroundWindow_fn)Iat_hook::detour_iat_ptr("GetForegroundWindow", (void*)hk_GetForegroundWindow, sD3D8);
            else Iat_hook::detour_iat_ptr("GetForegroundWindow", (void*)hk_GetForegroundWindow, sD3D8);

            HMODULE ole32 = GetModuleHandleA("ole32.dll");
            if (ole32) {
                if (!oRegisterClassA)   oRegisterClassA = (RegisterClassA_fn)Iat_hook::detour_iat_ptr("RegisterClassA", (void*)hk_RegisterClassA, ole32);
                else                    Iat_hook::detour_iat_ptr("RegisterClassA", (void*)hk_RegisterClassA, ole32);
                if (!oRegisterClassW)   oRegisterClassW = (RegisterClassW_fn)Iat_hook::detour_iat_ptr("RegisterClassW", (void*)hk_RegisterClassW, ole32);
                else                    Iat_hook::detour_iat_ptr("RegisterClassW", (void*)hk_RegisterClassW, ole32);
                if (!oRegisterClassExA) oRegisterClassExA = (RegisterClassExA_fn)Iat_hook::detour_iat_ptr("RegisterClassExA", (void*)hk_RegisterClassExA, ole32);
                else                    Iat_hook::detour_iat_ptr("RegisterClassExA", (void*)hk_RegisterClassExA, ole32);
                if (!oRegisterClassExW) oRegisterClassExW = (RegisterClassExW_fn)Iat_hook::detour_iat_ptr("RegisterClassExW", (void*)hk_RegisterClassExW, ole32);
                else                    Iat_hook::detour_iat_ptr("RegisterClassExW", (void*)hk_RegisterClassExW, ole32);
                if (!oGetActiveWindow)  oGetActiveWindow = (GetActiveWindow_fn)Iat_hook::detour_iat_ptr("GetActiveWindow", (void*)hk_GetActiveWindow, ole32);
                else                    Iat_hook::detour_iat_ptr("GetActiveWindow", (void*)hk_GetActiveWindow, ole32);
            }

            HookImportedModules();
        }

        // ----- BINK: if bink already present, patch all modules now
        if (GetModuleHandleA("binkw32.dll")) {
            std::cout << "[BINK] binkw32.dll already loaded -> patch all modules now" << std::endl;
            BnkPatchAllModules();
        }
        else {
            std::cout << "[BINK] binkw32.dll not loaded yet (we patch callers as it loads)" << std::endl;
        }

        break;
    }

    case DLL_PROCESS_DETACH:
    {
        DX_PRINT("[INIT] DLL_PROCESS_DETACH");
        if (mFPSLimitMode == FrameLimiter::FPSLimitMode::FPS_ACCURATE)
            timeEndPeriod(1);

        if (gOverlayTex) { gOverlayTex->Release(); gOverlayTex = nullptr; }
        if (sD3D8) FreeLibrary(sD3D8);

        CharSwap::Shutdown();

        break;
    }
    }

    return true;
}

// ============================================================================
// Exported forwards
// ============================================================================

int WINAPI Direct3D8EnableMaximizedWindowedModeShim(BOOL mEnable)
{
    if (!m_pDirect3D8EnableMaximizedWindowedModeShim) return E_FAIL;
    return m_pDirect3D8EnableMaximizedWindowedModeShim(mEnable);
}

HRESULT WINAPI ValidatePixelShader(DWORD* pixelshader, DWORD* reserved1, BOOL flag, DWORD* toto)
{
    if (!m_pValidatePixelShader) return E_FAIL;
    return m_pValidatePixelShader(pixelshader, reserved1, flag, toto);
}

HRESULT WINAPI ValidateVertexShader(DWORD* vertexshader, DWORD* reserved1, DWORD* reserved2, BOOL flag, DWORD* toto)
{
    if (!m_pValidateVertexShader) return E_FAIL;
    return m_pValidateVertexShader(vertexshader, reserved1, reserved2, flag, toto);
}

void WINAPI DebugSetMute()
{
    if (!m_pDebugSetMute) return;
    return m_pDebugSetMute();
}

IDirect3D8* WINAPI Direct3DCreate8(UINT SDKVersion)
{
    if (!m_pDirect3DCreate8) return nullptr;

    IDirect3D8* pD3D8 = m_pDirect3DCreate8(SDKVersion);
    if (pD3D8) return new m_IDirect3D8(pD3D8);
    return nullptr;
}
