#include "NativeDevice12.h"
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <cstring>
#include <limits>

namespace generals_mods::native12 {
namespace {
D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES result{};
    result.Type = type;
    result.CreationNodeMask = result.VisibleNodeMask = 1;
    return result;
}
D3D12_RESOURCE_DESC buffer(UINT64 size) {
    D3D12_RESOURCE_DESC result{};
    result.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    result.Width = size; result.Height = 1;
    result.DepthOrArraySize = result.MipLevels = 1;
    result.SampleDesc.Count = 1;
    result.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return result;
}
}
Device::~Device() { shutdown(); }

HRESULT Device::initialize(HWND window, UINT width, UINT height, bool debug) {
    if (!IsWindow(window) || !width || !height || device_) return E_INVALIDARG;
    UINT flags = 0;
    if (debug) {
        Ptr<ID3D12Debug> layer;
        HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&layer));
        if (FAILED(hr)) return hr; // Do not silently omit requested validation.
        layer->EnableDebugLayer();
        flags = DXGI_CREATE_FACTORY_DEBUG;
    }
    HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory_));
    if (FAILED(hr)) return hr;
    for (UINT adapterIndex = 0; ; ++adapterIndex) {
        Ptr<IDXGIAdapter1> adapter;
        hr = factory_->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                  IID_PPV_ARGS(&adapter));
        if (hr == DXGI_ERROR_NOT_FOUND) return DXGI_ERROR_UNSUPPORTED;
        if (FAILED(hr)) return hr;
        DXGI_ADAPTER_DESC1 desc{};
        hr = adapter->GetDesc1(&desc);
        if (FAILED(hr)) return hr;
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_));
        if (SUCCEEDED(hr)) { adapterDescription_ = desc; break; }
    }
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue_));
    if (FAILED(hr)) return hr;
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width; desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = frameCount;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    Ptr<IDXGISwapChain1> swap;
    hr = factory_->CreateSwapChainForHwnd(queue_.Get(), window, &desc, nullptr, nullptr, &swap);
    if (SUCCEEDED(hr)) hr = swap.As(&swap_);
    if (SUCCEEDED(hr)) hr = factory_->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr)) return hr;
    width_ = width; height_ = height;
    D3D12_DESCRIPTOR_HEAP_DESC targetDesc{};
    targetDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; targetDesc.NumDescriptors = frameCount;
    hr = device_->CreateDescriptorHeap(&targetDesc, IID_PPV_ARGS(&targets_));
    if (FAILED(hr)) return hr;
    targetStride_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    if (FAILED(hr)) return hr;
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_) return HRESULT_FROM_WIN32(GetLastError());
    const auto uploadHeap = heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto uploadDesc = buffer(uploadBytes);
    for (auto& frame : frames_) {
        hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator));
        if (SUCCEEDED(hr)) hr = device_->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
            &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&frame.upload));
        D3D12_RANGE noRead{0, 0};
        if (SUCCEEDED(hr)) hr = frame.upload->Map(0, &noRead, reinterpret_cast<void**>(&frame.mapped));
        if (FAILED(hr)) return hr;
    }
    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames_[0].allocator.Get(),
                                   nullptr, IID_PPV_ARGS(&commands_));
    if (SUCCEEDED(hr)) hr = commands_->Close();
    if (SUCCEEDED(hr)) hr = createPipeline();
    if (SUCCEEDED(hr)) hr = createTargets();
    return hr;
}

HRESULT Device::createTargets() {
    auto handle = targets_->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < frameCount; ++i) {
        HRESULT hr = swap_->GetBuffer(i, IID_PPV_ARGS(&frames_[i].target));
        if (FAILED(hr)) return hr;
        device_->CreateRenderTargetView(frames_[i].target.Get(), nullptr, handle);
        handle.ptr += targetStride_;
    }
    return S_OK;
}
HRESULT Device::createPipeline() {
    constexpr char shader[] = R"(
cbuffer Camera : register(b0) { row_major float4x4 transform; };
struct Input { float3 position : POSITION; float4 color : COLOR; };
struct Output { float4 position : SV_POSITION; float4 color : COLOR; };
Output VS(Input v) { Output o; o.position=mul(float4(v.position,1),transform); o.color=v.color; return o; }
float4 PS(Output v) : SV_TARGET { return v.color; }
)";
    Ptr<ID3DBlob> vertex, pixel, serialized, errors;
    HRESULT hr = D3DCompile(shader, sizeof(shader)-1, "Native12", nullptr, nullptr, "VS", "vs_5_0",
                           D3DCOMPILE_ENABLE_STRICTNESS, 0, &vertex, &errors);
    if (SUCCEEDED(hr)) hr = D3DCompile(shader, sizeof(shader)-1, "Native12", nullptr, nullptr, "PS", "ps_5_0",
                                     D3DCOMPILE_ENABLE_STRICTNESS, 0, &pixel, &errors);
    if (FAILED(hr)) return hr;
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameter.Constants.Num32BitValues = 16;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 1; rootDesc.pParameters = &parameter;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
    if (SUCCEEDED(hr)) hr = device_->CreateRootSignature(0, serialized->GetBufferPointer(),
        serialized->GetBufferSize(), IID_PPV_ARGS(&root_));
    if (FAILED(hr)) return hr;
    const D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = root_.Get();
    pso.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
    pso.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    pso.InputLayout = {layout, _countof(layout)};
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    auto& blend = pso.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;
    return device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipeline_));
}

HRESULT Device::wait(UINT64 value) {
    if (!value) return S_OK;
    if (!fence_ || !event_) return E_UNEXPECTED;
    const UINT64 completed = fence_->GetCompletedValue();
    if (completed == UINT64_MAX) return device_->GetDeviceRemovedReason();
    if (completed >= value) return S_OK;
    HRESULT hr = fence_->SetEventOnCompletion(value, event_);
    if (FAILED(hr)) return hr;
    // A hung GPU must not leave the process blocked indefinitely.
    const DWORD result = WaitForSingleObject(event_, 30000);
    if (result == WAIT_OBJECT_0) return S_OK;
    if (result == WAIT_TIMEOUT) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    return HRESULT_FROM_WIN32(GetLastError());
}
HRESULT Device::signal(UINT64& value) {
    const UINT64 next = nextFence_++;
    HRESULT hr = queue_->Signal(fence_.Get(), next);
    if (SUCCEEDED(hr)) value = next;
    return hr;
}
HRESULT Device::waitIdle() {
    if (!queue_ || !fence_ || !event_) return S_OK;
    UINT64 value = 0;
    HRESULT hr = signal(value);
    return FAILED(hr) ? hr : wait(value);
}
void Device::barrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = resource;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = before; b.Transition.StateAfter = after;
    commands_->ResourceBarrier(1, &b);
}
D3D12_CPU_DESCRIPTOR_HANDLE Device::targetHandle() const {
    auto handle = targets_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index_) * targetStride_;
    return handle;
}
HRESULT Device::beginFrame(const float clear[4]) {
    if (!swap_ || recording_ || !clear) return E_UNEXPECTED;
    index_ = swap_->GetCurrentBackBufferIndex();
    auto& frame = frames_[index_];
    HRESULT hr = wait(frame.fenceValue);
    if (SUCCEEDED(hr)) hr = frame.allocator->Reset();
    if (SUCCEEDED(hr)) hr = commands_->Reset(frame.allocator.Get(), pipeline_.Get());
    if (FAILED(hr)) return hr;
    recording_ = true; frame.uploadOffset = 0;
    barrier(frame.target.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto handle = targetHandle();
    commands_->ClearRenderTargetView(handle, clear, 0, nullptr);
    return bindTarget();
}
HRESULT Device::bindTarget() {
    if (!recording_) return E_UNEXPECTED;
    auto handle=targetHandle();
    commands_->OMSetRenderTargets(1, &handle, FALSE, nullptr);
    D3D12_VIEWPORT viewport{0,0,static_cast<float>(width_),static_cast<float>(height_),0,1};
    D3D12_RECT rect{0,0,static_cast<LONG>(width_),static_cast<LONG>(height_)};
    commands_->RSSetViewports(1, &viewport); commands_->RSSetScissorRects(1, &rect);
    return S_OK;
}
HRESULT Device::drawTriangles(const Vertex* vertices, UINT count, const float transform[16]) {
    if (!recording_ || !vertices || !transform || !count || count % 3) return E_INVALIDARG;
    auto& frame = frames_[index_];
    const UINT64 size = static_cast<UINT64>(count) * sizeof(Vertex);
    if (size > uploadBytes - frame.uploadOffset) return E_OUTOFMEMORY;
    std::memcpy(frame.mapped + frame.uploadOffset, vertices, static_cast<size_t>(size));
    D3D12_VERTEX_BUFFER_VIEW view{frame.upload->GetGPUVirtualAddress() + frame.uploadOffset,
                                 static_cast<UINT>(size), sizeof(Vertex)};
    frame.uploadOffset += static_cast<UINT>(size);
    commands_->SetPipelineState(pipeline_.Get());
    commands_->SetGraphicsRootSignature(root_.Get());
    commands_->SetGraphicsRoot32BitConstants(0,16,transform,0);
    commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands_->IASetVertexBuffers(0,1,&view);
    commands_->DrawInstanced(count,1,0,0);
    return S_OK;
}
HRESULT Device::endFrame(UINT syncInterval, std::vector<uint8_t>* pixels) {
    if (!recording_ || syncInterval > 4) return E_INVALIDARG;
    auto& frame = frames_[index_];
    Ptr<ID3D12Resource> readback;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 size = 0;
    HRESULT hr = S_OK;
    if (pixels) {
        auto desc = frame.target->GetDesc();
        device_->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&size);
        const auto properties = heap(D3D12_HEAP_TYPE_READBACK);
        const auto resourceDesc = buffer(size);
        hr = device_->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_NONE,&resourceDesc,
                                              D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback));
        if (FAILED(hr)) return hr; // Frame remains open; caller may retry without capture.
        barrier(frame.target.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION from{}, to{};
        from.pResource = frame.target.Get(); from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        to.pResource = readback.Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        to.PlacedFootprint = footprint;
        commands_->CopyTextureRegion(&to,0,0,0,&from,nullptr);
        barrier(frame.target.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT);
    } else barrier(frame.target.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
    hr = commands_->Close(); recording_ = false;
    if (FAILED(hr)) return hr;
    ID3D12CommandList* lists[] = {commands_.Get()};
    queue_->ExecuteCommandLists(1,lists);
    // Signal even if Present fails, so allocators never reuse in-flight work.
    hr = signal(frame.fenceValue);
    if (FAILED(hr)) return hr;
    const HRESULT presentResult = swap_->Present(syncInterval,0);
    if (pixels) {
        hr = wait(frame.fenceValue);
        if (FAILED(hr)) return hr;
        void* mapped = nullptr;
        D3D12_RANGE range{0,static_cast<SIZE_T>(size)};
        hr = readback->Map(0,&range,&mapped);
        if (FAILED(hr)) return hr;
        pixels->resize(static_cast<size_t>(width_) * height_ * 4);
        for (UINT row=0; row<height_; ++row) std::memcpy(pixels->data()+static_cast<size_t>(row)*width_*4,
            static_cast<const uint8_t*>(mapped)+footprint.Offset+static_cast<size_t>(row)*footprint.Footprint.RowPitch,static_cast<size_t>(width_)*4);
        D3D12_RANGE noWrite{0,0}; readback->Unmap(0,&noWrite);
    }
    return presentResult;
}
HRESULT Device::resize(UINT width, UINT height) {
    if (!swap_ || recording_ || !width || !height) return E_INVALIDARG;
    if (width==width_ && height==height_) return S_OK;
    HRESULT hr = waitIdle();
    if (FAILED(hr)) return hr;
    for (auto& frame : frames_) frame.target.Reset();
    hr = swap_->ResizeBuffers(frameCount,width,height,DXGI_FORMAT_R8G8B8A8_UNORM,0);
    if (FAILED(hr)) { createTargets(); return hr; }
    width_ = width; height_ = height;
    return createTargets();
}
HRESULT Device::checkDebugErrors() const {
    if (!device_) return E_UNEXPECTED;
    Ptr<ID3D12InfoQueue> info;
    HRESULT hr = device_.As(&info);
    if (FAILED(hr)) return hr;
    const UINT64 count = info->GetNumStoredMessages();
    for (UINT64 i=0; i<count; ++i) {
        SIZE_T size=0; hr=info->GetMessage(i,nullptr,&size);
        if (FAILED(hr)) return hr;
        std::vector<uint8_t> bytes(size);
        auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
        hr=info->GetMessage(i,message,&size);
        if (FAILED(hr)) return hr;
        if (message->Severity==D3D12_MESSAGE_SEVERITY_ERROR || message->Severity==D3D12_MESSAGE_SEVERITY_CORRUPTION)
            return E_FAIL;
    }
    return S_OK;
}
void Device::shutdown() {
    if (recording_ && commands_) { commands_->Close(); recording_=false; }
    waitIdle();
    commands_.Reset(); pipeline_.Reset(); root_.Reset();
    for (auto& frame : frames_) {
        if (frame.mapped && frame.upload) frame.upload->Unmap(0,nullptr);
        frame.mapped=nullptr; frame.upload.Reset(); frame.target.Reset(); frame.allocator.Reset();
        frame.fenceValue=0; frame.uploadOffset=0;
    }
    targets_.Reset(); swap_.Reset(); fence_.Reset(); queue_.Reset(); device_.Reset(); factory_.Reset();
    if (event_) { CloseHandle(event_); event_=nullptr; }
    width_=height_=index_=targetStride_=0; nextFence_=1; adapterDescription_={};
}
}
