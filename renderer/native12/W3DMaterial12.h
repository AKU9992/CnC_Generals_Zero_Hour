#pragma once
#include <d3d8.h>
#include "W3DResources12.h"
#include <map>

namespace generals_mods::native12 {
struct MaterialVertex {
    float position[4]={0,0,0,1},previous[4]={0,0,0,1},normal[3]={0,0,1};
    float color[4]={1,1,1,1},specular[4]={0,0,0,0};
    float uv[4][4]{};
};
struct MaterialState {
    D3DMATRIX world{},view{},projection{},previousTransform{},textureMatrix[4]{};
    D3DMATERIAL8 material{};D3DLIGHT8 lights[8]{};BOOL lightEnabled[8]{};
    DWORD render[256]{},stage[4][32]{};
    float clipPlanes[6][4]{};
    // Native replacements for W3D water programs (wave, river, reflection, grid).
    UINT waterShader=0;bool waveVertex=false;float vertexConstants[16][4]{},pixelConstants[8][4]{};
    std::shared_ptr<TextureData> textures[4];
    D3DVIEWPORT8 viewport{};
    float jitter[2]{};UINT renderWidth=0,renderHeight=0;
    DXGI_FORMAT colorFormat=DXGI_FORMAT_R8G8B8A8_UNORM,depthFormat=DXGI_FORMAT_UNKNOWN;
    bool worldPass=false,pretransformed=false;
};
class Materials {
    template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
public:
    explicit Materials(Device& device):device_(device){}
    ~Materials();
    HRESULT initialize();
    void beginFrame(); // after Device::beginFrame has waited on this frame's allocator
    void retain(ID3D12Resource* resource); // bound targets live until the frame fence
    HRESULT draw(const MaterialVertex* vertices,UINT count,MaterialState& state,D3D12_PRIMITIVE_TOPOLOGY topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    void shutdown();
private:
    struct Page {Ptr<ID3D12Resource> resource;uint8_t* mapped=nullptr;UINT offset=0;};
    struct Frame {
        std::vector<Page> pages;UINT page=0,descriptors=0,samplerDescriptors=0;
        std::map<std::array<DWORD,40>,UINT> samplerCache;
        Ptr<ID3D12DescriptorHeap> textures,samplers;
        std::vector<Ptr<ID3D12Resource>> retained;
    };
    HRESULT reserve(UINT bytes,UINT alignment,uint8_t** cpu,D3D12_GPU_VIRTUAL_ADDRESS* gpu);
    HRESULT pipeline(const MaterialState& state,D3D12_PRIMITIVE_TOPOLOGY topology,ID3D12PipelineState** result);
    Device& device_;std::array<Frame,Device::frameCount> frames_;UINT frame_=0;
    Ptr<ID3D12RootSignature> root_;Ptr<ID3DBlob> vertexShader_,pixelShader_;
    std::map<std::array<DWORD,24>,Ptr<ID3D12PipelineState>> pipelines_;
    static constexpr UINT descriptorCount=16384,pageBytes=4*1024*1024;
};
}
