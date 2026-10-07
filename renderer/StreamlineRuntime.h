#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <sl.h>
#include <string>
#include <cstdio>

namespace generals_mods {
class StreamlineRuntime {
public:
    HRESULT initialize(ID3D12Device* device) {
        if (!device || module_) return E_INVALIDARG;
        wchar_t path[MAX_PATH]{};
        if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return HRESULT_FROM_WIN32(GetLastError());
        directory_ = path;
        directory_.resize(directory_.find_last_of(L"\\/"));
        directory_ += L"\\Streamline";
        module_ = LoadLibraryExW((directory_ + L"\\sl.interposer.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module_) return HRESULT_FROM_WIN32(GetLastError());
        auto initialize = function<PFun_slInit>("slInit");
        auto support = function<PFun_slIsFeatureSupported>("slIsFeatureSupported");
        auto setDevice = function<PFun_slSetD3DDevice>("slSetD3DDevice");
        if (!initialize || !support || !setDevice || !function<PFun_slShutdown>("slShutdown")) return E_NOINTERFACE;
        const wchar_t* paths[] = {directory_.c_str()};
        const sl::Feature feature = sl::kFeatureDLSS;
        sl::Preferences preferences{};
        preferences.pathsToPlugins = paths; preferences.numPathsToPlugins = 1;
        preferences.featuresToLoad = &feature; preferences.numFeaturesToLoad = 1;
        preferences.engine = sl::EngineType::eCustom; preferences.engineVersion = "generals-mods-development-1";
        preferences.projectId = "27e229d8-40c4-4c22-a261-78817dd7e765";
        preferences.renderAPI = sl::RenderAPI::eD3D12;
        preferences.flags = sl::PreferenceFlags::eUseManualHooking | sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eUseFrameBasedResourceTagging;
        preferences.logMessageCallback = [](sl::LogType type, const char* message) {
            if (type != sl::LogType::eError && type != sl::LogType::eWarn) return;
            FILE* log = nullptr;
            if (fopen_s(&log, "GeneralsNeuralAA.log", "a") == 0 && log) {
                std::fprintf(log, "PID %lu: %s\n", GetCurrentProcessId(), message); std::fclose(log);
            }
        };
        sl::Result result = initialize(preferences, sl::kSDKVersion);
        if (result != sl::Result::eOk) return E_FAIL;
        initialized_ = true;
        LUID luid = device->GetAdapterLuid();
        sl::AdapterInfo info{}; info.deviceLUID = reinterpret_cast<uint8_t*>(&luid); info.deviceLUIDSizeInBytes = sizeof(luid);
        if (support(feature, info) != sl::Result::eOk || setDevice(device) != sl::Result::eOk) return DXGI_ERROR_UNSUPPORTED;
        auto getFunction=function<PFun_slGetFeatureFunction>("slGetFeatureFunction");
        void* entry=nullptr;
        if(!getFunction || getFunction(sl::kFeatureCommon,"slHookPresent",entry)!=sl::Result::eOk || !entry) return E_NOINTERFACE;
        beforePresent_=reinterpret_cast<BeforePresent>(entry);
        entry=nullptr;
        if(getFunction(sl::kFeatureCommon,"slHookAfterPresent",entry)!=sl::Result::eOk || !entry) return E_NOINTERFACE;
        afterPresent_=reinterpret_cast<AfterPresent>(entry);
        ready_ = true;
        return S_OK;
    }
    bool ready() const { return ready_; }
    HMODULE module() const { return module_; }
    void beforePresent() {
        // The 9On12 swapchain has no public IDXGISwapChain accessor. In pinned
        // SL 2.14.1 this common hook uses only Flags for bookkeeping/collection;
        // DLSS SR does not need a native swapchain (unlike frame generation).
        if(ready_) {bool skip=false;beforePresent_(nullptr,0,0,skip);}
    }
    void afterPresent() { if(ready_) afterPresent_(0); }
    ~StreamlineRuntime() {
        if (initialized_) function<PFun_slShutdown>("slShutdown")();
        if (module_) FreeLibrary(module_);
    }
    StreamlineRuntime() = default;
    StreamlineRuntime(const StreamlineRuntime&) = delete;
private:
    template<class F> F* function(const char* name) const { return reinterpret_cast<F*>(GetProcAddress(module_, name)); }
    HMODULE module_ = nullptr;
    std::wstring directory_;
    bool initialized_ = false, ready_ = false;
    using BeforePresent=HRESULT (*)(IDXGISwapChain*,UINT,UINT,bool&);
    using AfterPresent=HRESULT (*)(UINT);
    BeforePresent beforePresent_=nullptr;
    AfterPresent afterPresent_=nullptr;
};
}
