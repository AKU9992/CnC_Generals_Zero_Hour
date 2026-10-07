#pragma once
#include <d3d9.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cstdio>

namespace generals_mods {
struct VertexShadow {
    std::vector<unsigned char> bytes;
    void* locked = nullptr;
    UINT offset = 0, length = 0;
};
inline std::unordered_map<IDirect3DVertexBuffer9*, VertexShadow>& vertexShadows() {
    static std::unordered_map<IDirect3DVertexBuffer9*, VertexShadow> data; return data;
}
inline void captureVertexLock(IDirect3DVertexBuffer9* buffer, UINT offset, UINT length, void* data) {
    if (!data) return;
    D3DVERTEXBUFFER_DESC desc{};
    if (FAILED(buffer->GetDesc(&desc)) || offset > desc.Size || length > desc.Size-offset) return;
    auto& shadow = vertexShadows()[buffer]; shadow.bytes.resize(desc.Size);
    shadow.locked = data; shadow.offset = offset; shadow.length = length ? length : desc.Size-offset;
}
inline void captureVertexUnlock(IDirect3DVertexBuffer9* buffer) {
    auto found = vertexShadows().find(buffer);
    if (found == vertexShadows().end() || !found->second.locked) return;
    auto& shadow = found->second;
    std::memcpy(shadow.bytes.data()+shadow.offset, shadow.locked, shadow.length); shadow.locked = nullptr;
}
inline void forgetVertexBuffer(IDirect3DVertexBuffer9* buffer) { vertexShadows().erase(buffer); }

class MotionCapture9 {
    template<class T> using Ptr = Microsoft::WRL::ComPtr<T>;
    struct Pose {
        std::vector<DirectX::XMFLOAT3> vertices, previous;
        DirectX::XMFLOAT4X4 transform{}, previousTransform{};
        D3DMATRIX world{};
        uint32_t frame = UINT32_MAX;
    };
public:
    HRESULT initialize(IDirect3DDevice9* device) {
        device_ = device;
        const char* source = R"(
row_major float4x4 raster : register(c0);
row_major float4x4 current : register(c4);
row_major float4x4 previous : register(c8);
float4 alphaSettings : register(c12);
struct Input { float3 position:POSITION0; float3 oldPosition:POSITION1; float2 uv:TEXCOORD0; float4 color:COLOR0; };
struct Output { float4 position:POSITION; float4 now:TEXCOORD0; float4 old:TEXCOORD1; float2 uv:TEXCOORD2; float alpha:TEXCOORD3; };
Output VS(Input input) {
 Output o; o.position=mul(float4(input.position,1),raster); o.now=mul(float4(input.position,1),current);
 o.old=mul(float4(input.oldPosition,1),previous); o.uv=input.uv; o.alpha=input.color.a; return o;
}
sampler2D albedo:register(s0);
float4 PS(Output input):COLOR0 {
 float alpha=tex2D(albedo,input.uv).a * (alphaSettings.y ? input.alpha:1);
 if(alphaSettings.x>0) clip(alpha-alphaSettings.x);
 float2 now=input.now.xy/input.now.w, old=input.old.xy/input.old.w;
 return float4((old-now)*float2(0.5,-0.5)*alphaSettings.zw,0,1);
}
)";
        Ptr<ID3DBlob> vs, ps, errors;
        HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "VS", "vs_3_0", D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, 0, &vs, &errors);
        if (SUCCEEDED(hr)) hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "PS", "ps_3_0", D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, 0, &ps, &errors);
        if (FAILED(hr) && errors) std::fprintf(stderr,"Motion shader compilation: %s\n", static_cast<const char*>(errors->GetBufferPointer()));
        if (SUCCEEDED(hr)) hr = device_->CreateVertexShader(static_cast<const DWORD*>(vs->GetBufferPointer()), &vertexShader_);
        if (SUCCEEDED(hr)) hr = device_->CreatePixelShader(static_cast<const DWORD*>(ps->GetBufferPointer()), &pixelShader_);
        return hr;
    }
    void begin(IDirect3DSurface9* target, uint32_t frame) { target_ = target; frame_ = frame; object_ = 0; }
    void object(uint64_t identity) { object_ = identity; }
    void reset() { poses_.clear(); }
    void resetDevice() { poses_.clear(); previousBuffer_.Reset(); state_.Reset(); declarations_.clear(); previousCapacity_=0; target_=nullptr; }
    void end() {
        target_ = nullptr; object_ = 0;
        for (auto it=poses_.begin();it!=poses_.end();) {
            if (uint32_t(frame_-it->second.frame)>3) it=poses_.erase(it); else ++it;
        }
    }
    void matrices(const D3DMATRIX& unjitteredProjection) { projection_ = unjitteredProjection; }
    HRESULT indexed(D3DPRIMITIVETYPE type, INT base, UINT minimum, UINT count, UINT start, UINT primitives) {
        if (!target_ || !count || count > 1000000 || base < 0) return S_FALSE;
        Ptr<IDirect3DVertexBuffer9> buffer; UINT offset=0,stride=0;
        if (FAILED(device_->GetStreamSource(0,&buffer,&offset,&stride)) || !buffer || !stride) return S_FALSE;
        auto shadow=vertexShadows().find(buffer.Get());
        if (shadow==vertexShadows().end()) return S_FALSE;
        const uint64_t first=uint64_t(offset)+(uint64_t(base)+minimum)*stride;
        if (first+uint64_t(count)*stride>shadow->second.bytes.size()) return S_FALSE;
        return replay(type, buffer.Get(), UINT(first), stride, shadow->second.bytes.data()+first, count, minimum, start, primitives, true);
    }
    HRESULT primitive(D3DPRIMITIVETYPE type, UINT start, UINT primitives) {
        UINT count=vertexCount(type,primitives);
        if (!target_ || !count) return S_FALSE;
        Ptr<IDirect3DVertexBuffer9> buffer; UINT offset=0,stride=0;
        if (FAILED(device_->GetStreamSource(0,&buffer,&offset,&stride)) || !buffer || !stride) return S_FALSE;
        auto shadow=vertexShadows().find(buffer.Get());
        uint64_t first=uint64_t(offset)+uint64_t(start)*stride;
        if (shadow==vertexShadows().end() || first+uint64_t(count)*stride>shadow->second.bytes.size()) return S_FALSE;
        return replay(type,buffer.Get(),UINT(first),stride,shadow->second.bytes.data()+first,count,0,start,primitives,false);
    }
private:
    static UINT vertexCount(D3DPRIMITIVETYPE type, UINT primitives) {
        switch(type) { case D3DPT_TRIANGLELIST:return primitives*3; case D3DPT_TRIANGLESTRIP:case D3DPT_TRIANGLEFAN:return primitives+2;
            case D3DPT_LINELIST:return primitives*2; case D3DPT_LINESTRIP:return primitives+1; case D3DPT_POINTLIST:return primitives; default:return 0; }
    }
    HRESULT replay(D3DPRIMITIVETYPE type, IDirect3DVertexBuffer9* currentBuffer, UINT offset, UINT stride,
                   const unsigned char* bytes, UINT count, UINT minimum, UINT start, UINT primitives, bool indexed) {
        DWORD zEnable=0, zWrite=0, blend=0;
        device_->GetRenderState(D3DRS_ZENABLE,&zEnable); device_->GetRenderState(D3DRS_ZWRITEENABLE,&zWrite); device_->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);
        if (!zEnable || !zWrite || blend) return S_FALSE;
        Ptr<IDirect3DVertexShader9> shader; device_->GetVertexShader(&shader);
        // Programmable displacement cannot be reconstructed from CPU positions.
        if (shader) return S_FALSE;
        Ptr<IDirect3DVertexDeclaration9> original;
        if (FAILED(device_->GetVertexDeclaration(&original)) || !original) return S_FALSE;
        D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1]{}; UINT length=MAXD3DDECLLENGTH+1;
        if (FAILED(original->GetDeclaration(elements,&length))) return S_FALSE;
        D3DVERTEXELEMENT9 declaration[5]{}; UINT used=0; int position=-1; bool color=false,uv=false;
        for (UINT i=0;i<length && elements[i].Stream!=0xff;++i) {
            const auto& element=elements[i]; if(element.Stream!=0) continue;
            if(element.Usage==D3DDECLUSAGE_POSITION && element.UsageIndex==0 && element.Type==D3DDECLTYPE_FLOAT3) { declaration[used++]=element; position=element.Offset; }
            else if(element.Usage==D3DDECLUSAGE_TEXCOORD && element.UsageIndex==0 && element.Type==D3DDECLTYPE_FLOAT2) { declaration[used++]=element; uv=true; }
            else if(element.Usage==D3DDECLUSAGE_COLOR && element.UsageIndex==0) { declaration[used++]=element; color=true; }
        }
        if(position<0 || UINT(position)+12>stride) return S_FALSE;
        declaration[used++]={1,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,1}; declaration[used]=D3DDECL_END();
        uint64_t key=object_ ? object_ : uint64_t(reinterpret_cast<uintptr_t>(currentBuffer));
        auto mix=[&](uint64_t part) { key^=part+0x9e3779b97f4a7c15ULL+(key<<6)+(key>>2); };
        mix(start); mix(primitives); mix(count); mix(indexed); if(!object_) mix(offset);
        auto& pose=poses_[key];
        D3DMATRIX world{}, view{}; device_->GetTransform(D3DTS_WORLD,&world); device_->GetTransform(D3DTS_VIEW,&view);
        using namespace DirectX;
        auto load=[](const D3DMATRIX& matrix) { XMFLOAT4X4 value; std::memcpy(&value,&matrix,sizeof(value)); return XMLoadFloat4x4(&value); };
        XMFLOAT4X4 currentTransform; XMStoreFloat4x4(&currentTransform,load(world)*load(view)*load(projection_));
        if(pose.frame!=frame_) {
            const bool valid=pose.frame==frame_-1 && pose.vertices.size()==count;
            pose.previous.swap(pose.vertices); pose.previousTransform=pose.transform;
            pose.vertices.resize(count);
            for(UINT i=0;i<count;++i) std::memcpy(&pose.vertices[i],bytes+size_t(i)*stride+position,12);
            pose.transform=currentTransform; pose.frame=frame_;
            const bool stationary=valid && std::memcmp(&world,&pose.world,sizeof(world))==0 &&
                std::memcmp(pose.vertices.data(),pose.previous.data(),size_t(count)*12)==0;
            pose.world=world;
            if(!valid) { pose.previous=pose.vertices; pose.previousTransform=currentTransform; }
            // Static geometry already has exact camera motion from depth. Avoid
            // rendering the terrain/trees/buildings a second time every frame.
            if(stationary) return S_FALSE;
        }
        if(previousCapacity_<count) {
            previousBuffer_.Reset(); previousCapacity_=count;
            HRESULT hr=device_->CreateVertexBuffer(count*12,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&previousBuffer_,nullptr);
            if(FAILED(hr)) return hr;
        }
        void* memory=nullptr; HRESULT hr=previousBuffer_->Lock(0,count*12,&memory,D3DLOCK_DISCARD);
        if(FAILED(hr)) return hr;
        std::memcpy(memory,pose.previous.data(),count*12); previousBuffer_->Unlock();
        IDirect3DVertexDeclaration9* captureDeclaration=nullptr;
        for(auto& cached:declarations_) {
            if(cached.elements.size()==used+1 && std::memcmp(cached.elements.data(),declaration,(used+1)*sizeof(declaration[0]))==0) {captureDeclaration=cached.declaration.Get();break;}
        }
        if(!captureDeclaration) {
            Declaration cached; cached.elements.assign(declaration,declaration+used+1);
            if(FAILED(hr=device_->CreateVertexDeclaration(declaration,&cached.declaration))) return hr;
            captureDeclaration=cached.declaration.Get(); declarations_.push_back(std::move(cached));
        }
        Ptr<IDirect3DSurface9> renderTarget;
        if(!state_) hr=device_->CreateStateBlock(D3DSBT_ALL,&state_); else hr=state_->Capture();
        if(FAILED(hr) || FAILED(hr=device_->GetRenderTarget(0,&renderTarget))) return hr;
        D3DVIEWPORT9 viewport{}; device_->GetViewport(&viewport);
        D3DMATRIX actualProjection{}; device_->GetTransform(D3DTS_PROJECTION,&actualProjection);
        XMFLOAT4X4 raster; XMStoreFloat4x4(&raster,load(world)*load(view)*load(actualProjection));
        DWORD alphaTest=0,alphaRef=0; device_->GetRenderState(D3DRS_ALPHATESTENABLE,&alphaTest); device_->GetRenderState(D3DRS_ALPHAREF,&alphaRef);
        D3DSURFACE_DESC targetDescription{}; target_->GetDesc(&targetDescription);
        const float alpha[4]={alphaTest&&uv ? float(alphaRef)/255.0f:0, color?1.0f:0,
            float(viewport.Width)/targetDescription.Width,float(viewport.Height)/targetDescription.Height};
        device_->SetRenderTarget(0,target_); device_->SetViewport(&viewport);
        device_->SetVertexDeclaration(captureDeclaration); device_->SetVertexShader(vertexShader_.Get()); device_->SetPixelShader(pixelShader_.Get());
        device_->SetVertexShaderConstantF(0,&raster._11,4); device_->SetVertexShaderConstantF(4,&currentTransform._11,4); device_->SetVertexShaderConstantF(8,&pose.previousTransform._11,4);
        device_->SetPixelShaderConstantF(12,alpha,1);
        device_->SetRenderState(D3DRS_ZWRITEENABLE,FALSE); device_->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        device_->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE); device_->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
        device_->SetRenderState(D3DRS_STENCILENABLE,FALSE); device_->SetRenderState(D3DRS_FOGENABLE,FALSE);
        device_->SetRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN);
        device_->SetStreamSource(0,currentBuffer,offset,stride); device_->SetStreamSource(1,previousBuffer_.Get(),0,12);
        hr=indexed ? device_->DrawIndexedPrimitive(type,-INT(minimum),minimum,count,start,primitives) : device_->DrawPrimitive(type,0,primitives);
        device_->SetRenderTarget(0,renderTarget.Get()); state_->Apply(); device_->SetViewport(&viewport);
        return hr;
    }
    IDirect3DDevice9* device_=nullptr;
    IDirect3DSurface9* target_=nullptr;
    Ptr<IDirect3DVertexShader9> vertexShader_;
    Ptr<IDirect3DPixelShader9> pixelShader_;
    Ptr<IDirect3DVertexBuffer9> previousBuffer_;
    struct Declaration { std::vector<D3DVERTEXELEMENT9> elements; Ptr<IDirect3DVertexDeclaration9> declaration; };
    std::vector<Declaration> declarations_;
    Ptr<IDirect3DStateBlock9> state_;
    UINT previousCapacity_=0;
    uint64_t object_=0;
    uint32_t frame_=0;
    D3DMATRIX projection_{};
    std::unordered_map<uint64_t,Pose> poses_;
};
}
