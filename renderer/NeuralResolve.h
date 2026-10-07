#pragma once
#include "D3D9On12Resources.h"
#include "DlaaPass.h"
#include <d3dcompiler.h>
#include <cstring>

namespace generals_mods {
// Resolve real D3D9On12 scene resources, then composite into the native-sized
// target. The caller supplies rasterized object motion and unjittered camera
// constants; this class never fabricates temporal inputs.
class NeuralResolve {
    template<class T> using Ptr = Microsoft::WRL::ComPtr<T>;
public:
    explicit NeuralResolve(HMODULE streamline) : pass_(streamline) {}
    ~NeuralResolve() { if (initialized_) { resources_.wait(); pass_.release(); } }
    HRESULT initialize(IDirect3DDevice9* device, UINT width, UINT height, sl::DLSSMode mode) {
        if (initialized_) return E_UNEXPECTED;
        HRESULT hr = resources_.initialize(device);
        if (FAILED(hr)) return hr;
        if (pass_.initialize(width, height, mode) != sl::Result::eOk) return E_FAIL;
        initialized_ = true;
        width_ = width; height_ = height;
        hr = resources_.device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_));
        if (SUCCEEDED(hr)) hr = resources_.device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&commands_));
        if (SUCCEEDED(hr)) hr = commands_->Close();
        if (FAILED(hr)) return hr;
        D3D12_DESCRIPTOR_HEAP_DESC desc{}; desc.NumDescriptors = 5;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr = resources_.device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&views_));
        desc.NumDescriptors = 1; desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (SUCCEEDED(hr)) hr = resources_.device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&targetView_));
        if (SUCCEEDED(hr)) hr = texture(DXGI_FORMAT_R32_FLOAT, renderWidth(), renderHeight(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &depth_);
        if (SUCCEEDED(hr)) hr = texture(DXGI_FORMAT_R16G16_FLOAT, renderWidth(), renderHeight(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &motion_);
        if (SUCCEEDED(hr)) hr = texture(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &output_);
        if (SUCCEEDED(hr)) hr = createShaders();
        return hr;
    }
    UINT renderWidth() const { return pass_.renderWidth(); }
    UINT renderHeight() const { return pass_.renderHeight(); }
    sl::Result lastEvaluation() const { return lastEvaluation_; }
    HRESULT resolve(IDirect3DResource9* color9, IDirect3DResource9* depth9,
                    IDirect3DResource9* motion9, IDirect3DResource9* target9,
                    const sl::Constants& camera, uint32_t frameIndex) {
        if (!initialized_ || !color9 || !depth9 || !motion9 || !target9 || color9 == target9) return E_INVALIDARG;
        HRESULT hr = resources_.wait();
        if (FAILED(hr)) return hr;
        Ptr<ID3D12Resource> color, sourceDepth, motion, target;
        if (FAILED(hr = resources_.acquire(color9, &color)) ||
            FAILED(hr = resources_.acquire(depth9, &sourceDepth)) ||
            FAILED(hr = resources_.acquire(motion9, &motion)) ||
            FAILED(hr = resources_.acquire(target9, &target))) {
            resources_.submit(nullptr); return hr;
        }
        const auto colorDesc = color->GetDesc(), depthDesc = sourceDepth->GetDesc();
        const auto motionDesc = motion->GetDesc(), targetDesc = target->GetDesc();
        if (colorDesc.Width != renderWidth() || colorDesc.Height != renderHeight() ||
            depthDesc.Width != renderWidth() || depthDesc.Height != renderHeight() ||
            motionDesc.Width != renderWidth() || motionDesc.Height != renderHeight() ||
            targetDesc.Width != width_ || targetDesc.Height != height_ ||
            colorDesc.SampleDesc.Count != 1 || depthDesc.SampleDesc.Count != 1 ||
            motionDesc.SampleDesc.Count != 1 || targetDesc.SampleDesc.Count != 1 ||
            motionDesc.Format != DXGI_FORMAT_R16G16_FLOAT || depthDesc.Format != DXGI_FORMAT_D24_UNORM_S8_UINT ||
            targetDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
            resources_.submit(nullptr); return E_INVALIDARG;
        }
        if (!depthCopy_) {
            hr = texture(DXGI_FORMAT_R24G8_TYPELESS, renderWidth(), renderHeight(), D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, &depthCopy_);
            if (FAILED(hr)) { resources_.submit(nullptr); return hr; }
        }
        if (FAILED(hr = allocator_->Reset()) || FAILED(hr = commands_->Reset(allocator_.Get(), nullptr))) {
            resources_.submit(nullptr); return hr;
        }
        transition(sourceDepth.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
        transition(depthCopy_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        commands_->CopyResource(depthCopy_.Get(), sourceDepth.Get());
        transition(sourceDepth.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
        transition(depthCopy_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        transition(depth_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transition(motion_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transition(motion.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        auto handle = views_->GetCPUDescriptorHandleForHeapStart();
        const UINT stride = resources_.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{}; srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; srv.Texture2D.MipLevels = 1; srv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        resources_.device()->CreateShaderResourceView(depthCopy_.Get(), &srv, handle);
        handle.ptr += stride;
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{}; uav.Format = DXGI_FORMAT_R32_FLOAT; uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        resources_.device()->CreateUnorderedAccessView(depth_.Get(), nullptr, &uav, handle);
        handle.ptr += stride; srv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        resources_.device()->CreateShaderResourceView(output_.Get(), &srv, handle);
        handle.ptr += stride; srv.Format = DXGI_FORMAT_R16G16_FLOAT;
        resources_.device()->CreateShaderResourceView(motion.Get(), &srv, handle);
        handle.ptr += stride; uav.Format = DXGI_FORMAT_R16G16_FLOAT;
        resources_.device()->CreateUnorderedAccessView(motion_.Get(), nullptr, &uav, handle);
        ID3D12DescriptorHeap* heaps[] = { views_.Get() };
        commands_->SetDescriptorHeaps(1, heaps);
        commands_->SetComputeRootSignature(root_.Get()); commands_->SetPipelineState(depthPipeline_.Get());
        commands_->SetComputeRootDescriptorTable(0, views_->GetGPUDescriptorHandleForHeapStart());
        struct ConversionConstants { sl::float4x4 clipToPrevious; sl::float2 jitter; sl::float2 size; } constants{
            camera.clipToPrevClip, camera.jitterOffset, {float(renderWidth()),float(renderHeight())}};
        commands_->SetComputeRoot32BitConstants(1, 20, &constants, 0);
        commands_->Dispatch((renderWidth() + 7) / 8, (renderHeight() + 7) / 8, 1);
        transition(depth_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        transition(color.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        transition(motion_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        transition(output_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        DlaaFrame frame{}; frame.frameIndex = frameIndex; frame.constants = camera;
        frame.sceneColor = color.Get(); frame.depth = depth_.Get(); frame.motionVectors = motion_.Get(); frame.outputColor = output_.Get();
        frame.commands = commands_.Get();
        frame.sceneColorState = frame.depthState = frame.motionVectorsState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        frame.outputColorState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        lastEvaluation_ = pass_.evaluate(frame);
        // On evaluation failure restore ownership and leave the target untouched.
        if (lastEvaluation_ == sl::Result::eOk) {
            transition(output_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            transition(target.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
            resources_.device()->CreateRenderTargetView(target.Get(), nullptr, targetView_->GetCPUDescriptorHandleForHeapStart());
            commands_->SetDescriptorHeaps(1, heaps); commands_->SetGraphicsRootSignature(root_.Get());
            commands_->SetPipelineState(blitPipeline_.Get());
            commands_->SetGraphicsRootDescriptorTable(0, views_->GetGPUDescriptorHandleForHeapStart());
            const auto rtv = targetView_->GetCPUDescriptorHandleForHeapStart(); commands_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            D3D12_VIEWPORT viewport{0, 0, float(width_), float(height_), 0, 1}; D3D12_RECT rect{0, 0, LONG(width_), LONG(height_)};
            commands_->RSSetViewports(1, &viewport); commands_->RSSetScissorRects(1, &rect);
            commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); commands_->DrawInstanced(3, 1, 0, 0);
            transition(target.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
            transition(output_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        } else transition(output_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        transition(color.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        transition(motion.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        transition(motion_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        transition(depth_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        transition(depthCopy_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        hr = commands_->Close();
        if (SUCCEEDED(hr)) hr = resources_.submit(commands_.Get());
        else resources_.submit(nullptr);
        return FAILED(hr) ? hr : lastEvaluation_ == sl::Result::eOk ? S_OK : E_FAIL;
    }
private:
    HRESULT texture(DXGI_FORMAT format, UINT width, UINT height, D3D12_RESOURCE_FLAGS flags, ID3D12Resource** out) {
        D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc{}; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width; desc.Height = height; desc.DepthOrArraySize = desc.MipLevels = 1; desc.SampleDesc.Count = 1;
        desc.Format = format; desc.Flags = flags;
        return resources_.device()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(out));
    }
    void transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource; barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before; barrier.Transition.StateAfter = after;
        commands_->ResourceBarrier(1, &barrier);
    }
    HRESULT createShaders() {
        const char* shader = R"(
Texture2D<float> sourceDepth : register(t0);
RWTexture2D<float> convertedDepth : register(u0);
Texture2D<float4> filteredColor : register(t1);
Texture2D<float2> objectMotion : register(t2);
RWTexture2D<float2> mergedMotion : register(u1);
cbuffer Conversion : register(b0) { row_major float4x4 clipToPrevious; float2 jitter; float2 size; };
[numthreads(8,8,1)] void ConvertDepth(uint3 tid:SV_DispatchThreadID) {
 uint w,h; convertedDepth.GetDimensions(w,h); if(tid.x>=w || tid.y>=h) return;
 float depth=sourceDepth.Load(int3(tid.xy,0)); convertedDepth[tid.xy]=depth;
 float2 motion=objectMotion.Load(int3(tid.xy,0));
 if(abs(motion.x)>=0.99 || abs(motion.y)>=0.99) {
  float2 uv=(float2(tid.xy)+0.5-jitter)/size;
  float4 now=float4(uv*float2(2,-2)+float2(-1,1),depth,1);
  float4 old=mul(now,clipToPrevious); float2 previousUV=old.xy/old.w*float2(0.5,-0.5)+0.5;
  motion=previousUV-uv;
 }
 mergedMotion[tid.xy]=motion;
}
float4 Fullscreen(uint id:SV_VertexID):SV_Position {
 return float4(id==2 ? 3:-1, id==1 ? 3:-1, 0, 1);
}
float4 Composite(float4 position:SV_Position):SV_Target { return filteredColor.Load(int3(position.xy,0)); }
)";
        Ptr<ID3DBlob> cs, vs, ps, errors;
        HRESULT hr = D3DCompile(shader, std::strlen(shader), nullptr, nullptr, nullptr, "ConvertDepth", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &cs, &errors);
        if (SUCCEEDED(hr)) hr = D3DCompile(shader, std::strlen(shader), nullptr, nullptr, nullptr, "Fullscreen", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, &errors);
        if (SUCCEEDED(hr)) hr = D3DCompile(shader, std::strlen(shader), nullptr, nullptr, nullptr, "Composite", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, &errors);
        if (FAILED(hr)) return hr;
        D3D12_DESCRIPTOR_RANGE ranges[5]{};
        const UINT registers[]={0,0,1,2,1};
        for (UINT i=0;i<5;++i) { ranges[i].RangeType = i==1 || i==4 ? D3D12_DESCRIPTOR_RANGE_TYPE_UAV : D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            ranges[i].NumDescriptors = 1; ranges[i].BaseShaderRegister = registers[i]; ranges[i].OffsetInDescriptorsFromTableStart = i; }
        D3D12_ROOT_PARAMETER parameters[2]{}; parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[0].DescriptorTable.NumDescriptorRanges = 5; parameters[0].DescriptorTable.pDescriptorRanges = ranges;
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; parameters[1].Constants.Num32BitValues = 20;
        D3D12_ROOT_SIGNATURE_DESC rootDesc{}; rootDesc.NumParameters = 2; rootDesc.pParameters = parameters;
        Ptr<ID3DBlob> signature;
        hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
        if (SUCCEEDED(hr)) hr = resources_.device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_));
        if (FAILED(hr)) return hr;
        D3D12_COMPUTE_PIPELINE_STATE_DESC compute{}; compute.pRootSignature = root_.Get(); compute.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
        hr = resources_.device()->CreateComputePipelineState(&compute, IID_PPV_ARGS(&depthPipeline_));
        if (FAILED(hr)) return hr;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics{}; graphics.pRootSignature = root_.Get();
        graphics.VS = {vs->GetBufferPointer(), vs->GetBufferSize()}; graphics.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        graphics.SampleMask = UINT_MAX; graphics.SampleDesc.Count = 1;
        graphics.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; graphics.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        graphics.RasterizerState.DepthClipEnable = TRUE;
        graphics.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        graphics.DepthStencilState.DepthEnable = FALSE; graphics.DepthStencilState.StencilEnable = FALSE;
        graphics.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; graphics.NumRenderTargets = 1;
        graphics.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
        return resources_.device()->CreateGraphicsPipelineState(&graphics, IID_PPV_ARGS(&blitPipeline_));
    }
    D3D9On12Resources resources_;
    DlaaPass pass_;
    Ptr<ID3D12CommandAllocator> allocator_;
    Ptr<ID3D12GraphicsCommandList> commands_;
    Ptr<ID3D12DescriptorHeap> views_, targetView_;
    Ptr<ID3D12Resource> depthCopy_, depth_, motion_, output_;
    Ptr<ID3D12RootSignature> root_;
    Ptr<ID3D12PipelineState> depthPipeline_, blitPipeline_;
    UINT width_ = 0, height_ = 0;
    bool initialized_ = false;
    sl::Result lastEvaluation_ = sl::Result::eErrorNotInitialized;
};
}
