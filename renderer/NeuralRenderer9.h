#pragma once
#include "StreamlineRuntime.h"
#include "MotionCapture9.h"
#include "NeuralResolve.h"
#include <memory>
#include <cmath>
#include "NativePresentation12.h"

namespace generals_mods {
inline void neuralLog(const char* message, HRESULT hr=S_OK) {
    FILE* file=nullptr; if(fopen_s(&file,"GeneralsNeuralAA.log","a")==0 && file) {
        std::fprintf(file,"PID %lu: %s: 0x%08lX\n",GetCurrentProcessId(),message,static_cast<unsigned long>(hr)); std::fclose(file);
    }
}
class NeuralRenderer9 {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
public:
    explicit NeuralRenderer9(IDirect3DDevice9* device):device_(device) {}
    bool available() {
        if(!probed_) {
            const ULONG before=referenceCount();
            probed_=true;
            Ptr<IDirect3DDevice9On12> interop; Ptr<ID3D12Device> native;
            HRESULT hr=device_->QueryInterface(IID_PPV_ARGS(&interop));
            if(SUCCEEDED(hr)) hr=interop->GetD3D12Device(IID_PPV_ARGS(&native));
            if(SUCCEEDED(hr)) hr=runtime_.initialize(native.Get());
            if(SUCCEEDED(hr)) hr=motion_.initialize(device_);
            ready_=SUCCEEDED(hr); neuralLog("DLSS/DLAA renderer initialization",hr);
            interop.Reset(); native.Reset(); // Count only references owned beyond this probe.
            references_+=referenceCount()-before;
        }
        return ready_;
    }
    bool setMode(UINT mode) {
        if(mode>2 || (mode && !available()) || active_) return false;
        if(mode!=mode_) { const ULONG before=referenceCount(); releaseTargets(); mode_=mode; reset_=true; motion_.resetDevice(); references_+=referenceCount()-before; }
        neuralLog(mode_==2 ? "DLAA selected":mode_==1 ? "DLSS Quality selected":"Neural AA disabled");
        return true;
    }
    UINT mode() const { return mode_; }
    void beforePresent() {runtime_.beforePresent();}
    void afterPresent() {runtime_.afterPresent();}
    HRESULT nativePresent() {
        const ULONG before=referenceCount();
        HRESULT hr=presentation_.present(device_);
        references_+=referenceCount()-before;
        return hr;
    }
    ULONG ownedReferences() const { return references_; }
    void resetDevice() { const ULONG before=referenceCount(); presentation_.reset(); releaseTargets(); reset_=true; motion_.resetDevice(); references_+=referenceCount()-before; }
    void resetHistory() {reset_=true;motion_.reset();}
    HRESULT begin(uint64_t scene) {
        if(!mode_ || !available()) return S_FALSE;
        Ptr<IDirect3DSurface9> target, depth;
        HRESULT hr=device_->GetRenderTarget(0,&target);
        if(FAILED(hr)) return hr;
        device_->GetDepthStencilSurface(&depth);
        D3DSURFACE_DESC description{}; target->GetDesc(&description);
        // MSAA must first be disabled by the engine's normal reset path.
        if(description.MultiSampleType!=D3DMULTISAMPLE_NONE) return S_FALSE;
        if(!resolve_ || width_!=description.Width || height_!=description.Height) {
            const ULONG before=referenceCount();
            releaseTargets(); width_=description.Width; height_=description.Height;
            resolve_=std::make_unique<NeuralResolve>(runtime_.module());
            hr=resolve_->initialize(device_,width_,height_,mode_==2 ? sl::DLSSMode::eDLAA:sl::DLSSMode::eMaxQuality);
            if(SUCCEEDED(hr)) hr=device_->CreateTexture(resolve_->renderWidth(),resolve_->renderHeight(),1,D3DUSAGE_RENDERTARGET,D3DFMT_A16B16G16R16F,D3DPOOL_DEFAULT,&colorTexture_,nullptr);
            if(SUCCEEDED(hr)) hr=colorTexture_->GetSurfaceLevel(0,&color_);
            if(SUCCEEDED(hr)) hr=device_->CreateTexture(resolve_->renderWidth(),resolve_->renderHeight(),1,D3DUSAGE_RENDERTARGET,D3DFMT_G16R16F,D3DPOOL_DEFAULT,&motionTexture_,nullptr);
            if(SUCCEEDED(hr)) hr=motionTexture_->GetSurfaceLevel(0,&motionTarget_);
            if(SUCCEEDED(hr)) hr=device_->CreateDepthStencilSurface(resolve_->renderWidth(),resolve_->renderHeight(),D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,FALSE,&depth_,nullptr);
            if(FAILED(hr)) { neuralLog("Neural scene targets failed",hr); releaseTargets(); references_+=referenceCount()-before; mode_=0; return hr; }
            references_+=referenceCount()-before;
            char text[180]; std::snprintf(text,sizeof(text),"%s scene %ux%u -> %ux%u",mode_==2 ? "DLAA":"DLSS Quality",resolve_->renderWidth(),resolve_->renderHeight(),width_,height_); neuralLog(text);
        }
        if(scene_!=scene) { scene_=scene; reset_=true; motion_.reset(); }
        target_=target; originalDepth_=depth; device_->GetViewport(&originalViewport_);
        device_->GetTransform(D3DTS_VIEW,&savedView_); device_->GetTransform(D3DTS_PROJECTION,&savedProjection_);
        device_->SetDepthStencilSurface(nullptr); device_->SetRenderTarget(0,motionTarget_.Get());
        device_->Clear(0,nullptr,D3DCLEAR_TARGET,0xffffffff,1,0);
        if(FAILED(hr=device_->SetRenderTarget(0,color_.Get())) || FAILED(hr=device_->SetDepthStencilSurface(depth_.Get()))) {
            restore(); return hr;
        }
        active_=true; cameraCaptured_=false;
        jitter_={halton((frame_%32)+1,2)-.5f,halton((frame_%32)+1,3)-.5f};
        motion_.begin(motionTarget_.Get(),frame_);
        D3DVIEWPORT9 viewport=scaledViewport(originalViewport_); device_->SetViewport(&viewport);
        return S_OK;
    }
    HRESULT end() {
        if(!active_) return S_FALSE;
        motion_.end(); active_=false;
        // D3D9 EndScene is needed to flush the translation layer before checkout.
        HRESULT hr=device_->EndScene();
        restore(true);
        if(SUCCEEDED(hr) && cameraCaptured_) {
            auto constants=cameraConstants();
            hr=resolve_->resolve(color_.Get(),depth_.Get(),motionTarget_.Get(),target_.Get(),constants,frame_);
            if(SUCCEEDED(hr)) {
                if(!evaluated_) { neuralLog("First actual game neural frame",hr); evaluated_=true; }
                if(++evaluations_==32) neuralLog("32 actual neural frames completed",hr);
                previousView_=cameraView_; previousProjection_=cameraProjection_; previousViewport_=cameraViewport_; reset_=false;
            }
        } else if(SUCCEEDED(hr)) hr=E_UNEXPECTED;
        if(FAILED(hr)) {
            neuralLog("Neural frame failed; preserving unfiltered scene",hr);
            device_->StretchRect(color_.Get(),nullptr,target_.Get(),nullptr,D3DTEXF_LINEAR);
            reset_=true; mode_=0;
        }
        HRESULT beginResult=device_->BeginScene();
        target_.Reset(); originalDepth_.Reset(); ++frame_;
        return FAILED(hr) ? hr:beginResult;
    }
    void abort() { if(active_) {motion_.end();restore();active_=false;target_.Reset();originalDepth_.Reset();reset_=true;} }
    D3DVIEWPORT9 scaledViewport(D3DVIEWPORT9 viewport) const {
        if(!active_ || !resolve_) return viewport;
        const double sx=double(resolve_->renderWidth())/width_,sy=double(resolve_->renderHeight())/height_;
        UINT right=UINT(std::lround((viewport.X+viewport.Width)*sx)),bottom=UINT(std::lround((viewport.Y+viewport.Height)*sy));
        viewport.X=UINT(std::lround(viewport.X*sx)); viewport.Y=UINT(std::lround(viewport.Y*sy));
        viewport.Width=right-viewport.X; viewport.Height=bottom-viewport.Y; return viewport;
    }
    HRESULT setViewport(const D3DVIEWPORT9* viewport) {
        if(!viewport) return D3DERR_INVALIDCALL;
        const auto scaled=scaledViewport(*viewport);
        Ptr<IDirect3DSurface9> target;
        if(SUCCEEDED(device_->GetRenderTarget(0,&target))) {
            D3DSURFACE_DESC description{}; target->GetDesc(&description);
            if(uint64_t(scaled.X)+scaled.Width>description.Width || uint64_t(scaled.Y)+scaled.Height>description.Height) return D3DERR_INVALIDCALL;
        }
        return device_->SetViewport(&scaled);
    }
    HRESULT getViewport(D3DVIEWPORT9* viewport) {
        HRESULT hr=device_->GetViewport(viewport);
        if(SUCCEEDED(hr) && active_ && resolve_) {
            const double sx=double(width_)/resolve_->renderWidth(),sy=double(height_)/resolve_->renderHeight();
            const UINT right=UINT(std::lround((viewport->X+viewport->Width)*sx)),bottom=UINT(std::lround((viewport->Y+viewport->Height)*sy));
            viewport->X=UINT(std::lround(viewport->X*sx)); viewport->Y=UINT(std::lround(viewport->Y*sy));
            viewport->Width=right-viewport->X; viewport->Height=bottom-viewport->Y;
        }
        return hr;
    }
    HRESULT transform(D3DTRANSFORMSTATETYPE state,const D3DMATRIX* matrix) {
        if(!matrix) return D3DERR_INVALIDCALL;
        if(active_ && state==D3DTS_PROJECTION) {
            projection_=*matrix; motion_.matrices(*matrix);
            if(std::abs(matrix->_44)<.0001f && std::abs(matrix->_34)>.5f) {
                using namespace DirectX;
                XMFLOAT4X4 value; std::memcpy(&value,matrix,sizeof(value));
                D3DVIEWPORT9 viewport{}; device_->GetViewport(&viewport);
                auto shift=XMMatrixTranslation(2*jitter_.x/viewport.Width,-2*jitter_.y/viewport.Height,0);
                XMStoreFloat4x4(&value,XMLoadFloat4x4(&value)*shift);
                D3DMATRIX raster; std::memcpy(&raster,&value,sizeof(raster));
                return device_->SetTransform(state,&raster);
            }
        }
        return device_->SetTransform(state,matrix);
    }
    void beforeDraw() {
        if(!active_ || cameraCaptured_ || std::abs(projection_._44)>.0001f || std::abs(projection_._34)<.5f) return;
        device_->GetTransform(D3DTS_VIEW,&cameraView_); cameraProjection_=projection_; cameraCaptured_=true;
        device_->GetViewport(&cameraViewport_);
    }
    HRESULT drawPrimitiveUP(D3DPRIMITIVETYPE type,UINT primitives,const void* data,UINT stride) {
        beforeDraw();
        const UINT count=type==D3DPT_TRIANGLELIST ? primitives*3:type==D3DPT_TRIANGLESTRIP || type==D3DPT_TRIANGLEFAN ? primitives+2:
            type==D3DPT_LINELIST ? primitives*2:type==D3DPT_LINESTRIP ? primitives+1:primitives;
        return device_->DrawPrimitiveUP(type,primitives,scaledScreenVertices(data,stride,count),stride);
    }
    HRESULT drawIndexedPrimitiveUP(D3DPRIMITIVETYPE type,UINT minimum,UINT count,UINT primitives,const void* indices,D3DFORMAT format,const void* data,UINT stride) {
        beforeDraw();
        return device_->DrawIndexedPrimitiveUP(type,minimum,count,primitives,indices,format,scaledScreenVertices(data,stride,minimum+count),stride);
    }
    void object(uint64_t identity) { if(active_) motion_.object(identity); }
    void afterIndexed(D3DPRIMITIVETYPE type,INT base,UINT minimum,UINT count,UINT start,UINT primitives) { if(active_) {const ULONG before=referenceCount(); motion_.indexed(type,base,minimum,count,start,primitives);references_+=referenceCount()-before;} }
    void afterPrimitive(D3DPRIMITIVETYPE type,UINT start,UINT primitives) { if(active_) {const ULONG before=referenceCount();motion_.primitive(type,start,primitives);references_+=referenceCount()-before;} }
private:
    const void* scaledScreenVertices(const void* data,UINT stride,UINT count) {
        DWORD fvf=0;device_->GetFVF(&fvf);
        if(!active_ || !data || stride<16 || count>1000000 || (fvf&D3DFVF_POSITION_MASK)!=D3DFVF_XYZRHW) return data;
        screenVertices_.resize(size_t(count)*stride);std::memcpy(screenVertices_.data(),data,screenVertices_.size());
        const float sx=float(resolve_->renderWidth())/width_,sy=float(resolve_->renderHeight())/height_;
        for(UINT i=0;i<count;++i) {
            float xy[2];std::memcpy(xy,screenVertices_.data()+size_t(i)*stride,8);
            xy[0]=(xy[0]+.5f)*sx-.5f;xy[1]=(xy[1]+.5f)*sy-.5f;
            std::memcpy(screenVertices_.data()+size_t(i)*stride,xy,8);
        }
        return screenVertices_.data();
    }
    ULONG referenceCount() const { device_->AddRef(); return device_->Release(); }
    void restore(bool afterDraw=false) {
        device_->SetDepthStencilSurface(nullptr);
        if(target_) device_->SetRenderTarget(0,target_.Get());
        device_->SetDepthStencilSurface(originalDepth_.Get()); device_->SetViewport(&originalViewport_);
        if(!afterDraw) device_->SetTransform(D3DTS_VIEW,&savedView_);
        device_->SetTransform(D3DTS_PROJECTION,afterDraw ? &projection_:&savedProjection_);
    }
    void releaseTargets() {
        resolve_.reset(); color_.Reset(); colorTexture_.Reset(); motionTarget_.Reset(); motionTexture_.Reset(); depth_.Reset();
        target_.Reset(); originalDepth_.Reset(); active_=false; evaluated_=false;evaluations_=0;
    }
    static float halton(UINT index,UINT base) { float value=0,fraction=1; while(index) {fraction/=base;value+=fraction*(index%base);index/=base;} return value; }
    sl::Constants cameraConstants() {
        using namespace DirectX;
        auto load=[](const D3DMATRIX& matrix){XMFLOAT4X4 value;std::memcpy(&value,&matrix,sizeof(value));return XMLoadFloat4x4(&value);};
        auto store=[](sl::float4x4& destination,FXMMATRIX matrix){XMFLOAT4X4 value;XMStoreFloat4x4(&value,matrix);std::memcpy(&destination,&value,sizeof(value));};
        auto viewportProjection=[&](const D3DMATRIX& matrix,const D3DVIEWPORT9& viewport) {
            const float sx=float(viewport.Width)/resolve_->renderWidth(),sy=float(viewport.Height)/resolve_->renderHeight();
            const float ox=(2.0f*viewport.X+viewport.Width)/resolve_->renderWidth()-1.0f;
            const float oy=1.0f-(2.0f*viewport.Y+viewport.Height)/resolve_->renderHeight();
            return load(matrix)*XMMatrixScaling(sx,sy,1)*XMMatrixTranslation(ox,oy,0);
        };
        auto view=load(cameraView_),projection=viewportProjection(cameraProjection_,cameraViewport_),inverseView=XMMatrixInverse(nullptr,view);
        XMFLOAT4X4 world; XMStoreFloat4x4(&world,inverseView);
        if(!reset_) {
            XMFLOAT4X4 oldWorld;XMStoreFloat4x4(&oldWorld,XMMatrixInverse(nullptr,load(previousView_)));
            const float dx=world._41-oldWorld._41,dy=world._42-oldWorld._42,dz=world._43-oldWorld._43;
            if(dx*dx+dy*dy+dz*dz>10000.0f) resetHistory();
        }
        const auto& p=cameraProjection_;
        sl::Constants c{}; store(c.cameraViewToClip,projection); store(c.clipToCameraView,XMMatrixInverse(nullptr,projection));
        auto previous=reset_ ? view*projection:load(previousView_)*viewportProjection(previousProjection_,previousViewport_);
        auto clipToPrevious=XMMatrixInverse(nullptr,view*projection)*previous;
        store(c.clipToPrevClip,clipToPrevious);store(c.prevClipToClip,XMMatrixInverse(nullptr,clipToPrevious));
        c.cameraPos={world._41,world._42,world._43}; c.cameraUp={world._21,world._22,world._23}; c.cameraRight={world._11,world._12,world._13};
        c.cameraFwd={world._31*p._34,world._32*p._34,world._33*p._34};
        c.cameraNear=-p._43/(p._33*p._34);c.cameraFar=-p._43/(p._33*p._34-1);
        c.cameraFOV=2*std::atan(1/std::abs(p._22));c.cameraAspectRatio=float(width_)/height_;
        c.jitterOffset=jitter_;c.mvecScale={1,1};c.cameraPinholeOffset={0,0};c.depthInverted=sl::Boolean::eFalse;
        c.cameraMotionIncluded=sl::Boolean::eTrue;c.motionVectors3D=sl::Boolean::eFalse;c.reset=reset_ ? sl::Boolean::eTrue:sl::Boolean::eFalse;
        return c;
    }
    IDirect3DDevice9* device_;
    StreamlineRuntime runtime_; // Destroyed after resolve and its GPU resources.
    MotionCapture9 motion_;
    std::unique_ptr<NeuralResolve> resolve_;
    Ptr<IDirect3DTexture9> colorTexture_,motionTexture_;
    Ptr<IDirect3DSurface9> color_,motionTarget_,depth_,target_,originalDepth_;
    D3DVIEWPORT9 originalViewport_{},cameraViewport_{},previousViewport_{};
    D3DMATRIX projection_{},savedView_{},savedProjection_{},cameraView_{},cameraProjection_{},previousView_{},previousProjection_{};
    sl::float2 jitter_{0,0};
    UINT width_=0,height_=0,mode_=0;
    ULONG references_=0;
    NativePresentation12 presentation_;
    std::vector<unsigned char> screenVertices_;
    uint32_t frame_=0;
    uint32_t evaluations_=0;
    uint64_t scene_=0;
    bool probed_=false,ready_=false,active_=false,cameraCaptured_=false,reset_=true,evaluated_=false;
};
inline std::unordered_map<IDirect3DDevice9*,std::unique_ptr<NeuralRenderer9>>& neuralRenderers() {
    static std::unordered_map<IDirect3DDevice9*,std::unique_ptr<NeuralRenderer9>> renderers;return renderers;
}
inline NeuralRenderer9& neuralRenderer(IDirect3DDevice9* device) {
    auto& entry=neuralRenderers()[device];if(!entry) entry=std::make_unique<NeuralRenderer9>(device);return *entry;
}
inline void releaseNeuralRenderer(IDirect3DDevice9* device) {neuralRenderers().erase(device);}
inline ULONG neuralOwnedReferences(IDirect3DDevice9* device) {
    auto found=neuralRenderers().find(device);return found==neuralRenderers().end() ? 0:found->second->ownedReferences();
}
}
