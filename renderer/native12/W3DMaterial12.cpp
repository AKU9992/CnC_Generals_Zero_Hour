#include "W3DMaterial12.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <cstring>
#include <cfloat>
#include <cstdio>
#include <cmath>

namespace generals_mods::native12 {
namespace {
struct alignas(16) Constants {
    D3DMATRIX current,previous,world,view,textureMatrix[4];
    float materialDiffuse[4],materialAmbient[4],materialEmissive[4],ambient[4],fogColor[4],fogSettings[4],options[4],outputSize[4];
    float lightDirection[8][4],lightColor[8][4],clipPlanes[6][4];
    DWORD colorOp[4][4],alphaOp[4][4],textureOptions[4][4];float textureFactor[4],bumpMatrix[4][4],bumpLuminance[4][4];
    float waterOptions[4],waterVertex[16][4],waterPixel[8][4];
    DWORD materialSources[4];float lightAmbient[8][4];
    float motionViewport[4];
    D3DMATRIX normalTransform;float materialSpecular[4],lightingCamera[4],lightingView[4];
    float lightPosition[8][4],lightAttenuation[8][4],lightCone[8][4],lightSpecular[8][4];
};
float floating(DWORD value){float f;std::memcpy(&f,&value,4);return f;}
void color(DWORD packed,float* output){output[0]=float((packed>>16)&255)/255;output[1]=float((packed>>8)&255)/255;
    output[2]=float(packed&255)/255;output[3]=float(packed>>24)/255;}
D3D12_BLEND blend(DWORD b){if(b>=1 && b<=11)return static_cast<D3D12_BLEND>(b);return D3D12_BLEND_ONE;}
D3D12_FILTER filter(const DWORD* s){
    const UINT min=s[D3DTSS_MINFILTER]==D3DTEXF_LINEAR ? 1 : 0,mag=s[D3DTSS_MAGFILTER]==D3DTEXF_LINEAR ? 1 : 0,
        mip=s[D3DTSS_MIPFILTER]==D3DTEXF_LINEAR ? 1 : 0;
    if(s[D3DTSS_MINFILTER]==D3DTEXF_ANISOTROPIC || s[D3DTSS_MAGFILTER]==D3DTEXF_ANISOTROPIC)return D3D12_FILTER_ANISOTROPIC;
    return static_cast<D3D12_FILTER>((min<<4)|(mag<<2)|mip);
}
}
Materials::~Materials(){shutdown();}
HRESULT Materials::initialize(){
    constexpr const char* source=
#include "W3DMaterialShader12.inc"
    ;
    Microsoft::WRL::ComPtr<ID3DBlob> errors,serialized;
    HRESULT hr=D3DCompile(source,std::strlen(source),"W3DMaterial12",nullptr,nullptr,"VS","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vertexShader_,&errors);
    if(SUCCEEDED(hr))hr=D3DCompile(source,std::strlen(source),"W3DMaterial12",nullptr,nullptr,"PS","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&pixelShader_,&errors);
    if(FAILED(hr)){if(errors)std::fprintf(stderr,"%s\n",static_cast<const char*>(errors->GetBufferPointer()));return hr;}
    D3D12_DESCRIPTOR_RANGE ranges[2]{};ranges[0].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;ranges[0].NumDescriptors=4;
    ranges[1].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;ranges[1].NumDescriptors=4;
    D3D12_ROOT_PARAMETER parameters[3]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    for(UINT i=0;i<2;++i){parameters[i+1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[i+1].DescriptorTable.NumDescriptorRanges=1;parameters[i+1].DescriptorTable.pDescriptorRanges=&ranges[i];
        parameters[i+1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
    D3D12_ROOT_SIGNATURE_DESC root{};root.NumParameters=3;root.pParameters=parameters;
    root.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    hr=D3D12SerializeRootSignature(&root,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&errors);
    if(SUCCEEDED(hr))hr=device_.device()->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root_));
    if(FAILED(hr))return hr;
    for(auto& frame:frames_){
        D3D12_DESCRIPTOR_HEAP_DESC d{};d.NumDescriptors=descriptorCount;d.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;d.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr=device_.device()->CreateDescriptorHeap(&d,IID_PPV_ARGS(&frame.textures));
        // Hardware shader-visible sampler heaps have a 2048 descriptor limit.
        d.NumDescriptors=2048;d.Type=D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        if(SUCCEEDED(hr))hr=device_.device()->CreateDescriptorHeap(&d,IID_PPV_ARGS(&frame.samplers));
        if(FAILED(hr))return hr;
    }
    return S_OK;
}
void Materials::beginFrame(){
    frame_=device_.swapchain()->GetCurrentBackBufferIndex();auto& frame=frames_[frame_];frame.descriptors=0;frame.page=0;frame.retained.clear();
    frame.samplerDescriptors=0;frame.samplerCache.clear();
    for(auto& page:frame.pages)page.offset=0;
}
void Materials::retain(ID3D12Resource* resource){if(resource)frames_[frame_].retained.emplace_back(resource);}
HRESULT Materials::reserve(UINT bytes,UINT alignment,uint8_t** cpu,D3D12_GPU_VIRTUAL_ADDRESS* gpu){
    auto& frame=frames_[frame_];
    if(bytes>pageBytes)return E_OUTOFMEMORY;
    for(;;){
        if(frame.page>=frame.pages.size()){
            Page page;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=pageBytes;d.Height=1;
            d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            D3D12_HEAP_PROPERTIES h{};h.Type=D3D12_HEAP_TYPE_UPLOAD;h.CreationNodeMask=h.VisibleNodeMask=1;
            HRESULT hr=device_.device()->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&page.resource));
            D3D12_RANGE noRead{0,0};if(SUCCEEDED(hr))hr=page.resource->Map(0,&noRead,reinterpret_cast<void**>(&page.mapped));
            if(FAILED(hr))return hr;frame.pages.push_back(std::move(page));
        }
        auto& page=frame.pages[frame.page];UINT offset=(page.offset+alignment-1)&~(alignment-1);
        if(bytes<=pageBytes-offset){*cpu=page.mapped+offset;*gpu=page.resource->GetGPUVirtualAddress()+offset;page.offset=offset+bytes;return S_OK;}
        ++frame.page;
    }
}
HRESULT Materials::pipeline(const MaterialState& s,D3D12_PRIMITIVE_TOPOLOGY topology,ID3D12PipelineState** result){
    constexpr DWORD states[]={D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,D3DRS_CULLMODE,D3DRS_ALPHABLENDENABLE,
        D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_STENCILENABLE,D3DRS_STENCILFAIL,D3DRS_STENCILZFAIL,D3DRS_STENCILPASS,
        D3DRS_STENCILFUNC,D3DRS_STENCILMASK,D3DRS_STENCILWRITEMASK,D3DRS_COLORWRITEENABLE,D3DRS_ZBIAS};
    std::array<DWORD,24> key{};for(UINT i=0;i<_countof(states);++i)key[i]=s.render[states[i]];
    key[16]=s.colorFormat;key[17]=s.depthFormat;key[18]=s.worldPass;
    key[19]=topology==D3D_PRIMITIVE_TOPOLOGY_LINELIST ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE :
        topology==D3D_PRIMITIVE_TOPOLOGY_POINTLIST ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    auto cached=pipelines_.find(key);if(cached!=pipelines_.end()){*result=cached->second.Get();return S_OK;}
    const D3D12_INPUT_ELEMENT_DESC layout[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"POSITION",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,44,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,60,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,76,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,92,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,0,108,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",3,DXGI_FORMAT_R32G32B32A32_FLOAT,0,124,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    static_assert(sizeof(MaterialVertex)==140);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=root_.Get();p.InputLayout={layout,_countof(layout)};
    p.VS={vertexShader_->GetBufferPointer(),vertexShader_->GetBufferSize()};p.PS={pixelShader_->GetBufferPointer(),pixelShader_->GetBufferSize()};
    p.SampleMask=UINT_MAX;p.SampleDesc.Count=1;p.PrimitiveTopologyType=static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(key[19]);
    p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=s.render[D3DRS_CULLMODE]==D3DCULL_NONE ? D3D12_CULL_MODE_NONE :
        s.render[D3DRS_CULLMODE]==D3DCULL_CW ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK;
    p.RasterizerState.DepthClipEnable=TRUE;p.RasterizerState.DepthBias=-static_cast<INT>(s.render[D3DRS_ZBIAS]);
    p.BlendState.IndependentBlendEnable=TRUE;
    for(UINT i=0;i<3;++i){auto& b=p.BlendState.RenderTarget[i];b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ONE;
        b.DestBlend=b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOp=b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.LogicOp=D3D12_LOGIC_OP_NOOP;
        b.RenderTargetWriteMask=static_cast<UINT8>(s.render[D3DRS_COLORWRITEENABLE]);}
    auto& b=p.BlendState.RenderTarget[0];b.BlendEnable=s.render[D3DRS_ALPHABLENDENABLE]!=0;
    b.SrcBlend=blend(s.render[D3DRS_SRCBLEND]);b.DestBlend=blend(s.render[D3DRS_DESTBLEND]);
    b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;
    if(b.BlendEnable || !s.render[D3DRS_ZWRITEENABLE]){p.BlendState.RenderTarget[1].RenderTargetWriteMask=0;p.BlendState.RenderTarget[2].RenderTargetWriteMask=0;}
    auto& z=p.DepthStencilState;z.DepthEnable=s.depthFormat!=DXGI_FORMAT_UNKNOWN && s.render[D3DRS_ZENABLE]!=0;
    z.DepthWriteMask=s.render[D3DRS_ZWRITEENABLE] ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    z.DepthFunc=static_cast<D3D12_COMPARISON_FUNC>(s.render[D3DRS_ZFUNC]);
    z.StencilEnable=s.depthFormat==DXGI_FORMAT_D24_UNORM_S8_UINT && s.render[D3DRS_STENCILENABLE]!=0;
    z.StencilReadMask=static_cast<UINT8>(s.render[D3DRS_STENCILMASK]);z.StencilWriteMask=static_cast<UINT8>(s.render[D3DRS_STENCILWRITEMASK]);
    z.FrontFace.StencilFailOp=static_cast<D3D12_STENCIL_OP>(s.render[D3DRS_STENCILFAIL]);
    z.FrontFace.StencilDepthFailOp=static_cast<D3D12_STENCIL_OP>(s.render[D3DRS_STENCILZFAIL]);
    z.FrontFace.StencilPassOp=static_cast<D3D12_STENCIL_OP>(s.render[D3DRS_STENCILPASS]);
    z.FrontFace.StencilFunc=static_cast<D3D12_COMPARISON_FUNC>(s.render[D3DRS_STENCILFUNC]);z.BackFace=z.FrontFace;
    p.NumRenderTargets=s.worldPass ? 3 : 1;p.RTVFormats[0]=s.colorFormat;
    if(s.worldPass){p.RTVFormats[1]=DXGI_FORMAT_R16G16_FLOAT;p.RTVFormats[2]=DXGI_FORMAT_R32_FLOAT;}
    p.DSVFormat=s.depthFormat;Ptr<ID3D12PipelineState> native;
    HRESULT hr=device_.device()->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&native));
    if(SUCCEEDED(hr)){*result=native.Get();pipelines_.emplace(key,std::move(native));}return hr;
}
HRESULT Materials::draw(const MaterialVertex* vertices,UINT count,MaterialState& s,D3D12_PRIMITIVE_TOPOLOGY topology){
    if(!device_.commands() || !vertices || !count || !s.renderWidth || !s.renderHeight)return E_INVALIDARG;
    auto& frame=frames_[frame_];if(frame.descriptors>descriptorCount-4)return E_OUTOFMEMORY;
    const UINT bytes=count*sizeof(MaterialVertex);if(UINT64(count)*sizeof(MaterialVertex)>pageBytes)return E_OUTOFMEMORY;
    uint8_t* cpu=nullptr;D3D12_GPU_VIRTUAL_ADDRESS gpu=0;HRESULT hr=reserve(bytes,4,&cpu,&gpu);if(FAILED(hr))return hr;
    std::memcpy(cpu,vertices,bytes);D3D12_VERTEX_BUFFER_VIEW vb{gpu,bytes,sizeof(MaterialVertex)};
    Constants c{};
    using namespace DirectX;XMFLOAT4X4 matrix;
    const auto world=XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&s.world));
    const auto view=XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&s.view));
    const auto projection=XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&s.projection));
    XMStoreFloat4x4(&matrix,world*view*projection);std::memcpy(&c.current,&matrix,64);c.previous=s.previousTransform;c.world=s.world;c.view=s.view;
    std::memcpy(c.textureMatrix,s.textureMatrix,sizeof(c.textureMatrix));
    std::memcpy(c.materialDiffuse,&s.material.Diffuse,16);std::memcpy(c.materialAmbient,&s.material.Ambient,16);std::memcpy(c.materialEmissive,&s.material.Emissive,16);
    color(s.render[D3DRS_AMBIENT],c.ambient);color(s.render[D3DRS_FOGCOLOR],c.fogColor);c.fogColor[3]=float(s.render[D3DRS_ALPHAREF]&255)/255;
    c.fogSettings[0]=floating(s.render[D3DRS_FOGSTART]);c.fogSettings[1]=floating(s.render[D3DRS_FOGEND]);c.fogSettings[2]=floating(s.render[D3DRS_FOGDENSITY]);
    c.fogSettings[3]=s.render[D3DRS_FOGENABLE] ? float(s.render[D3DRS_FOGTABLEMODE] ? s.render[D3DRS_FOGTABLEMODE] : s.render[D3DRS_FOGVERTEXMODE]) : 0;
    c.options[0]=s.pretransformed;c.options[1]=s.render[D3DRS_LIGHTING]!=0;c.options[2]=s.render[D3DRS_ALPHATESTENABLE] ? float(s.render[D3DRS_ALPHAFUNC]) : 0;
    c.outputSize[0]=s.pretransformed ? 0 : s.jitter[0];c.outputSize[1]=s.pretransformed ? 0 : s.jitter[1];
    c.outputSize[2]=float(s.renderWidth);c.outputSize[3]=float(s.renderHeight);
    c.motionViewport[0]=c.motionViewport[1]=1;
    if(s.worldPass){c.motionViewport[0]=float(s.viewport.Width)/device_.width();c.motionViewport[1]=float(s.viewport.Height)/device_.height();
        c.outputSize[2]*=c.motionViewport[0];c.outputSize[3]*=c.motionViewport[1];}
    for(UINT i=0;i<8;++i){std::memcpy(c.lightDirection[i],&s.lights[i].Direction,12);if(s.lightEnabled[i])std::memcpy(c.lightColor[i],&s.lights[i].Diffuse,16);}
    for(UINT i=0;i<6;++i){if(s.render[D3DRS_CLIPPLANEENABLE]&(1u<<i))std::memcpy(c.clipPlanes[i],s.clipPlanes[i],16);else c.clipPlanes[i][3]=FLT_MAX;}
    color(s.render[D3DRS_TEXTUREFACTOR],c.textureFactor);
    c.waterOptions[0]=float(s.waterShader);c.waterOptions[1]=s.waveVertex ? 1.f : 0.f;
    std::memcpy(c.waterVertex,s.vertexConstants,sizeof(c.waterVertex));std::memcpy(c.waterPixel,s.pixelConstants,sizeof(c.waterPixel));
    c.materialSources[0]=s.render[D3DRS_COLORVERTEX] ? s.render[D3DRS_DIFFUSEMATERIALSOURCE] : D3DMCS_MATERIAL;
    c.materialSources[1]=s.render[D3DRS_COLORVERTEX] ? s.render[D3DRS_AMBIENTMATERIALSOURCE] : D3DMCS_MATERIAL;
    c.materialSources[2]=s.render[D3DRS_COLORVERTEX] ? s.render[D3DRS_EMISSIVEMATERIALSOURCE] : D3DMCS_MATERIAL;
    c.materialSources[3]=(s.render[D3DRS_COLORVERTEX] ? s.render[D3DRS_SPECULARMATERIALSOURCE] : D3DMCS_MATERIAL)|(s.render[D3DRS_SPECULARENABLE] ? 256u : 0u);
    for(UINT i=0;i<8;++i)if(s.lightEnabled[i])std::memcpy(c.lightAmbient[i],&s.lights[i].Ambient,16);
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&c.normalTransform),XMMatrixTranspose(XMMatrixInverse(nullptr,world)));
    std::memcpy(c.materialSpecular,&s.material.Specular,16);
    XMFLOAT4X4 inverseView;XMStoreFloat4x4(&inverseView,XMMatrixInverse(nullptr,view));
    c.lightingCamera[0]=inverseView._41;c.lightingCamera[1]=inverseView._42;c.lightingCamera[2]=inverseView._43;c.lightingCamera[3]=s.material.Power;
    c.lightingView[0]=inverseView._31;c.lightingView[1]=inverseView._32;c.lightingView[2]=inverseView._33;c.lightingView[3]=float(s.render[D3DRS_LOCALVIEWER]!=0);
    for(UINT i=0;i<8;++i){const auto& light=s.lights[i];c.lightDirection[i][3]=s.lightEnabled[i] ? float(light.Type) : 0;
        std::memcpy(c.lightPosition[i],&light.Position,12);c.lightPosition[i][3]=light.Range;
        c.lightAttenuation[i][0]=light.Attenuation0;c.lightAttenuation[i][1]=light.Attenuation1;c.lightAttenuation[i][2]=light.Attenuation2;c.lightAttenuation[i][3]=light.Falloff;
        c.lightCone[i][0]=std::cos(light.Theta*.5f);c.lightCone[i][1]=std::cos(light.Phi*.5f);if(s.lightEnabled[i])std::memcpy(c.lightSpecular[i],&light.Specular,16);
    }
    auto textureCpu=frame.textures->GetCPUDescriptorHandleForHeapStart();auto samplerCpu=frame.samplers->GetCPUDescriptorHandleForHeapStart();
    const UINT textureStride=device_.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const UINT samplerStride=device_.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    textureCpu.ptr+=SIZE_T(frame.descriptors)*textureStride;
    for(UINT i=0;i<4;++i){
        const DWORD* t=s.stage[i];c.colorOp[i][0]=t[D3DTSS_COLOROP];c.colorOp[i][1]=t[D3DTSS_COLORARG1];c.colorOp[i][2]=t[D3DTSS_COLORARG2];
        c.alphaOp[i][0]=t[D3DTSS_ALPHAOP];c.alphaOp[i][1]=t[D3DTSS_ALPHAARG1];c.alphaOp[i][2]=t[D3DTSS_ALPHAARG2];
        c.textureOptions[i][0]=t[D3DTSS_TEXCOORDINDEX];c.textureOptions[i][1]=t[D3DTSS_TEXTURETRANSFORMFLAGS];
        c.bumpMatrix[i][0]=floating(t[D3DTSS_BUMPENVMAT00]);c.bumpMatrix[i][1]=floating(t[D3DTSS_BUMPENVMAT01]);
        c.bumpMatrix[i][2]=floating(t[D3DTSS_BUMPENVMAT10]);c.bumpMatrix[i][3]=floating(t[D3DTSS_BUMPENVMAT11]);
        c.bumpLuminance[i][0]=floating(t[D3DTSS_BUMPENVLSCALE]);c.bumpLuminance[i][1]=floating(t[D3DTSS_BUMPENVLOFFSET]);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;
        srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        ID3D12Resource* resource=nullptr;
        if(s.textures[i]){hr=uploadTexture(device_,*s.textures[i]);if(FAILED(hr))return hr;
            textureBarrier(device_.commands(),*s.textures[i],D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            resource=s.textures[i]->resource.Get();srv.Format=s.textures[i]->format;srv.Texture2D.MipLevels=static_cast<UINT>(s.textures[i]->levels.size());
            srv.Shader4ComponentMapping=s.textures[i]->componentMapping;c.textureOptions[i][2]=1;frame.retained.push_back(s.textures[i]->resource);}
        device_.device()->CreateShaderResourceView(resource,&srv,textureCpu);textureCpu.ptr+=textureStride;
    }
    constexpr DWORD samplerStates[]={D3DTSS_ADDRESSU,D3DTSS_ADDRESSV,D3DTSS_ADDRESSW,D3DTSS_MINFILTER,D3DTSS_MAGFILTER,
        D3DTSS_MIPFILTER,D3DTSS_MIPMAPLODBIAS,D3DTSS_MAXMIPLEVEL,D3DTSS_MAXANISOTROPY,D3DTSS_BORDERCOLOR};
    std::array<DWORD,40> samplerKey{};
    for(UINT i=0;i<4;++i)for(UINT j=0;j<10;++j)samplerKey[i*10+j]=s.stage[i][samplerStates[j]];
    auto samplerFound=frame.samplerCache.find(samplerKey);UINT samplerOffset=0;
    if(samplerFound!=frame.samplerCache.end())samplerOffset=samplerFound->second;
    else{
        if(frame.samplerDescriptors>2048-4)return E_OUTOFMEMORY;
        samplerOffset=frame.samplerDescriptors;frame.samplerDescriptors+=4;
        for(UINT i=0;i<4;++i){const auto* t=s.stage[i];D3D12_SAMPLER_DESC sampler{};sampler.Filter=filter(t);
            sampler.AddressU=static_cast<D3D12_TEXTURE_ADDRESS_MODE>(t[D3DTSS_ADDRESSU]);
            sampler.AddressV=static_cast<D3D12_TEXTURE_ADDRESS_MODE>(t[D3DTSS_ADDRESSV]);
            sampler.AddressW=static_cast<D3D12_TEXTURE_ADDRESS_MODE>(t[D3DTSS_ADDRESSW]);
            sampler.MipLODBias=floating(t[D3DTSS_MIPMAPLODBIAS]);sampler.MinLOD=float(t[D3DTSS_MAXMIPLEVEL]);
            sampler.MaxLOD=t[D3DTSS_MIPFILTER]==D3DTEXF_NONE ? sampler.MinLOD : D3D12_FLOAT32_MAX;
            sampler.MaxAnisotropy=std::max<DWORD>(1,std::min<DWORD>(16,t[D3DTSS_MAXANISOTROPY]));sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
            color(t[D3DTSS_BORDERCOLOR],sampler.BorderColor);auto h=samplerCpu;h.ptr+=SIZE_T(samplerOffset+i)*samplerStride;
            device_.device()->CreateSampler(&sampler,h);
        }
        frame.samplerCache.emplace(samplerKey,samplerOffset);
    }
    hr=reserve(sizeof(c),256,&cpu,&gpu);if(FAILED(hr))return hr;std::memcpy(cpu,&c,sizeof(c));
    ID3D12PipelineState* pso=nullptr;hr=pipeline(s,topology,&pso);if(FAILED(hr))return hr;
    auto* commands=device_.commands();ID3D12DescriptorHeap* heaps[]={frame.textures.Get(),frame.samplers.Get()};commands->SetDescriptorHeaps(2,heaps);
    commands->SetGraphicsRootSignature(root_.Get());commands->SetPipelineState(pso);commands->SetGraphicsRootConstantBufferView(0,gpu);
    auto textures=frame.textures->GetGPUDescriptorHandleForHeapStart();textures.ptr+=UINT64(frame.descriptors)*textureStride;
    auto samplers=frame.samplers->GetGPUDescriptorHandleForHeapStart();samplers.ptr+=UINT64(samplerOffset)*samplerStride;
    commands->SetGraphicsRootDescriptorTable(1,textures);commands->SetGraphicsRootDescriptorTable(2,samplers);frame.descriptors+=4;
    commands->OMSetStencilRef(s.render[D3DRS_STENCILREF]);commands->IASetVertexBuffers(0,1,&vb);commands->IASetPrimitiveTopology(topology);
    commands->DrawInstanced(count,1,0,0);return S_OK;
}
void Materials::shutdown(){
    device_.waitIdle();pipelines_.clear();root_.Reset();vertexShader_.Reset();pixelShader_.Reset();
    for(auto& frame:frames_){frame.retained.clear();for(auto& page:frame.pages){if(page.mapped)page.resource->Unmap(0,nullptr);}frame.pages.clear();frame.textures.Reset();frame.samplers.Reset();}
}
}
