#include "SystemDirect3D8.h"
#include <cstdio>
#include <cwchar>

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || !SetCurrentDirectoryW(argv[1])) return 1;
    if (GetFileAttributesW(L"d3d8.dll") == INVALID_FILE_ATTRIBUTES) return 2;
    HMODULE module = loadSystemDirect3D8();
    if (!module) { std::printf("Load error: %lu\n", GetLastError()); return 3; }
    wchar_t modulePath[MAX_PATH] = {};
    if (!GetModuleFileNameW(module, modulePath, MAX_PATH)) return 4;
    std::wprintf(L"Loaded: %ls\n", modulePath);
    wchar_t expectedPath[MAX_PATH] = {};
    // The loader can report the requested System32 spelling under WOW64;
    // Windows redirects this path to its 32-bit system DLL automatically.
    UINT length = GetSystemDirectoryW(expectedPath, MAX_PATH);
    if (!length || length >= MAX_PATH - 10) return 6;
    lstrcatW(expectedPath, L"\\d3d8.dll");
    if (_wcsicmp(modulePath, expectedPath) != 0) return 7;
    if (!GetProcAddress(module, "Direct3DCreate8")) return 8;
    FreeLibrary(module);
    std::puts("PASS: system Direct3D8 loaded despite the local proxy DLL");
    return 0;
}
