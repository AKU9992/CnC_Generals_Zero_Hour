#pragma once
#include "NativeDevice12.h"
#include "../StreamlineRuntime.h"
#include "../DlaaPass.h"
#include <memory>

namespace generals_mods::native12 {
enum class NeuralMode { Off, DLSSQuality, DLAA };

// The scene renderer provides native color, depth, motion and camera data.
// This boundary does not unwrap D3D9 resources or generate synthetic motion.
// Declare after Device so it is destroyed before the device/queue.
class Dlss {
public:
    explicit Dlss(Device& renderer) : renderer_(renderer) {}
    ~Dlss() {
        if (renderer_.commands()) {
            // Pending recording is not submitted by this object. Drain earlier work.
            renderer_.waitIdle();
            if (pass_) pass_->release();
        } else shutdown();
    }
    Dlss(const Dlss&) = delete;
    Dlss& operator=(const Dlss&) = delete;

    HRESULT configure(NeuralMode mode, UINT width, UINT height) {
        if (renderer_.commands() || !renderer_.device() || !width || !height) return E_INVALIDARG;
        if (mode!=NeuralMode::Off && mode!=NeuralMode::DLSSQuality && mode!=NeuralMode::DLAA) return E_INVALIDARG;
        HRESULT hr=renderer_.waitIdle();
        if (FAILED(hr)) return hr;
        if (pass_) {
            lastResult_=pass_->release();
            if (lastResult_!=sl::Result::eOk) return E_FAIL;
        }
        pass_.reset(); mode_=NeuralMode::Off; width_=height_=0;
        if (mode==NeuralMode::Off) return S_OK;
        if (!runtime_) {
            auto runtime=std::make_unique<StreamlineRuntime>();
            hr=runtime->initialize(renderer_.device());
            if (FAILED(hr)) return hr;
            runtime_=std::move(runtime);
        }
        auto pass=std::make_unique<DlaaPass>(runtime_->module());
        lastResult_=pass->initialize(width,height,
            mode==NeuralMode::DLAA ? sl::DLSSMode::eDLAA : sl::DLSSMode::eMaxQuality);
        if (lastResult_!=sl::Result::eOk) return E_FAIL;
        pass_=std::move(pass); mode_=mode; width_=width; height_=height;
        return S_OK;
    }
    HRESULT evaluate(const DlaaFrame& frame) {
        if (!pass_ || !runtime_ || !runtime_->ready()) return E_UNEXPECTED;
        if (!frame.commands || !frame.sceneColor || !frame.depth || !frame.motionVectors || !frame.outputColor)
            return E_INVALIDARG;
        // All inputs must belong to this native device. Reject cross-device resources.
        for (auto* object : {static_cast<ID3D12DeviceChild*>(frame.commands),
             static_cast<ID3D12DeviceChild*>(frame.sceneColor),static_cast<ID3D12DeviceChild*>(frame.depth),
             static_cast<ID3D12DeviceChild*>(frame.motionVectors),static_cast<ID3D12DeviceChild*>(frame.outputColor)}) {
            Microsoft::WRL::ComPtr<ID3D12Device> owner;
            HRESULT hr=object->GetDevice(IID_PPV_ARGS(&owner));
            if (FAILED(hr)) return hr;
            if (owner.Get()!=renderer_.device()) return E_INVALIDARG;
        }
        lastResult_=pass_->evaluate(frame);
        return lastResult_==sl::Result::eOk ? S_OK : E_FAIL;
    }
    // Invoke immediately around native DXGI Present. No second Present or queue submission.
    void beforePresent(UINT interval=0,UINT flags=0) {
        if (runtime_) runtime_->beforePresent(renderer_.swapchain(),interval,flags);
    }
    void afterPresent() { if (runtime_) runtime_->afterPresent(); }
    HRESULT shutdown() {
        if (!runtime_) return S_OK;
        if (renderer_.commands()) return E_UNEXPECTED;
        HRESULT hr=renderer_.waitIdle();
        if (FAILED(hr)) return hr;
        if (pass_) {
            lastResult_=pass_->release();
            if (lastResult_!=sl::Result::eOk) return E_FAIL;
        }
        pass_.reset(); runtime_.reset(); mode_=NeuralMode::Off; width_=height_=0;
        return S_OK;
    }
    UINT renderWidth() const { return pass_ ? pass_->renderWidth() : renderer_.width(); }
    UINT renderHeight() const { return pass_ ? pass_->renderHeight() : renderer_.height(); }
    NeuralMode mode() const { return mode_; }
    sl::Result lastResult() const { return lastResult_; }
    // Used by isolated tests of the same D3D12 DLSS pass; not a D3D9 entry point.
    HMODULE module() const { return runtime_ ? runtime_->module() : nullptr; }
private:
    Device& renderer_;
    std::unique_ptr<StreamlineRuntime> runtime_;
    std::unique_ptr<DlaaPass> pass_;
    NeuralMode mode_=NeuralMode::Off;
    UINT width_=0,height_=0;
    sl::Result lastResult_=sl::Result::eErrorNotInitialized;
};
}
