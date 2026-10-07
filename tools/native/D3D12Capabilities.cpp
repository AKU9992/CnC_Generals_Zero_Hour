// Build-readiness probe. This does not replace or patch the game's renderer.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>

using Microsoft::WRL::ComPtr;

int main()
{
    ComPtr<IDXGIFactory6> factory;
    HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(result)) {
        std::printf("DXGI factory failed: 0x%08lX\n", static_cast<unsigned long>(result));
        return 1;
    }
    for (UINT index = 0; ; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        result = factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                     IID_PPV_ARGS(&adapter));
        if (result == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(result)) {
            std::printf("Adapter enumeration failed: 0x%08lX\n", static_cast<unsigned long>(result));
            return 2;
        }
        DXGI_ADAPTER_DESC1 description = {};
        if (FAILED(adapter->GetDesc1(&description)) || (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
        ComPtr<ID3D12Device> device;
        result = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
        if (FAILED(result)) continue;

        char adapterName[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, description.Description, -1, adapterName,
                            sizeof(adapterName), nullptr, nullptr);
        std::printf("Hardware D3D12 device created: %s\n", adapterName);
        std::printf("Vendor: 0x%04X; dedicated VRAM: %.0f MiB; process: %u-bit\n",
                    description.VendorId, static_cast<double>(description.DedicatedVideoMemory) / 1048576.0,
                    static_cast<unsigned>(sizeof(void*) * 8));
        D3D_FEATURE_LEVEL requested[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1,
                                         D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {};
        levels.NumFeatureLevels = _countof(requested);
        levels.pFeatureLevelsRequested = requested;
        result = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels));
        if (FAILED(result)) {
            std::printf("Feature query failed: 0x%08lX\n", static_cast<unsigned long>(result));
            return 3;
        }
        std::printf("Maximum feature level: 0x%04X\n", levels.MaxSupportedFeatureLevel);
        std::printf("Readiness probe only: no game rendering, DLSS integration or FPS measurement performed.\n");
        return 0;
    }
    std::printf("No usable hardware D3D12 adapter found in this session.\n");
    return 4;
}
