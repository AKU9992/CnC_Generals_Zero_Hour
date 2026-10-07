#define WIN32_LEAN_AND_MEAN
#include "../../renderer/D3D9On12Resources.h"
#include <cstdio>
#include <cstring>
using Microsoft::WRL::ComPtr;
#define CHECK(x) do { HRESULT result = (x); if (FAILED(result)) { std::printf("FAIL line %d: 0x%08lX\n", __LINE__, static_cast<unsigned long>(result)); return 1; } } while (0)
int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    HWND window = CreateWindowExW(0, L"STATIC", L"Resource interoperability test", WS_OVERLAPPEDWINDOW, 0, 0, 256, 256, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return 2;
    D3D9ON12_ARGS args{}; args.Enable9On12 = TRUE;
    ComPtr<IDirect3D9> d3d;
    d3d.Attach(Direct3DCreate9On12(D3D_SDK_VERSION, &args, 1));
    if (!d3d) return 3;
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = pp.BackBufferHeight = 256; pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8; pp.hDeviceWindow = window;
    ComPtr<IDirect3DDevice9> device;
    CHECK(d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device));
    generals_mods::D3D9On12Resources resources;
    CHECK(resources.initialize(device.Get()));
    ComPtr<IDirect3DSurface9> back, depth;
    CHECK(device->GetRenderTarget(0, &back));
    CHECK(device->GetDepthStencilSurface(&depth));
    CHECK(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255, 64, 128, 192), 0.5f, 0));
    ComPtr<ID3D12Resource> nativeColor, nativeDepth;
    CHECK(resources.acquire(back.Get(), &nativeColor));
    CHECK(resources.acquire(depth.Get(), &nativeDepth));
    const auto depthDesc = nativeDepth->GetDesc();
    std::printf("Native color format %u; depth format %u flags 0x%X\n", nativeColor->GetDesc().Format, depthDesc.Format, depthDesc.Flags);
    // Depth may be DENY_SHADER_RESOURCE: the integration must copy it to a
    // shader-readable typeless depth texture rather than sample it in place.
    ComPtr<ID3D12CommandAllocator> allocator;
    CHECK(resources.device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
    ComPtr<ID3D12GraphicsCommandList> commands;
    CHECK(resources.device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commands)));
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 1;
    CHECK(resources.device()->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)));
    resources.device()->CreateRenderTargetView(nativeColor.Get(), nullptr, heap->GetCPUDescriptorHandleForHeapStart());
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = nativeColor.Get(); barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commands->ResourceBarrier(1, &barrier);
    const float color[] = {0.75f, 0.5f, 0.25f, 1.0f};
    commands->ClearRenderTargetView(heap->GetCPUDescriptorHandleForHeapStart(), color, 0, nullptr);
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    commands->ResourceBarrier(1, &barrier);
    CHECK(commands->Close());
    CHECK(resources.submit(commands.Get()));
    // Return waits must be honoured without a CPU stall before D3D9 readback.
    ComPtr<IDirect3DSurface9> readback;
    CHECK(device->CreateOffscreenPlainSurface(256, 256, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &readback, nullptr));
    CHECK(device->GetRenderTargetData(back.Get(), readback.Get()));
    D3DLOCKED_RECT locked{};
    CHECK(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY));
    const DWORD pixel = *static_cast<const DWORD*>(locked.pBits);
    CHECK(readback->UnlockRect());
    const int red = int((pixel >> 16) & 255), green = int((pixel >> 8) & 255), blue = int(pixel & 255);
    if ((pixel >> 24) != 255 || red < 190 || red > 192 || green < 127 || green > 129 || blue < 63 || blue > 65) {
        std::printf("FAIL readback: %08lX\n", static_cast<unsigned long>(pixel)); return 4;
    }
    CHECK(resources.wait());
    CHECK(device->Present(nullptr, nullptr, nullptr, nullptr));
    std::puts("PASS: D3D9 scene/depth checkout, native D3D12 write, fence return and D3D9 pixel readback.");
    DestroyWindow(window);
    return 0;
}
