#pragma once
#include <d3d9on12.h>
#include <cstdio>
#if defined(_WIN64)
void neuralBeforePresent(IDirect3DDevice9* device);
void neuralAfterPresent(IDirect3DDevice9* device);
HRESULT neuralNativePresent(IDirect3DDevice9* device);
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

inline void bridgeCaptureFrame(IDirect3DDevice9* device)
{
    char path[MAX_PATH] = {};
    if (!GetEnvironmentVariableA("GENERALS_CAPTURE_FRAME", path, MAX_PATH)) return;
    static DWORD lastCapture = GetTickCount();
    if (GetTickCount() - lastCapture < 5000) return;
    lastCapture = GetTickCount();
    IDirect3DSurface9 *back = nullptr, *copy = nullptr;
    D3DSURFACE_DESC desc = {};
    HRESULT result = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
    if (SUCCEEDED(result)) result = back->GetDesc(&desc);
    if (SUCCEEDED(result)) result = device->CreateOffscreenPlainSurface(desc.Width, desc.Height,
        desc.Format, D3DPOOL_SYSTEMMEM, &copy, nullptr);
    if (SUCCEEDED(result)) result = device->GetRenderTargetData(back, copy);
    D3DLOCKED_RECT pixels = {};
    if (SUCCEEDED(result)) result = copy->LockRect(&pixels, nullptr, D3DLOCK_READONLY);
    if (SUCCEEDED(result)) {
        FILE* file = nullptr;
        if (fopen_s(&file, path, "wb") == 0 && file) {
            BITMAPFILEHEADER header = {};
            BITMAPINFOHEADER info = {};
            header.bfType = 0x4d42;
            header.bfOffBits = sizeof(header) + sizeof(info);
            header.bfSize = header.bfOffBits + desc.Width * desc.Height * 4;
            info.biSize = sizeof(info); info.biWidth = desc.Width;
            info.biHeight = -static_cast<LONG>(desc.Height);
            info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
            fwrite(&header, sizeof(header), 1, file); fwrite(&info, sizeof(info), 1, file);
            for (unsigned y = 0; y < desc.Height; ++y)
                fwrite(static_cast<const char*>(pixels.pBits) + y * pixels.Pitch, 4, desc.Width, file);
            fclose(file);
        }
        copy->UnlockRect();
    }
    if (copy) copy->Release();
    if (back) back->Release();
    bridgeLog("Diagnostic backbuffer capture", result);
}

inline HRESULT bridgePresent(IDirect3DDevice9* device, const RECT* source,
    const RECT* destination, HWND window)
{
#if defined(_WIN64)
    neuralBeforePresent(device);
#endif
    bridgeCaptureFrame(device);
    HRESULT result;
#if defined(_WIN64)
    char nativeOutput[8]={};
    const bool native=GetEnvironmentVariableA("GENERALS_NATIVE_PRESENT",nativeOutput,sizeof(nativeOutput)) && nativeOutput[0]=='1';
    result=native ? neuralNativePresent(device):device->Present(source,destination,window,nullptr);
#else
    result=device->Present(source,destination,window,nullptr);
#endif
#if defined(_WIN64)
    neuralAfterPresent(device);
#endif
    // Log the first successful presentation, plus failures, without per-frame I/O.
    static bool recorded = false;
    if (!recorded && SUCCEEDED(result))
    {
        IDirect3DSwapChain9* chain = nullptr;
        D3DPRESENT_PARAMETERS params = {};
        if (SUCCEEDED(device->GetSwapChain(0, &chain))) {
            chain->GetPresentParameters(&params); chain->Release();
            RECT rect = {}; GetClientRect(params.hDeviceWindow, &rect);
            char details[256] = {};
            sprintf_s(details, "Presentation window=%p override=%p visible=%d client=%ldx%ld buffer=%ux%u windowed=%d source=%d destination=%d",
                params.hDeviceWindow, window, IsWindowVisible(params.hDeviceWindow),
                rect.right, rect.bottom, params.BackBufferWidth, params.BackBufferHeight,
                params.Windowed, source != nullptr, destination != nullptr);
            bridgeLog(details);
        }
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
