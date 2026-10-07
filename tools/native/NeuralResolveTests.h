#pragma once
#include "../../renderer/NeuralResolve.h"
#include <DirectXMath.h>
#include <cstdio>
#include <cstring>

inline bool testNeuralResolve(ID3D12Device* nativeDevice, HMODULE streamline, UINT width, UINT height, sl::DLSSMode mode) {
    using Microsoft::WRL::ComPtr;
    HWND window = CreateWindowExW(0, L"STATIC", L"DLSS resource resolve test", WS_OVERLAPPEDWINDOW, 0, 0, 256, 256, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return false;
    struct WindowOwner { HWND value; ~WindowOwner() { DestroyWindow(value); } } windowOwner{window};
    D3D9ON12_ARGS args{}; args.Enable9On12 = TRUE; args.pD3D12Device = nativeDevice;
    ComPtr<IDirect3D9> d3d; d3d.Attach(Direct3DCreate9On12(D3D_SDK_VERSION, &args, 1));
    if (!d3d) return false;
    D3DPRESENT_PARAMETERS pp{}; pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = width; pp.BackBufferHeight = height; pp.BackBufferFormat = D3DFMT_A8R8G8B8; pp.hDeviceWindow = window;
    ComPtr<IDirect3DDevice9> device;
    if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device))) return false;
    generals_mods::NeuralResolve resolve(streamline);
    HRESULT hr = resolve.initialize(device.Get(), width, height, mode);
    if (FAILED(hr)) { std::printf("Resolve initialize: %08lX\n", static_cast<unsigned long>(hr)); return false; }
    UINT rw = resolve.renderWidth(), rh = resolve.renderHeight();
    ComPtr<IDirect3DTexture9> colorTexture, motionTexture;
    ComPtr<IDirect3DSurface9> color, motion, depth, target;
    if (FAILED(device->CreateTexture(rw, rh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &colorTexture, nullptr)) ||
        FAILED(device->CreateTexture(rw, rh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_G16R16F, D3DPOOL_DEFAULT, &motionTexture, nullptr)) ||
        FAILED(colorTexture->GetSurfaceLevel(0, &color)) || FAILED(motionTexture->GetSurfaceLevel(0, &motion)) ||
        FAILED(device->CreateDepthStencilSurface(rw, rh, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &depth, nullptr)) ||
        FAILED(device->GetRenderTarget(0, &target))) return false;
    sl::Constants camera{};
    DirectX::XMFLOAT4X4 projection, inverse, identity;
    auto matrix = DirectX::XMMatrixPerspectiveFovLH(1.0f, float(width) / height, 0.1f, 1000.0f);
    DirectX::XMStoreFloat4x4(&projection, matrix);
    DirectX::XMStoreFloat4x4(&inverse, DirectX::XMMatrixInverse(nullptr, matrix));
    DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
    std::memcpy(&camera.cameraViewToClip, &projection, sizeof(projection));
    std::memcpy(&camera.clipToCameraView, &inverse, sizeof(inverse));
    std::memcpy(&camera.clipToPrevClip, &identity, sizeof(identity)); std::memcpy(&camera.prevClipToClip, &identity, sizeof(identity));
    camera.cameraPos = {0,0,0}; camera.cameraUp = {0,1,0}; camera.cameraRight = {1,0,0}; camera.cameraFwd = {0,0,1};
    camera.cameraNear = 0.1f; camera.cameraFar = 1000; camera.cameraFOV = 1; camera.cameraAspectRatio = float(width) / height;
    camera.mvecScale = {1,1}; camera.jitterOffset = {0,0}; camera.depthInverted = sl::Boolean::eFalse;
    camera.cameraMotionIncluded = sl::Boolean::eTrue; camera.motionVectors3D = sl::Boolean::eFalse;
    for (uint32_t frame = 0; frame < 4; ++frame) {
        if (FAILED(device->SetDepthStencilSurface(nullptr)) || FAILED(device->SetRenderTarget(0, motion.Get())) ||
            FAILED(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0)) || FAILED(device->SetRenderTarget(0, color.Get())) ||
            FAILED(device->SetDepthStencilSurface(depth.Get())) ||
            FAILED(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255,64,128,192), 0.5f, 0)) ||
            FAILED(device->SetDepthStencilSurface(nullptr)) || FAILED(device->SetRenderTarget(0, target.Get()))) return false;
        camera.reset = frame ? sl::Boolean::eFalse : sl::Boolean::eTrue;
        hr = resolve.resolve(color.Get(), depth.Get(), motion.Get(), target.Get(), camera, frame);
        if (FAILED(hr)) { std::printf("D3D9 -> DLSS -> D3D9 resolve: %08lX; SL %d\n", static_cast<unsigned long>(hr), int(resolve.lastEvaluation())); return false; }
        // HUD is drawn after the neural pass at output resolution.
        D3DRECT hud{0, 0, 32, 32};
        if (FAILED(device->Clear(1, &hud, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255,0,255,0), 1, 0))) return false;
        ComPtr<IDirect3DSurface9> readback;
        if (FAILED(device->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &readback, nullptr)) ||
            FAILED(device->GetRenderTargetData(target.Get(), readback.Get()))) return false;
        D3DLOCKED_RECT locked{};
        if (FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY))) return false;
        const DWORD hudPixel = *static_cast<const DWORD*>(locked.pBits);
        const DWORD pixel = *reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(locked.pBits) + size_t(height / 2) * locked.Pitch + (width / 2) * 4);
        readback->UnlockRect();
        const int r = int((pixel >> 16) & 255), g = int((pixel >> 8) & 255), b = int(pixel & 255);
        if (hudPixel != D3DCOLOR_ARGB(255,0,255,0) || r < 60 || r > 68 || g < 124 || g > 132 || b < 188 || b > 196) {
            std::printf("Resolve pixel mismatch: scene %08lX HUD %08lX\n", static_cast<unsigned long>(pixel), static_cast<unsigned long>(hudPixel)); return false;
        }
    }
    std::printf("PASS: D3D9On12 %s %ux%u -> %ux%u, real depth copy, four temporal frames, native HUD and readback.\n", mode == sl::DLSSMode::eDLAA ? "DLAA" : "DLSS Quality", rw, rh, width, height);
    return true;
}
