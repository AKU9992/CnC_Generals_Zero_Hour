#pragma once
#include <d3d9on12.h>
#include <cstdio>
#if defined(_WIN64)
void neuralBeforePresent(IDirect3DDevice9* device);
void neuralAfterPresent(IDirect3DDevice9* device);
#endif

// Transitional renderer: the legacy W3D interface is translated to D3D12.
// This is independent of the x64 Streamline/DLAA pass.
inline void bridgeLog(const char* message, HRESULT result = S_OK)
{
    FILE* log = nullptr;
    if (fopen_s(&log, "GeneralsD3D12.log", "a") == 0 && log)
    {
        fprintf(log, "PID %lu: %s: 0x%08lX\n", GetCurrentProcessId(), message,
            static_cast<unsigned long>(result));
        fclose(log);
    }
}

inline IDirect3D9* createD3D9On12()
{
    // Require 9On12 for every adapter; never silently fall back to native D3D9.
    D3D9ON12_ARGS options = {};
    options.Enable9On12 = TRUE;
    IDirect3D9* renderer = Direct3DCreate9On12(D3D_SDK_VERSION, &options, 1);
    bridgeLog("Direct3DCreate9On12", renderer ? S_OK : E_FAIL);
    return renderer;
}

inline HRESULT validateD3D12Device(IDirect3DDevice9* device)
{
    IDirect3DDevice9On12* interop = nullptr;
    HRESULT result = device->QueryInterface(IID_PPV_ARGS(&interop));
    ID3D12Device* nativeDevice = nullptr;
    if (SUCCEEDED(result))
    {
        result = interop->GetD3D12Device(IID_PPV_ARGS(&nativeDevice));
        interop->Release();
    }
    if (nativeDevice) nativeDevice->Release();
    bridgeLog("Actual ID3D12Device verified", result);
    return result;
}

inline HRESULT bridgePresent(IDirect3DDevice9* device, const RECT* source,
    const RECT* destination, HWND window)
{
#if defined(_WIN64)
    neuralBeforePresent(device);
#endif
    const HRESULT result = device->Present(source, destination, window, nullptr);
#if defined(_WIN64)
    neuralAfterPresent(device);
#endif
    // Log the first successful presentation, plus failures, without per-frame I/O.
    static bool recorded = false;
    if (!recorded && SUCCEEDED(result))
    {
        bridgeLog("First D3D12 presentation", result);
        recorded = true;
    }
    else if (FAILED(result) && result != D3DERR_DEVICELOST)
        bridgeLog("D3D12 presentation failed", result);
    return result;
}

inline HRESULT recordBridgeDraw(HRESULT result)
{
    static bool recordedSuccess = false, recordedFailure = false;
    if (SUCCEEDED(result) && !recordedSuccess)
    {
        bridgeLog("First successful D3D12 draw", result);
        recordedSuccess = true;
    }
    if (FAILED(result) && !recordedFailure)
    {
        bridgeLog("First failed D3D12 draw", result);
        recordedFailure = true;
    }
    return result;
}
