#pragma once
#include <windows.h>
#include <cwchar>

// The experimental executable has a different layout from retail game.dat.
// Retail-only proxy DLLs such as GenTool must not hook this executable.
inline HMODULE loadSystemDirect3D8()
{
    wchar_t renderer[16] = {};
    const DWORD rendererLength = GetEnvironmentVariableW(L"GENERALS_RENDERER", renderer, 16);
    if ((rendererLength == 5 && lstrcmpW(renderer, L"d3d12") == 0)
#if defined(_WIN64)
        || rendererLength == 0 // Windows has no native x64 Direct3D8 runtime.
#endif
        )
    {
        wchar_t modulePath[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        if (!length || length >= MAX_PATH) return nullptr;
        wchar_t* separator = wcsrchr(modulePath, L'\\');
        const wchar_t bridgeName[] = L"generals-d3d12.dll";
        if (!separator || separator - modulePath + 1 + sizeof(bridgeName) / sizeof(wchar_t) > MAX_PATH)
            return nullptr;
        lstrcpyW(separator + 1, bridgeName);
        // A requested DX12 renderer must fail explicitly if its DLL is missing.
        return LoadLibraryExW(modulePath, nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    wchar_t systemPath[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(systemPath, MAX_PATH);
    const wchar_t suffix[] = L"\\d3d8.dll";
    if (length == 0 || length >= MAX_PATH || length + sizeof(suffix) / sizeof(suffix[0]) > MAX_PATH)
        return nullptr;
    lstrcatW(systemPath, suffix);
    return LoadLibraryExW(systemPath, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
}
