// Standalone SDK/device test. This does not render or alter the game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <sl.h>
#include <sl_dlss.h>
#include <cstdio>
#include <string>
#include "DlaaFrameTest.h"
#include "NeuralResolveTests.h"
#include "MotionCaptureTests.h"

using Microsoft::WRL::ComPtr;

template<class Function> Function* importFunction(HMODULE module, const char* name)
{
    return reinterpret_cast<Function*>(GetProcAddress(module, name));
}

int wmain(int argc, wchar_t** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 2) return 2;
    const std::wstring pluginPath = std::wstring(argv[1]) + L"\\bin\\x64";
    const std::wstring interposerPath = pluginPath + L"\\sl.interposer.dll";
    std::puts("Loading the signed NVIDIA Streamline interposer...");
    HMODULE module = LoadLibraryExW(interposerPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) { std::printf("Streamline load failed: %lu\n", GetLastError()); return 3; }
    auto* initialize = importFunction<PFun_slInit>(module, "slInit");
    auto* shutdown = importFunction<PFun_slShutdown>(module, "slShutdown");
    auto* isSupported = importFunction<PFun_slIsFeatureSupported>(module, "slIsFeatureSupported");
    auto* setDevice = importFunction<PFun_slSetD3DDevice>(module, "slSetD3DDevice");
    auto* getFeatureFunction = importFunction<PFun_slGetFeatureFunction>(module, "slGetFeatureFunction");
    if (!initialize || !shutdown || !isSupported || !setDevice || !getFeatureFunction) return 4;
    const wchar_t* pluginPaths[] = { pluginPath.c_str() };
    const sl::Feature feature = sl::kFeatureDLSS; // DLAA is a mode of this feature.
    sl::Preferences preferences{};
    preferences.pathsToPlugins = pluginPaths;
    preferences.numPathsToPlugins = 1;
    preferences.featuresToLoad = &feature;
    preferences.numFeaturesToLoad = 1;
    preferences.engine = sl::EngineType::eCustom;
    preferences.engineVersion = "generals-mods-development-1";
    preferences.projectId = "27e229d8-40c4-4c22-a261-78817dd7e765";
    preferences.renderAPI = sl::RenderAPI::eD3D12;
    preferences.flags = sl::PreferenceFlags::eUseManualHooking | sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    preferences.logLevel = sl::LogLevel::eDefault;
    preferences.logMessageCallback = [](sl::LogType type, const char* message) {
        FILE* log = nullptr;
        if (fopen_s(&log, "sdk.log", "a") == 0 && log) {
            std::fprintf(log, "%s\n", message);
            std::fclose(log);
        }
        if (type == sl::LogType::eError) std::printf("Streamline error: %s\n", message);
    };
    std::puts("Initializing the DLSS/DLAA plugin...");
    sl::Result result = initialize(preferences, sl::kSDKVersion);
    std::printf("slInit: %d\n", static_cast<int>(result));
    if (result != sl::Result::eOk) return 5;
    int status = 6;
    {
        ComPtr<IDXGIFactory6> factory;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
            for (UINT index = 0; ; ++index) {
                ComPtr<IDXGIAdapter1> adapter;
                HRESULT adapterResult = factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
                if (adapterResult == DXGI_ERROR_NOT_FOUND) break;
                if (FAILED(adapterResult)) continue;
                DXGI_ADAPTER_DESC1 description{};
                if (FAILED(adapter->GetDesc1(&description)) || (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
                sl::AdapterInfo info{};
                info.deviceLUID = reinterpret_cast<uint8_t*>(&description.AdapterLuid);
                info.deviceLUIDSizeInBytes = sizeof(description.AdapterLuid);
                result = isSupported(feature, info);
                std::wprintf(L"Adapter: %ls\n", description.Description);
                std::printf("DLSS/DLAA feature support: %d\n", static_cast<int>(result));
                if (result != sl::Result::eOk) continue;
                ComPtr<ID3D12Device> device;
                if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) continue;
                result = setDevice(device.Get());
                std::printf("slSetD3DDevice: %d\n", static_cast<int>(result));
                if (result != sl::Result::eOk) continue;
                void* entry = nullptr;
                result = getFeatureFunction(feature, "slDLSSGetOptimalSettings", entry);
                if (result != sl::Result::eOk || !entry) continue;
                auto* optimalSettings = reinterpret_cast<PFun_slDLSSGetOptimalSettings*>(entry);
                sl::DLSSOptions options{};
                options.mode = sl::DLSSMode::eDLAA;
                options.outputWidth = 3440;
                options.outputHeight = 1440;
                options.colorBuffersHDR = sl::Boolean::eFalse;
                sl::DLSSOptimalSettings settings{};
                result = optimalSettings(options, settings);
                std::printf("DLAA settings: %d; input %ux%u; output %ux%u\n", static_cast<int>(result),
                    settings.optimalRenderWidth, settings.optimalRenderHeight, options.outputWidth, options.outputHeight);
                if (result == sl::Result::eOk && settings.optimalRenderWidth == options.outputWidth && settings.optimalRenderHeight == options.outputHeight) status = 0;
                if (!status && !testDlaaFrame(device.Get(), module)) status = 8;
                if (!status && !testDlaaFrame(device.Get(), module, 3840, 2160, 1)) status = 9;
                if (!status && !testDlaaFrame(device.Get(), module, 3440, 1440, 2, sl::DLSSMode::eMaxQuality)) status = 10;
                if (!status && !testDlaaFrame(device.Get(), module, 3840, 2160, 3, sl::DLSSMode::eMaxQuality)) status = 11;
                if (!status && !testMotionCapture(device.Get())) status = 16;
                if (!status && !testNeuralResolve(device.Get(), module, 3440, 1440, sl::DLSSMode::eDLAA)) status = 12;
                if (!status && !testNeuralResolve(device.Get(), module, 3440, 1440, sl::DLSSMode::eMaxQuality)) status = 13;
                if (!status && !testNeuralResolve(device.Get(), module, 3840, 2160, sl::DLSSMode::eDLAA)) status = 14;
                if (!status && !testNeuralResolve(device.Get(), module, 3840, 2160, sl::DLSSMode::eMaxQuality)) status = 15;
                // Shutdown while the graphics device still exists, as required by SDK.
                result = shutdown();
                std::printf("slShutdown: %d\n", static_cast<int>(result));
                FreeLibrary(module);
                if (!status && result != sl::Result::eOk) status = 7;
                if (!status) std::puts("PASS: NVIDIA DLAA and DLSS Quality GPU evaluation at 3440x1440 and 3840x2160. Game integration remains pending.");
                return status;
            }
        }
    }
    shutdown();
    FreeLibrary(module);
    return status;
}
