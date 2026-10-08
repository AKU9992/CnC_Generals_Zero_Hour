#pragma once
#include <windows.h>
#include <d3d12.h>
#include <sl.h>
#include <sl_dlss.h>
#include <initializer_list>

namespace generals_mods {

// D3D12 integration boundary. The renderer owns these textures until GPU work
// finishes and restores its command-list state after evaluation.
struct DlaaFrame {
    ID3D12Resource* sceneColor = nullptr;
    ID3D12Resource* depth = nullptr;
    ID3D12Resource* motionVectors = nullptr;
    ID3D12Resource* outputColor = nullptr;
    ID3D12GraphicsCommandList* commands = nullptr;
    D3D12_RESOURCE_STATES sceneColorState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES depthState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES motionVectorsState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES outputColorState = D3D12_RESOURCE_STATE_COMMON;
    sl::Constants constants{}; // Actual current/previous camera, jitter, reset.
    uint32_t frameIndex = 0;
};

class DlaaPass {
public:
    explicit DlaaPass(HMODULE streamLine) : module_(streamLine) {}
    DlaaPass(const DlaaPass&) = delete;
    DlaaPass& operator=(const DlaaPass&) = delete;

    sl::Result initialize(uint32_t width, uint32_t height, sl::DLSSMode mode = sl::DLSSMode::eDLAA)
    {
        ready_ = false;
        if (!module_ || !width || !height) return sl::Result::eErrorInvalidParameter;
        if (mode != sl::DLSSMode::eDLAA && mode != sl::DLSSMode::eMaxQuality) return sl::Result::eErrorInvalidParameter;
        auto* getFunction = core<PFun_slGetFeatureFunction>("slGetFeatureFunction");
        if (!getFunction) return sl::Result::eErrorNotInitialized;
        void* function = nullptr;
        sl::Result result = getFunction(sl::kFeatureDLSS, "slDLSSSetOptions", function);
        if (result != sl::Result::eOk) return result;
        if (!function) return sl::Result::eErrorNotInitialized;
        setOptions_ = reinterpret_cast<PFun_slDLSSSetOptions*>(function);
        newFrame_ = core<PFun_slGetNewFrameToken>("slGetNewFrameToken");
        setConstants_ = core<PFun_slSetConstants>("slSetConstants");
        setTags_ = core<PFun_slSetTagForFrame>("slSetTagForFrame");
        evaluate_ = core<PFun_slEvaluateFeature>("slEvaluateFeature");
        freeResources_ = core<PFun_slFreeResources>("slFreeResources");
        if (!newFrame_ || !setConstants_ || !setTags_ || !evaluate_ || !freeResources_) return sl::Result::eErrorNotInitialized;
        sl::DLSSOptions options{};
        options.mode = mode;
        options.outputWidth = width;
        options.outputHeight = height;
        options.colorBuffersHDR = sl::Boolean::eFalse;
        options.useAutoExposure = sl::Boolean::eTrue;
        function = nullptr;
        result = getFunction(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", function);
        if (result != sl::Result::eOk) return result;
        if (!function) return sl::Result::eErrorNotInitialized;
        sl::DLSSOptimalSettings settings{};
        result = reinterpret_cast<PFun_slDLSSGetOptimalSettings*>(function)(options, settings);
        if (result != sl::Result::eOk) return result;
        if (!settings.optimalRenderWidth || !settings.optimalRenderHeight) return sl::Result::eErrorInvalidParameter;
        result = setOptions_(viewport_, options);
        if (result == sl::Result::eOk) {
            width_ = width; height_ = height;
            renderWidth_ = settings.optimalRenderWidth; renderHeight_ = settings.optimalRenderHeight;
            ready_ = true;
        }
        return result;
    }

    uint32_t renderWidth() const { return renderWidth_; }
    uint32_t renderHeight() const { return renderHeight_; }

    sl::Result evaluate(const DlaaFrame& frame)
    {
        if (!ready_) return sl::Result::eErrorNotInitialized;
        if (!frame.commands || !frame.sceneColor || !frame.depth || !frame.motionVectors || !frame.outputColor ||
            frame.sceneColor == frame.outputColor) return sl::Result::eErrorInvalidParameter;
        for (ID3D12Resource* resource : {frame.sceneColor, frame.depth, frame.motionVectors, frame.outputColor}) {
            const auto desc = resource->GetDesc();
            const bool output = resource == frame.outputColor;
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
                desc.Width != (output ? width_ : renderWidth_) || desc.Height != (output ? height_ : renderHeight_) || desc.SampleDesc.Count != 1)
                return sl::Result::eErrorInvalidParameter;
        }
        sl::FrameToken* token = nullptr;
        sl::Result result = newFrame_(token, &frame.frameIndex);
        if (result != sl::Result::eOk) return result;
        if (!token) return sl::Result::eErrorNotInitialized;
        result = setConstants_(frame.constants, *token, viewport_);
        if (result != sl::Result::eOk) return result;
        sl::Resource scene(sl::ResourceType::eTex2d, frame.sceneColor, frame.sceneColorState);
        sl::Resource depth(sl::ResourceType::eTex2d, frame.depth, frame.depthState);
        sl::Resource motion(sl::ResourceType::eTex2d, frame.motionVectors, frame.motionVectorsState);
        sl::Resource output(sl::ResourceType::eTex2d, frame.outputColor, frame.outputColorState);
        sl::Extent extent{0, 0, width_, height_};
        sl::Extent renderExtent{0, 0, renderWidth_, renderHeight_};
        const sl::ResourceTag tags[] = {
            {&scene, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &renderExtent},
            {&depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilEvaluate, &renderExtent},
            {&motion, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilEvaluate, &renderExtent},
            {&output, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &extent}
        };
        result = setTags_(*token, viewport_, tags, 4, frame.commands);
        if (result != sl::Result::eOk) return result;
        const sl::BaseStructure* inputs[] = { &viewport_ };
        evaluated_ = true;
        return evaluate_(sl::kFeatureDLSS, *token, inputs, 1, frame.commands);
    }

    // Call after waiting for GPU completion, before destroying SL/device.
    sl::Result release()
    {
        if (!ready_) return sl::Result::eOk;
        if (!evaluated_) { ready_ = false; return sl::Result::eOk; }
        const auto result = freeResources_(sl::kFeatureDLSS, viewport_);
        if (result == sl::Result::eOk) { ready_ = false; evaluated_ = false; }
        return result;
    }

private:
    template<class Function> Function* core(const char* name) const { return reinterpret_cast<Function*>(GetProcAddress(module_, name)); }
    HMODULE module_ = nullptr;
    sl::ViewportHandle viewport_{0};
    uint32_t width_ = 0, height_ = 0;
    uint32_t renderWidth_ = 0, renderHeight_ = 0;
    bool ready_ = false;
    bool evaluated_ = false;
    PFun_slDLSSSetOptions* setOptions_ = nullptr;
    PFun_slGetNewFrameToken* newFrame_ = nullptr;
    PFun_slSetConstants* setConstants_ = nullptr;
    PFun_slSetTagForFrame* setTags_ = nullptr;
    PFun_slEvaluateFeature* evaluate_ = nullptr;
    PFun_slFreeResources* freeResources_ = nullptr;
};
}
