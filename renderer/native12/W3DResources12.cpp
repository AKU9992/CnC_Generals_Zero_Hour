#include "W3DResources12.h"
#include <cstring>

namespace generals_mods::native12 {
using Microsoft::WRL::ComPtr;
static D3D12_RESOURCE_DESC textureDesc(const TextureData& texture){
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=texture.width;desc.Height=texture.height;
    desc.DepthOrArraySize=1;desc.MipLevels=static_cast<UINT16>(texture.levels.size());
    desc.Format=texture.format;desc.SampleDesc.Count=1;
    desc.Flags=texture.depthStencil ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL :
        texture.renderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;
    return desc;
}
static D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type){
    D3D12_HEAP_PROPERTIES p{};p.Type=type;p.CreationNodeMask=p.VisibleNodeMask=1;return p;
}
HRESULT createTarget(Device& renderer,TextureData& texture){
    if(!texture.width || !texture.height || texture.levels.empty() || texture.format==DXGI_FORMAT_UNKNOWN)return E_INVALIDARG;
    const auto gpu=heap(D3D12_HEAP_TYPE_DEFAULT);const auto desc=textureDesc(texture);
    texture.state=texture.depthStencil ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COMMON;
    return renderer.device()->CreateCommittedResource(&gpu,D3D12_HEAP_FLAG_NONE,&desc,texture.state,nullptr,IID_PPV_ARGS(&texture.resource));
}
void textureBarrier(ID3D12GraphicsCommandList* commands,TextureData& texture,D3D12_RESOURCE_STATES state){
    if(texture.state==state)return;
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=texture.resource.Get();
    b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;b.Transition.StateBefore=texture.state;b.Transition.StateAfter=state;
    commands->ResourceBarrier(1,&b);texture.state=state;
}
HRESULT uploadTexture(Device& renderer,TextureData& texture){
    if(!texture.dirty && texture.resource)return S_OK;
    if(texture.renderTarget || texture.depthStencil)return texture.resource ? S_OK : createTarget(renderer,texture);
    if(!texture.width || !texture.height || texture.levels.empty() || texture.levels.size()>15)return E_INVALIDARG;
    // Uploads are ordered on the same direct queue before the pending render list.
    // Replacement resources let previously recorded draws retain their old data.
    auto* device=renderer.device();const auto desc=textureDesc(texture);const auto gpu=heap(D3D12_HEAP_TYPE_DEFAULT);
    ComPtr<ID3D12Resource> target,upload;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> commands;
    HRESULT hr=device->CreateCommittedResource(&gpu,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&target));
    const UINT count=static_cast<UINT>(texture.levels.size());
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(count);std::vector<UINT> rows(count);std::vector<UINT64> rowSizes(count);
    UINT64 size=0;device->GetCopyableFootprints(&desc,0,count,0,footprints.data(),rows.data(),rowSizes.data(),&size);
    D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=size;buffer.Height=1;
    buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    const auto cpu=heap(D3D12_HEAP_TYPE_UPLOAD);
    if(SUCCEEDED(hr))hr=device->CreateCommittedResource(&cpu,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload));
    const bool inFrame=renderer.commands()!=nullptr;
    if(!inFrame && SUCCEEDED(hr))hr=device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator));
    if(!inFrame && SUCCEEDED(hr))hr=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands));
    if(FAILED(hr))return hr;
    auto* copyCommands=inFrame ? renderer.commands() : commands.Get();
    void* mapped=nullptr;D3D12_RANGE noRead{0,0};hr=upload->Map(0,&noRead,&mapped);if(FAILED(hr))return hr;
    for(UINT i=0;i<count;++i){
        const auto& level=texture.levels[i];
        if(level.rowBytes<rowSizes[i] || level.rows<rows[i] || level.bytes.size()<UINT64(level.rowBytes)*level.rows){upload->Unmap(0,nullptr);return E_INVALIDARG;}
        for(UINT row=0;row<rows[i];++row)std::memcpy(static_cast<uint8_t*>(mapped)+footprints[i].Offset+UINT64(row)*footprints[i].Footprint.RowPitch,
            level.bytes.data()+UINT64(row)*level.rowBytes,static_cast<size_t>(rowSizes[i]));
        D3D12_TEXTURE_COPY_LOCATION source{},destination{};source.pResource=upload.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint=footprints[i];
        destination.pResource=target.Get();destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;destination.SubresourceIndex=i;
        copyCommands->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
    }
    upload->Unmap(0,nullptr);
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=target.Get();
    b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;b.Transition.StateAfter=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    copyCommands->ResourceBarrier(1,&b);
    if(inFrame){
        // Copies and consuming draws share one command list. Immutable targets
        // preserve earlier draws; upload memory lives until this frame's fence.
        renderer.retainUpload(upload.Get());renderer.retainUpload(target.Get());
        texture.resource=target;texture.state=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;texture.dirty=false;return S_OK;
    }
    hr=commands->Close();if(FAILED(hr))return hr;
    ID3D12CommandList* lists[]={commands.Get()};renderer.queue()->ExecuteCommandLists(1,lists);
    hr=renderer.waitIdle();if(FAILED(hr))return hr;
    texture.resource=target;texture.state=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;texture.dirty=false;return S_OK;
}
}
