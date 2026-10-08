#pragma once
#include "NativeDlss12.h"

namespace generals_mods::native12 {
struct ScenePixel {float depth=0;float motionX=0,motionY=0;};
// Native scene color, Z and rasterized object motion. The W3D adapter must
// provide camera constants and current/previous transforms for each mesh.
class Scene {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
public:
    Scene(Device& device,Dlss& dlss):device_(device),dlss_(dlss){}
    ~Scene(){if(device_.commands())device_.waitIdle();else shutdown();}
    Scene(const Scene&)=delete;
    Scene& operator=(const Scene&)=delete;
    HRESULT initialize();
    HRESULT begin(const float clear[4]);
    HRESULT draw(const Vertex* vertices,UINT count,const float current[16],const float previous[16],sl::float2 jitter);
    HRESULT finish(const sl::Constants& camera,uint32_t frameIndex);
    // Diagnostic readback after frame submission; does not record during a frame.
    HRESULT readPixel(UINT x,UINT y,ScenePixel& pixel);
    HRESULT shutdown();
private:
    HRESULT shaders();
    void transition(ID3D12Resource* resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after);
    Device& device_;
    Dlss& dlss_;
    Ptr<ID3D12Resource> color_,depth_,motion_,z_,output_;
    Ptr<ID3D12DescriptorHeap> targets_,zView_,views_;
    Ptr<ID3D12RootSignature> sceneRoot_,blitRoot_;
    Ptr<ID3D12PipelineState> scenePso_,blitPso_;
    std::array<Ptr<ID3D12Resource>,Device::frameCount> upload_;
    std::array<uint8_t*,Device::frameCount> mapped_{};
    UINT width_=0,height_=0,offset_=0,index_=0;
    NeuralMode mode_=NeuralMode::Off;
    bool active_=false;
    static constexpr UINT uploadBytes=4*1024*1024;
};
}
