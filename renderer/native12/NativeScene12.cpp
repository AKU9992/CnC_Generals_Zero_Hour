#include "NativeScene12.h"
#include <d3dcompiler.h>
#include <cstring>
#include <DirectXPackedVector.h>

namespace generals_mods::native12 {
namespace {
D3D12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE type){
    D3D12_HEAP_PROPERTIES h{};h.Type=type;h.CreationNodeMask=h.VisibleNodeMask=1;return h;
}
HRESULT compile(const char* source,const char* entry,const char* target,ID3DBlob** blob){
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    return D3DCompile(source,std::strlen(source),"NativeScene12",nullptr,nullptr,entry,target,
        D3DCOMPILE_ENABLE_STRICTNESS,0,blob,&error);
}
void pipelineDefaults(D3D12_GRAPHICS_PIPELINE_STATE_DESC& pso){
    pso.SampleMask=UINT_MAX;pso.SampleDesc.Count=1;
    pso.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pso.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable=TRUE;
    pso.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pso.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;
    pso.BlendState.IndependentBlendEnable=TRUE;
    for(auto& b:pso.BlendState.RenderTarget){
        b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlend=b.DestBlendAlpha=D3D12_BLEND_ZERO;
        b.BlendOp=b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.LogicOp=D3D12_LOGIC_OP_NOOP;
        b.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    }
}
}
HRESULT Scene::initialize(){
    if(device_.commands() || color_ || !device_.device()) return E_UNEXPECTED;
    width_=dlss_.renderWidth();height_=dlss_.renderHeight();mode_=dlss_.mode();
    if(!width_ || !height_) return E_INVALIDARG;
    auto* device=device_.device();
    D3D12_DESCRIPTOR_HEAP_DESC heap{};heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;heap.NumDescriptors=3;
    HRESULT hr=device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&targets_));
    heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;heap.NumDescriptors=1;
    if(SUCCEEDED(hr)) hr=device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&zView_));
    heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;heap.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if(SUCCEEDED(hr)) hr=device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&views_));
    if(FAILED(hr)) return hr;
    const auto gpu=properties(D3D12_HEAP_TYPE_DEFAULT);
    auto texture=[&](DXGI_FORMAT format,UINT width,UINT height,D3D12_RESOURCE_FLAGS flags,
                     D3D12_RESOURCE_STATES state,ID3D12Resource** resource){
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width=width;d.Height=height;d.DepthOrArraySize=d.MipLevels=1;d.Format=format;
        d.SampleDesc.Count=1;d.Flags=flags;
        return device->CreateCommittedResource(&gpu,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(resource));
    };
    hr=texture(DXGI_FORMAT_R16G16B16A16_FLOAT,width_,height_,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_COMMON,&color_);
    if(SUCCEEDED(hr)) hr=texture(DXGI_FORMAT_R16G16_FLOAT,width_,height_,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_COMMON,&motion_);
    if(SUCCEEDED(hr)) hr=texture(DXGI_FORMAT_R32_FLOAT,width_,height_,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_COMMON,&depth_);
    if(SUCCEEDED(hr)) hr=texture(DXGI_FORMAT_D32_FLOAT,width_,height_,D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,D3D12_RESOURCE_STATE_DEPTH_WRITE,&z_);
    if(SUCCEEDED(hr)) hr=texture(DXGI_FORMAT_R16G16B16A16_FLOAT,device_.width(),device_.height(),D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON,&output_);
    if(FAILED(hr)) return hr;
    auto handle=targets_->GetCPUDescriptorHandleForHeapStart();
    const UINT stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for(auto* resource:{color_.Get(),motion_.Get(),depth_.Get()}){
        device->CreateRenderTargetView(resource,nullptr,handle);handle.ptr+=stride;
    }
    device->CreateDepthStencilView(z_.Get(),nullptr,zView_->GetCPUDescriptorHandleForHeapStart());
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels=1;
    device->CreateShaderResourceView(mode_==NeuralMode::Off ? color_.Get() : output_.Get(),&srv,views_->GetCPUDescriptorHandleForHeapStart());
    const auto cpu=properties(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC b{};b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;b.Width=uploadBytes;
    b.Height=1;b.DepthOrArraySize=b.MipLevels=1;b.SampleDesc.Count=1;b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    for(UINT i=0;i<Device::frameCount;++i){
        hr=device->CreateCommittedResource(&cpu,D3D12_HEAP_FLAG_NONE,&b,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload_[i]));
        D3D12_RANGE noRead{0,0};
        if(SUCCEEDED(hr)) hr=upload_[i]->Map(0,&noRead,reinterpret_cast<void**>(&mapped_[i]));
        if(FAILED(hr)) return hr;
    }
    return shaders();
}
HRESULT Scene::shaders(){
    constexpr const char* scene=R"(
cbuffer Matrices:register(b0){row_major float4x4 current;row_major float4x4 previous;float2 jitter;float2 size;}
struct Input{float3 position:POSITION;float4 color:COLOR;};
struct Varying{float4 position:SV_POSITION;float4 color:COLOR;float4 current:TEXCOORD0;float4 previous:TEXCOORD1;};
Varying VS(Input i){Varying o;o.current=mul(float4(i.position,1),current);o.previous=mul(float4(i.position,1),previous);
o.position=o.current;o.position.xy+=jitter*float2(2,-2)/size*o.position.w;o.color=i.color;return o;}
struct Targets{float4 color:SV_TARGET0;float2 motion:SV_TARGET1;float depth:SV_TARGET2;};
Targets PS(Varying i){Targets o;o.color=i.color;
o.motion=(i.previous.xy/i.previous.w-i.current.xy/i.current.w)*float2(.5,-.5);o.depth=i.position.z;return o;}
)";
    constexpr const char* blit=R"(
Texture2D<float4> color:register(t0);
struct Varying{float4 position:SV_POSITION;};
Varying VS(uint i:SV_VertexID){Varying o;float2 xy=float2((i<<1)&2,i&2);o.position=float4(xy*float2(2,-2)+float2(-1,1),0,1);return o;}
float4 PS(Varying i):SV_TARGET{return color.Load(int3(int2(i.position.xy),0));}
)";
    Ptr<ID3DBlob> vs,ps,root,error;
    HRESULT hr=compile(scene,"VS","vs_5_0",&vs);
    if(SUCCEEDED(hr)) hr=compile(scene,"PS","ps_5_0",&ps);
    if(FAILED(hr)) return hr;
    D3D12_ROOT_PARAMETER constant{};constant.ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constant.Constants.Num32BitValues=36;constant.ShaderVisibility=D3D12_SHADER_VISIBILITY_VERTEX;
    D3D12_ROOT_SIGNATURE_DESC signature{};signature.NumParameters=1;signature.pParameters=&constant;
    signature.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    hr=D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&root,&error);
    if(SUCCEEDED(hr)) hr=device_.device()->CreateRootSignature(0,root->GetBufferPointer(),root->GetBufferSize(),IID_PPV_ARGS(&sceneRoot_));
    if(FAILED(hr)) return hr;
    const D3D12_INPUT_ELEMENT_DESC layout[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};pipelineDefaults(pso);
    pso.pRootSignature=sceneRoot_.Get();pso.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pso.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    pso.InputLayout={layout,_countof(layout)};pso.DepthStencilState.DepthEnable=TRUE;pso.DSVFormat=DXGI_FORMAT_D32_FLOAT;
    pso.NumRenderTargets=3;pso.RTVFormats[0]=DXGI_FORMAT_R16G16B16A16_FLOAT;
    pso.RTVFormats[1]=DXGI_FORMAT_R16G16_FLOAT;pso.RTVFormats[2]=DXGI_FORMAT_R32_FLOAT;
    hr=device_.device()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&scenePso_));
    if(FAILED(hr)) return hr;
    D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=1;
    D3D12_ROOT_PARAMETER table{};table.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    table.DescriptorTable.NumDescriptorRanges=1;table.DescriptorTable.pDescriptorRanges=&range;table.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    signature.pParameters=&table;
    hr=D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&root,&error);
    if(SUCCEEDED(hr)) hr=device_.device()->CreateRootSignature(0,root->GetBufferPointer(),root->GetBufferSize(),IID_PPV_ARGS(&blitRoot_));
    if(SUCCEEDED(hr)) hr=compile(blit,"VS","vs_5_0",&vs);
    if(SUCCEEDED(hr)) hr=compile(blit,"PS","ps_5_0",&ps);
    if(FAILED(hr)) return hr;
    pso={};pipelineDefaults(pso);pso.pRootSignature=blitRoot_.Get();
    pso.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pso.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    pso.NumRenderTargets=1;pso.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;
    return device_.device()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&blitPso_));
}
void Scene::transition(ID3D12Resource* r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource=r;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore=before;b.Transition.StateAfter=after;device_.commands()->ResourceBarrier(1,&b);
}
HRESULT Scene::begin(const float clear[4]){
    if(active_ || !device_.commands() || !scenePso_ || !clear) return E_UNEXPECTED;
    if(mode_!=dlss_.mode() || width_!=dlss_.renderWidth() || height_!=dlss_.renderHeight()) return E_INVALIDARG;
    auto outputDesc=output_->GetDesc();
    if(outputDesc.Width!=device_.width() || outputDesc.Height!=device_.height()) return E_INVALIDARG;
    active_=true;offset_=0;index_=device_.swapchain()->GetCurrentBackBufferIndex();
    auto* commands=device_.commands();
    for(auto* r:{color_.Get(),motion_.Get(),depth_.Get()}) transition(r,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto handle=targets_->GetCPUDescriptorHandleForHeapStart();const UINT stride=device_.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    commands->ClearRenderTargetView(handle,clear,0,nullptr);handle.ptr+=stride;
    const float zero[4]={0,0,0,0},farDepth[4]={1,0,0,0};
    commands->ClearRenderTargetView(handle,zero,0,nullptr);handle.ptr+=stride;commands->ClearRenderTargetView(handle,farDepth,0,nullptr);
    auto depth=zView_->GetCPUDescriptorHandleForHeapStart();commands->ClearDepthStencilView(depth,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);
    handle=targets_->GetCPUDescriptorHandleForHeapStart();commands->OMSetRenderTargets(3,&handle,TRUE,&depth);
    D3D12_VIEWPORT viewport{0,0,float(width_),float(height_),0,1};D3D12_RECT scissor{0,0,LONG(width_),LONG(height_)};
    commands->RSSetViewports(1,&viewport);commands->RSSetScissorRects(1,&scissor);
    return S_OK;
}
HRESULT Scene::draw(const Vertex* vertices,UINT count,const float current[16],const float previous[16],sl::float2 jitter){
    if(!active_ || !device_.commands() || !vertices || !current || !previous || !count || count%3) return E_INVALIDARG;
    const UINT64 bytes=UINT64(count)*sizeof(Vertex);if(bytes>uploadBytes-offset_) return E_OUTOFMEMORY;
    std::memcpy(mapped_[index_]+offset_,vertices,static_cast<size_t>(bytes));
    D3D12_VERTEX_BUFFER_VIEW vb{upload_[index_]->GetGPUVirtualAddress()+offset_,static_cast<UINT>(bytes),sizeof(Vertex)};
    offset_+=static_cast<UINT>(bytes);
    struct Constants{float current[16],previous[16];sl::float2 jitter,size;} constants{};
    static_assert(sizeof(Constants)==36*4);
    std::memcpy(constants.current,current,64);std::memcpy(constants.previous,previous,64);
    constants.jitter=jitter;constants.size={float(width_),float(height_)};
    auto* commands=device_.commands();commands->SetPipelineState(scenePso_.Get());commands->SetGraphicsRootSignature(sceneRoot_.Get());
    commands->SetGraphicsRoot32BitConstants(0,36,&constants,0);commands->IASetVertexBuffers(0,1,&vb);
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);commands->DrawInstanced(count,1,0,0);return S_OK;
}
HRESULT Scene::finish(const sl::Constants& camera,uint32_t frameIndex){
    if(!active_ || !device_.commands()) return E_UNEXPECTED;
    auto* commands=device_.commands();ID3D12Resource* source=color_.Get();HRESULT hr=S_OK;
    if(mode_!=NeuralMode::Off){
        for(auto* r:{color_.Get(),motion_.Get(),depth_.Get()}) transition(r,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        transition(output_.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        DlaaFrame frame{};frame.sceneColor=color_.Get();frame.depth=depth_.Get();frame.motionVectors=motion_.Get();frame.outputColor=output_.Get();
        frame.commands=commands;frame.constants=camera;frame.frameIndex=frameIndex;
        frame.sceneColorState=frame.depthState=frame.motionVectorsState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        frame.outputColorState=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;hr=dlss_.evaluate(frame);
        transition(output_.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        source=output_.Get();
        for(auto* r:{color_.Get(),motion_.Get(),depth_.Get()}) transition(r,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
    }else{
        transition(color_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        for(auto* r:{motion_.Get(),depth_.Get()}) transition(r,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COMMON);
    }
    device_.bindTarget();
    if(SUCCEEDED(hr)){
        ID3D12DescriptorHeap* heaps[]={views_.Get()};commands->SetDescriptorHeaps(1,heaps);
        commands->SetPipelineState(blitPso_.Get());commands->SetGraphicsRootSignature(blitRoot_.Get());
        commands->SetGraphicsRootDescriptorTable(0,views_->GetGPUDescriptorHandleForHeapStart());
        commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);commands->DrawInstanced(3,1,0,0);
    }
    transition(source,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);active_=false;
    return hr;
}
HRESULT Scene::shutdown(){
    if(device_.commands()) return E_UNEXPECTED;
    HRESULT hr=device_.waitIdle();if(FAILED(hr)) return hr;
    for(UINT i=0;i<Device::frameCount;++i){if(mapped_[i] && upload_[i]) upload_[i]->Unmap(0,nullptr);mapped_[i]=nullptr;upload_[i].Reset();}
    color_.Reset();depth_.Reset();motion_.Reset();z_.Reset();output_.Reset();targets_.Reset();zView_.Reset();views_.Reset();
    scenePso_.Reset();blitPso_.Reset();sceneRoot_.Reset();blitRoot_.Reset();width_=height_=offset_=0;active_=false;return S_OK;
}
HRESULT Scene::readPixel(UINT x,UINT y,ScenePixel& pixel){
    if(active_ || device_.commands() || !motion_ || x>=width_ || y>=height_) return E_INVALIDARG;
    Ptr<ID3D12CommandAllocator> allocator;Ptr<ID3D12GraphicsCommandList> commands;Ptr<ID3D12Resource> readback;
    auto* device=device_.device();
    HRESULT hr=device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator));
    if(SUCCEEDED(hr)) hr=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands));
    const auto cpu=properties(D3D12_HEAP_TYPE_READBACK);
    D3D12_RESOURCE_DESC b{};b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;b.Width=768;b.Height=1;
    b.DepthOrArraySize=b.MipLevels=1;b.SampleDesc.Count=1;b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if(SUCCEEDED(hr)) hr=device->CreateCommittedResource(&cpu,D3D12_HEAP_FLAG_NONE,&b,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback));
    if(FAILED(hr)) return hr;
    ID3D12Resource* inputs[]={motion_.Get(),depth_.Get()};const DXGI_FORMAT formats[]={DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R32_FLOAT};
    for(UINT i=0;i<2;++i){
        D3D12_RESOURCE_BARRIER transition{};transition.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transition.Transition.pResource=inputs[i];transition.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        transition.Transition.StateBefore=D3D12_RESOURCE_STATE_COMMON;transition.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
        commands->ResourceBarrier(1,&transition);
        D3D12_TEXTURE_COPY_LOCATION source{},destination{};source.pResource=inputs[i];source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.pResource=readback.Get();destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint.Offset=UINT64(i)*512;destination.PlacedFootprint.Footprint={formats[i],1,1,1,256};
        const D3D12_BOX box{x,y,0,x+1,y+1,1};commands->CopyTextureRegion(&destination,0,0,0,&source,&box);
        transition.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;transition.Transition.StateAfter=D3D12_RESOURCE_STATE_COMMON;
        commands->ResourceBarrier(1,&transition);
    }
    hr=commands->Close();if(FAILED(hr)) return hr;
    ID3D12CommandList* lists[]={commands.Get()};device_.queue()->ExecuteCommandLists(1,lists);
    hr=device_.waitIdle();if(FAILED(hr)) return hr;
    void* data=nullptr;D3D12_RANGE readRange{0,768};hr=readback->Map(0,&readRange,&data);if(FAILED(hr)) return hr;
    const auto* half=static_cast<const DirectX::PackedVector::HALF*>(data);
    pixel.motionX=DirectX::PackedVector::XMConvertHalfToFloat(half[0]);pixel.motionY=DirectX::PackedVector::XMConvertHalfToFloat(half[1]);
    std::memcpy(&pixel.depth,static_cast<const uint8_t*>(data)+512,4);
    D3D12_RANGE noWrite{0,0};readback->Unmap(0,&noWrite);return S_OK;
}
}
