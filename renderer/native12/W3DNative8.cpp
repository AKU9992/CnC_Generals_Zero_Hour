// W3D's existing source ABI implemented with native D3D12 objects.
// No Direct3D8/9 runtime is loaded or queried by this module.
#define Direct3DCreate8 W3DLegacyDeclarationDirect3DCreate8
#include <d3d8.h>
#undef Direct3DCreate8
#include "NativeScene12.h"
#include "W3DMaterial12.h"
#include <DirectXMath.h>
#include <atomic>
#include <set>
#include <string>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace generals_mods::native12::w3d {
static void log(const char* message){FILE* file=nullptr;if(fopen_s(&file,"GeneralsNative12.log","a")==0 && file){
    std::fprintf(file,"PID %lu: %s\n",GetCurrentProcessId(),message);std::fclose(file);}}
static void unsupported(const char* name){static std::set<std::string> reported;if(reported.insert(name).second){
    char text[256]{};sprintf_s(text,"Unsupported operation: %s",name);log(text);}}
#include "W3DNativeInterfaces.h"
struct Device8;
struct Context {
    Device device;Dlss dlss{device};Scene scene{device,dlss};Materials materials{device};
    IDirect3DDevice8* owner=nullptr;
};
static HRESULT owner(const std::shared_ptr<Context>& context,IDirect3DDevice8** out){
    if(!out)return E_POINTER;*out=context->owner;if(!*out)return D3DERR_INVALIDCALL;(*out)->AddRef();return S_OK;
}
static DXGI_FORMAT format(D3DFORMAT f){switch(f){
    case D3DFMT_A8R8G8B8:case D3DFMT_X8R8G8B8:return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3DFMT_R5G6B5:return DXGI_FORMAT_B5G6R5_UNORM;
    case D3DFMT_A1R5G5B5:case D3DFMT_X1R5G5B5:return DXGI_FORMAT_B5G5R5A1_UNORM;
    case D3DFMT_A4R4G4B4:return DXGI_FORMAT_B4G4R4A4_UNORM;
    case D3DFMT_A8:case D3DFMT_L8:return DXGI_FORMAT_R8_UNORM;
    case D3DFMT_A8L8:return DXGI_FORMAT_R8G8_UNORM;
    case D3DFMT_V8U8:return DXGI_FORMAT_R8G8_SNORM;
    case D3DFMT_DXT1:return DXGI_FORMAT_BC1_UNORM;
    case D3DFMT_DXT2:case D3DFMT_DXT3:return DXGI_FORMAT_BC2_UNORM;
    case D3DFMT_DXT4:case D3DFMT_DXT5:return DXGI_FORMAT_BC3_UNORM;
    case D3DFMT_D16:return DXGI_FORMAT_D16_UNORM;
    case D3DFMT_D24S8:case D3DFMT_D24X8:return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case D3DFMT_D32:return DXGI_FORMAT_D32_FLOAT;
    default:return DXGI_FORMAT_UNKNOWN;
}}
static UINT pixelBytes(D3DFORMAT f){switch(f){case D3DFMT_A8:case D3DFMT_L8:return 1;
    case D3DFMT_R5G6B5:case D3DFMT_A1R5G5B5:case D3DFMT_X1R5G5B5:case D3DFMT_A4R4G4B4:case D3DFMT_A8L8:case D3DFMT_V8U8:case D3DFMT_D16:return 2;
    default:return 4;}}
static bool compressed(D3DFORMAT f){return f==D3DFMT_DXT1 || f==D3DFMT_DXT2 || f==D3DFMT_DXT3 || f==D3DFMT_DXT4 || f==D3DFMT_DXT5;}
static std::shared_ptr<TextureData> textureData(UINT width,UINT height,UINT levels,D3DFORMAT f,DWORD usage){
    if(!width || !height || width>16384 || height>16384 || format(f)==DXGI_FORMAT_UNKNOWN)return {};
    UINT maximum=1;for(UINT size=std::max(width,height);size>1;size>>=1)++maximum;
    if(!levels)levels=maximum;levels=std::min(levels,maximum);
    auto t=std::make_shared<TextureData>();t->width=width;t->height=height;t->format=format(f);
    t->renderTarget=(usage&D3DUSAGE_RENDERTARGET)!=0;t->depthStencil=(usage&D3DUSAGE_DEPTHSTENCIL)!=0;
    if(f==D3DFMT_X8R8G8B8 || f==D3DFMT_X1R5G5B5)t->componentMapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0,1,2,D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
    if(f==D3DFMT_L8)t->componentMapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0,0,0,D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
    if(f==D3DFMT_A8)t->componentMapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(5,5,5,0);
    if(f==D3DFMT_A8L8)t->componentMapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0,0,0,1);
    for(UINT i=0;i<levels;++i){TextureLevel l;l.width=std::max(1u,width>>i);l.height=std::max(1u,height>>i);
        l.rowBytes=compressed(f) ? std::max(1u,(l.width+3)/4)*(f==D3DFMT_DXT1 ? 8u : 16u) : l.width*pixelBytes(f);
        l.rows=compressed(f) ? std::max(1u,(l.height+3)/4) : l.height;l.bytes.resize(size_t(l.rowBytes)*l.rows);t->levels.push_back(std::move(l));}
    return t;
}
struct TextureMeta {
    std::shared_ptr<Context> context;std::shared_ptr<TextureData> data;D3DFORMAT format=D3DFMT_UNKNOWN;DWORD usage=0;D3DPOOL pool=D3DPOOL_MANAGED;
    std::vector<bool> locked,readOnly;
    HRESULT desc(UINT level,D3DSURFACE_DESC* out){if(!out)return E_POINTER;if(level>=data->levels.size())return D3DERR_INVALIDCALL;
        const auto& l=data->levels[level];*out={format,D3DRTYPE_SURFACE,usage,pool,static_cast<UINT>(l.bytes.size()),D3DMULTISAMPLE_NONE,l.width,l.height};return S_OK;}
    HRESULT lock(UINT level,D3DLOCKED_RECT* out,const RECT* rect,DWORD flags){
        if(!out || level>=data->levels.size() || locked[level])return D3DERR_INVALIDCALL;
        auto& l=data->levels[level];UINT x=0,y=0;
        if(rect){if(rect->left<0 || rect->top<0 || rect->right<=rect->left || rect->bottom<=rect->top || UINT(rect->right)>l.width || UINT(rect->bottom)>l.height)return D3DERR_INVALIDCALL;
            x=UINT(rect->left);y=UINT(rect->top);if(compressed(format) && ((x&3)||(y&3)))return D3DERR_INVALIDCALL;}
        if(compressed(format)){x=x/4*(format==D3DFMT_DXT1 ? 8u : 16u);y/=4;}else x*=pixelBytes(format);
        out->Pitch=static_cast<INT>(l.rowBytes);out->pBits=l.bytes.data()+size_t(y)*l.rowBytes+x;
        locked[level]=true;readOnly[level]=(flags&D3DLOCK_READONLY)!=0;return S_OK;
    }
    HRESULT unlock(UINT level){if(level>=locked.size() || !locked[level])return D3DERR_INVALIDCALL;
        locked[level]=false;if(!readOnly[level])data->dirty=true;return S_OK;}
};
struct Surface8 final:Surface8Methods {
    std::atomic<ULONG> refs{1};std::shared_ptr<TextureMeta> meta;UINT level=0;bool back=false;
    Surface8(std::shared_ptr<TextureMeta> m,UINT l=0,bool b=false):meta(std::move(m)),level(l),back(b){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;
        if(id==IID_IUnknown || id==IID_IDirect3DSurface8){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** out)override{return owner(meta->context,out);}
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* out)override{return meta->desc(level,out);}
    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* out,const RECT* rect,DWORD flags)override{return meta->lock(level,out,rect,flags);}
    HRESULT STDMETHODCALLTYPE UnlockRect()override{return meta->unlock(level);}
};
struct Texture8 final:Texture8Methods {
    std::atomic<ULONG> refs{1};std::shared_ptr<TextureMeta> meta;DWORD priority=0,lod=0;
    explicit Texture8(std::shared_ptr<TextureMeta> m):meta(std::move(m)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;
        if(id==IID_IUnknown || id==IID_IDirect3DResource8 || id==IID_IDirect3DBaseTexture8 || id==IID_IDirect3DTexture8){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** out)override{return owner(meta->context,out);}
    DWORD STDMETHODCALLTYPE SetPriority(DWORD p)override{DWORD old=priority;priority=p;return old;}DWORD STDMETHODCALLTYPE GetPriority()override{return priority;}
    void STDMETHODCALLTYPE PreLoad()override{}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType()override{return D3DRTYPE_TEXTURE;}
    DWORD STDMETHODCALLTYPE SetLOD(DWORD l)override{DWORD old=lod;lod=std::min(l,GetLevelCount()-1);return old;}
    DWORD STDMETHODCALLTYPE GetLOD()override{return lod;}DWORD STDMETHODCALLTYPE GetLevelCount()override{return static_cast<DWORD>(meta->data->levels.size());}
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT l,D3DSURFACE_DESC* out)override{return meta->desc(l,out);}
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT l,IDirect3DSurface8** out)override{if(!out)return E_POINTER;*out=nullptr;
        if(l>=meta->data->levels.size())return D3DERR_INVALIDCALL;*out=new Surface8(meta,l);return S_OK;}
    HRESULT STDMETHODCALLTYPE LockRect(UINT l,D3DLOCKED_RECT* out,const RECT* rect,DWORD flags)override{return meta->lock(l,out,rect,flags);}
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT l)override{return meta->unlock(l);}
    HRESULT STDMETHODCALLTYPE AddDirtyRect(const RECT*)override{meta->data->dirty=true;return S_OK;}
};
template<class Base,class Desc,D3DRESOURCETYPE Type>struct Buffer8 final:Base {
    std::atomic<ULONG> refs{1};std::shared_ptr<Context> context;Desc desc{};std::vector<uint8_t> bytes;bool locked=false;
    Buffer8(std::shared_ptr<Context> c,const Desc& d):context(std::move(c)),desc(d),bytes(d.Size){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;
        if(id==IID_IUnknown || id==IID_IDirect3DResource8 || (Type==D3DRTYPE_VERTEXBUFFER ? id==IID_IDirect3DVertexBuffer8 : id==IID_IDirect3DIndexBuffer8)){*out=this;this->AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** out)override{return owner(context,out);}
    DWORD STDMETHODCALLTYPE SetPriority(DWORD)override{return 0;}DWORD STDMETHODCALLTYPE GetPriority()override{return 0;}
    void STDMETHODCALLTYPE PreLoad()override{}D3DRESOURCETYPE STDMETHODCALLTYPE GetType()override{return Type;}
    HRESULT STDMETHODCALLTYPE GetDesc(Desc* out)override{if(!out)return E_POINTER;*out=desc;return S_OK;}
    HRESULT STDMETHODCALLTYPE Lock(UINT offset,UINT size,BYTE** out,DWORD)override{if(!out || locked || offset>bytes.size())return D3DERR_INVALIDCALL;
        if(!size)size=static_cast<UINT>(bytes.size())-offset;if(size>bytes.size()-offset)return D3DERR_INVALIDCALL;*out=bytes.data()+offset;locked=true;return S_OK;}
    HRESULT STDMETHODCALLTYPE Unlock()override{if(!locked)return D3DERR_INVALIDCALL;locked=false;return S_OK;}
};
using VertexBuffer8=Buffer8<VertexBuffer8Methods,D3DVERTEXBUFFER_DESC,D3DRTYPE_VERTEXBUFFER>;
using IndexBuffer8=Buffer8<IndexBuffer8Methods,D3DINDEXBUFFER_DESC,D3DRTYPE_INDEXBUFFER>;
static void identity(D3DMATRIX& matrix){using namespace DirectX;XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&matrix),XMMatrixIdentity());}
static void caps(D3DCAPS8* out){*out={};out->DeviceType=D3DDEVTYPE_HAL;out->Caps=D3DCAPS_READ_SCANLINE;
    out->Caps2=D3DCAPS2_CANRENDERWINDOWED;out->PresentationIntervals=D3DPRESENT_INTERVAL_IMMEDIATE|D3DPRESENT_INTERVAL_ONE;
    out->DevCaps=D3DDEVCAPS_HWTRANSFORMANDLIGHT|D3DDEVCAPS_HWRASTERIZATION|D3DDEVCAPS_DRAWPRIMTLVERTEX|D3DDEVCAPS_EXECUTESYSTEMMEMORY|
        D3DDEVCAPS_TEXTUREVIDEOMEMORY|D3DDEVCAPS_TEXTURESYSTEMMEMORY|D3DDEVCAPS_DRAWPRIMITIVES2|D3DDEVCAPS_DRAWPRIMITIVES2EX;
    out->PrimitiveMiscCaps=D3DPMISCCAPS_CULLNONE|D3DPMISCCAPS_CULLCW|D3DPMISCCAPS_CULLCCW|D3DPMISCCAPS_COLORWRITEENABLE;
    out->RasterCaps=D3DPRASTERCAPS_FOGVERTEX|D3DPRASTERCAPS_FOGTABLE|D3DPRASTERCAPS_ZBIAS|D3DPRASTERCAPS_ANISOTROPY|D3DPRASTERCAPS_MIPMAPLODBIAS;
    out->ZCmpCaps=out->AlphaCmpCaps=255;out->SrcBlendCaps=out->DestBlendCaps=0x7ff;
    out->TextureCaps=D3DPTEXTURECAPS_ALPHA|D3DPTEXTURECAPS_MIPMAP|D3DPTEXTURECAPS_PROJECTED;
    out->TextureFilterCaps=D3DPTFILTERCAPS_MINFPOINT|D3DPTFILTERCAPS_MINFLINEAR|D3DPTFILTERCAPS_MINFANISOTROPIC|
        D3DPTFILTERCAPS_MAGFPOINT|D3DPTFILTERCAPS_MAGFLINEAR|D3DPTFILTERCAPS_MAGFANISOTROPIC|D3DPTFILTERCAPS_MIPFPOINT|D3DPTFILTERCAPS_MIPFLINEAR;
    out->TextureAddressCaps=0x3f;out->StencilCaps=255;
    out->MaxTextureWidth=out->MaxTextureHeight=8192;out->MaxTextureRepeat=8192;out->MaxTextureAspectRatio=8192;out->MaxAnisotropy=16;
    out->TextureOpCaps=0xffffff & ~D3DTEXOPCAPS_PREMODULATE;out->MaxTextureBlendStages=out->MaxSimultaneousTextures=4;
    out->VertexProcessingCaps=D3DVTXPCAPS_TEXGEN|D3DVTXPCAPS_DIRECTIONALLIGHTS|D3DVTXPCAPS_POSITIONALLIGHTS|D3DVTXPCAPS_LOCALVIEWER|D3DVTXPCAPS_MATERIALSOURCE7;
    out->MaxActiveLights=8;out->MaxUserClipPlanes=6;out->MaxVertexW=1e10f;out->MaxPrimitiveCount=0xffffff;out->MaxVertexIndex=0xffffff;
    out->MaxStreams=1;out->MaxStreamStride=256;out->MaxPointSize=1;out->VertexShaderVersion=out->PixelShaderVersion=0;
}
struct Device8 final:Device8Methods {
    std::atomic<ULONG> refs{1};std::shared_ptr<Context> context=std::make_shared<Context>();
    Microsoft::WRL::ComPtr<IDirect3D8> parent;D3DPRESENT_PARAMETERS parameters{};HWND window=nullptr;
    MaterialState state;D3DMATRIX transforms[512]{};
    Microsoft::WRL::ComPtr<IDirect3DVertexBuffer8> streams[2];UINT strides[2]{};
    Microsoft::WRL::ComPtr<IDirect3DIndexBuffer8> indices;UINT baseVertex=0;DWORD vertexFormat=D3DFVF_XYZ,pixelShader=0;
    Microsoft::WRL::ComPtr<IDirect3DBaseTexture8> textures[4];
    Microsoft::WRL::ComPtr<IDirect3DSurface8> back,defaultDepth,target,zTarget;
    bool world=false,sceneReady=false,resetHistory=true,inScene=false;float clearColor[4]={0,0,0,1};UINT frameIndex=0,draws=0;
    uint64_t objectId=0,sceneId=0;
    D3DMATRIX previousCamera{};D3DVIEWPORT8 previousViewport{};float previousCameraPosition[3]{};UINT worldFrames=0;DWORD loggedWater=0;bool loggedShadow=false;
    struct History{D3DMATRIX transform;std::vector<MaterialVertex> vertices;UINT frame=0;};std::map<std::array<uint64_t,3>,History> history;
    struct Snapshot{MaterialState state;D3DMATRIX transforms[512];DWORD vertexFormat;};std::map<DWORD,Snapshot> blocks;DWORD nextBlock=1;
    explicit Device8(IDirect3D8* p):parent(p){context->owner=this;defaults();}
    ~Device8(){if(context->device.commands())context->device.endFrame();context->owner=nullptr;context->device.waitIdle();}
    void defaults(){
        identity(state.world);identity(state.view);identity(state.projection);identity(state.previousTransform);
        for(auto& matrix:transforms)identity(matrix);for(auto& matrix:state.textureMatrix)identity(matrix);
        auto* r=state.render;r[D3DRS_ZENABLE]=TRUE;r[D3DRS_ZWRITEENABLE]=TRUE;r[D3DRS_ZFUNC]=D3DCMP_LESSEQUAL;r[D3DRS_CULLMODE]=D3DCULL_CCW;
        r[D3DRS_SRCBLEND]=D3DBLEND_ONE;r[D3DRS_DESTBLEND]=D3DBLEND_ZERO;r[D3DRS_ALPHAFUNC]=D3DCMP_ALWAYS;
        r[D3DRS_STENCILFAIL]=r[D3DRS_STENCILZFAIL]=r[D3DRS_STENCILPASS]=D3DSTENCILOP_KEEP;r[D3DRS_STENCILFUNC]=D3DCMP_ALWAYS;
        r[D3DRS_STENCILMASK]=r[D3DRS_STENCILWRITEMASK]=0xffffffff;r[D3DRS_COLORWRITEENABLE]=15;r[D3DRS_TEXTUREFACTOR]=0xffffffff;
        r[D3DRS_LIGHTING]=TRUE;r[D3DRS_COLORVERTEX]=TRUE;r[D3DRS_DIFFUSEMATERIALSOURCE]=D3DMCS_COLOR1;r[D3DRS_AMBIENTMATERIALSOURCE]=D3DMCS_MATERIAL;
        r[D3DRS_LOCALVIEWER]=TRUE;r[D3DRS_SPECULARMATERIALSOURCE]=D3DMCS_COLOR2;
        for(UINT i=0;i<4;++i){auto* s=state.stage[i];s[D3DTSS_COLOROP]=i ? D3DTOP_DISABLE : D3DTOP_MODULATE;
            s[D3DTSS_ALPHAOP]=i ? D3DTOP_DISABLE : D3DTOP_SELECTARG1;s[D3DTSS_COLORARG1]=s[D3DTSS_ALPHAARG1]=D3DTA_TEXTURE;
            s[D3DTSS_COLORARG2]=s[D3DTSS_ALPHAARG2]=D3DTA_CURRENT;s[D3DTSS_TEXCOORDINDEX]=i;
            s[D3DTSS_ADDRESSU]=s[D3DTSS_ADDRESSV]=s[D3DTSS_ADDRESSW]=D3DTADDRESS_WRAP;
            s[D3DTSS_MINFILTER]=s[D3DTSS_MAGFILTER]=D3DTEXF_POINT;s[D3DTSS_MAXANISOTROPY]=1;}
        state.material.Diffuse={1,1,1,1};state.material.Ambient={1,1,1,1};
    }
    std::shared_ptr<TextureMeta> meta(UINT w,UINT h,UINT levels,D3DFORMAT f,DWORD usage,D3DPOOL pool){
        auto data=textureData(w,h,levels,f,usage);if(!data)return {};auto m=std::make_shared<TextureMeta>();m->context=context;m->data=data;
        m->format=f;m->usage=usage;m->pool=pool;m->locked.resize(data->levels.size());m->readOnly.resize(data->levels.size());return m;
    }
    HRESULT initialize(D3DPRESENT_PARAMETERS* p,HWND focus){
        if(!p)return D3DERR_INVALIDCALL;parameters=*p;window=p->hDeviceWindow ? p->hDeviceWindow : focus;
        RECT rect{};GetClientRect(window,&rect);UINT w=p->BackBufferWidth ? p->BackBufferWidth : UINT(rect.right),h=p->BackBufferHeight ? p->BackBufferHeight : UINT(rect.bottom);
        HRESULT hr=context->device.initialize(window,w,h);if(FAILED(hr))return hr;
        hr=context->materials.initialize();if(FAILED(hr))return hr;
        back.Attach(new Surface8(meta(w,h,1,D3DFMT_A8R8G8B8,0,D3DPOOL_DEFAULT),0,true));
        hr=CreateDepthStencilSurface(w,h,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&defaultDepth);if(FAILED(hr))return hr;
        state.viewport={0,0,w,h,0,1};parameters.BackBufferWidth=w;parameters.BackBufferHeight=h;*p=parameters;
        log("Native AMD64 D3D12 device created; W3D ABI uses native resources");return S_OK;
    }
    HRESULT frame(){if(context->device.commands())return S_OK;
        if(!sceneReady){HRESULT hr=context->scene.initialize();if(FAILED(hr))return hr;sceneReady=true;}
        HRESULT hr=context->device.beginFrame(clearColor);if(FAILED(hr))return hr;context->materials.beginFrame();return bind();}
    HRESULT bind(){
        auto* commands=context->device.commands();if(!commands)return D3DERR_INVALIDCALL;
        if(target && !static_cast<Surface8*>(target.Get())->back){auto& m=*static_cast<Surface8*>(target.Get())->meta;
            HRESULT hr=uploadTexture(context->device,*m.data);if(FAILED(hr))return hr;textureBarrier(commands,*m.data,D3D12_RESOURCE_STATE_RENDER_TARGET);
            context->materials.retain(m.data->resource.Get());
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;
            hr=context->device.device()->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap));if(FAILED(hr))return hr;
            auto handle=heap->GetCPUDescriptorHandleForHeapStart();D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=m.data->format;view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;
            view.Texture2D.MipSlice=static_cast<Surface8*>(target.Get())->level;context->device.device()->CreateRenderTargetView(m.data->resource.Get(),&view,handle);
            D3D12_CPU_DESCRIPTOR_HANDLE depth{};Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depthHeap;
            if(zTarget){auto& z=*static_cast<Surface8*>(zTarget.Get())->meta;hr=uploadTexture(context->device,*z.data);if(FAILED(hr))return hr;
                context->materials.retain(z.data->resource.Get());
                hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;hr=context->device.device()->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&depthHeap));if(FAILED(hr))return hr;
                depth=depthHeap->GetCPUDescriptorHandleForHeapStart();context->device.device()->CreateDepthStencilView(z.data->resource.Get(),nullptr,depth);state.depthFormat=z.data->format;
            }else state.depthFormat=DXGI_FORMAT_UNKNOWN;
            commands->OMSetRenderTargets(1,&handle,FALSE,zTarget ? &depth : nullptr);state.colorFormat=m.data->format;state.worldPass=false;
        }else if(world){HRESULT hr=context->scene.bind();if(FAILED(hr))return hr;state.colorFormat=DXGI_FORMAT_R16G16B16A16_FLOAT;
            state.depthFormat=DXGI_FORMAT_D24_UNORM_S8_UINT;state.worldPass=true;
        }else{HRESULT hr=context->device.bindTarget();if(FAILED(hr))return hr;
            state.colorFormat=DXGI_FORMAT_R8G8B8A8_UNORM;state.depthFormat=DXGI_FORMAT_UNKNOWN;state.worldPass=false;
            if(defaultDepth){auto& d=*static_cast<Surface8*>(defaultDepth.Get())->meta->data;hr=uploadTexture(context->device,d);if(FAILED(hr))return hr;
                Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv,dsv;D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=1;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                hr=context->device.device()->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv));if(FAILED(hr))return hr;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
                hr=context->device.device()->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&dsv));if(FAILED(hr))return hr;
                auto color=rtv->GetCPUDescriptorHandleForHeapStart(),depth=dsv->GetCPUDescriptorHandleForHeapStart();
                context->device.device()->CreateRenderTargetView(context->device.target(),nullptr,color);context->device.device()->CreateDepthStencilView(d.resource.Get(),nullptr,depth);
                commands->OMSetRenderTargets(1,&color,FALSE,&depth);state.depthFormat=d.format;
            }
        }
        state.renderWidth=world ? context->dlss.renderWidth() : context->device.width();state.renderHeight=world ? context->dlss.renderHeight() : context->device.height();
        float sx=world ? float(state.renderWidth)/context->device.width() : 1,sy=world ? float(state.renderHeight)/context->device.height() : 1;
        if(target && !static_cast<Surface8*>(target.Get())->back){D3DSURFACE_DESC d{};target->GetDesc(&d);state.renderWidth=d.Width;state.renderHeight=d.Height;sx=sy=1;}
        D3D12_VIEWPORT v{state.viewport.X*sx,state.viewport.Y*sy,state.viewport.Width*sx,state.viewport.Height*sy,state.viewport.MinZ,state.viewport.MaxZ};
        D3D12_RECT scissor{LONG(v.TopLeftX),LONG(v.TopLeftY),LONG(v.TopLeftX+v.Width),LONG(v.TopLeftY+v.Height)};
        commands->RSSetViewports(1,&v);commands->RSSetScissorRects(1,&scissor);return S_OK;
    }
    HRESULT beginWorld(uint64_t scene){
        HRESULT hr=frame();if(FAILED(hr))return hr;if(world)return D3DERR_INVALIDCALL;
        if(scene!=sceneId){sceneId=scene;history.clear();resetHistory=true;}
        if(!sceneReady){ // Initialization is performed outside command recording by BeginScene.
            return E_UNEXPECTED;
        }
        if(context->dlss.mode()!=NeuralMode::Off){auto halton=[](UINT index,UINT base){float value=0,scale=1;while(index){scale/=float(base);value+=float(index%base)*scale;index/=base;}return value;};
            state.jitter[0]=halton(frameIndex%16+1,2)-.5f;state.jitter[1]=halton(frameIndex%16+1,3)-.5f;}
        hr=context->scene.begin(clearColor);if(FAILED(hr))return hr;world=true;target.Reset();zTarget.Reset();return bind();
    }
    sl::Constants camera(){using namespace DirectX;sl::Constants c{};
        auto p=XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&state.projection));XMFLOAT4X4 inverse,clipHistory,fullProjection;
        const float sx=float(state.viewport.Width)/context->device.width(),sy=float(state.viewport.Height)/context->device.height();
        p=p*XMMatrixScaling(sx,sy,1)*XMMatrixTranslation(2*float(state.viewport.X)/context->device.width()+sx-1,1-sy-2*float(state.viewport.Y)/context->device.height(),0);
        if(previousViewport.X!=state.viewport.X || previousViewport.Y!=state.viewport.Y || previousViewport.Width!=state.viewport.Width || previousViewport.Height!=state.viewport.Height)resetHistory=true;
        previousViewport=state.viewport;
        XMStoreFloat4x4(&inverse,XMMatrixInverse(nullptr,p));
        XMStoreFloat4x4(&fullProjection,p);std::memcpy(&c.cameraViewToClip,&fullProjection,64);std::memcpy(&c.clipToCameraView,&inverse,64);
        auto v=XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&state.view));auto currentCamera=v*p;
        auto prior=resetHistory || !worldFrames ? currentCamera : XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&previousCamera));
        auto historyMatrix=XMMatrixInverse(nullptr,currentCamera)*prior;XMStoreFloat4x4(&clipHistory,historyMatrix);std::memcpy(&c.clipToPrevClip,&clipHistory,64);
        XMStoreFloat4x4(&clipHistory,XMMatrixInverse(nullptr,historyMatrix));std::memcpy(&c.prevClipToClip,&clipHistory,64);
        XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&previousCamera),currentCamera);
        XMStoreFloat4x4(&inverse,XMMatrixInverse(nullptr,v));
        c.cameraPos={inverse._41,inverse._42,inverse._43};c.cameraRight={inverse._11,inverse._12,inverse._13};
        const float dx=c.cameraPos.x-previousCameraPosition[0],dy=c.cameraPos.y-previousCameraPosition[1],dz=c.cameraPos.z-previousCameraPosition[2];
        if(worldFrames && dx*dx+dy*dy+dz*dz>10000)resetHistory=true;
        previousCameraPosition[0]=c.cameraPos.x;previousCameraPosition[1]=c.cameraPos.y;previousCameraPosition[2]=c.cameraPos.z;
        c.cameraUp={inverse._21,inverse._22,inverse._23};c.cameraFwd={inverse._31*state.projection._34,inverse._32*state.projection._34,inverse._33*state.projection._34};
        c.cameraNear=std::abs(state.projection._43/state.projection._33);
        c.cameraFar=std::abs(state.projection._43/(state.projection._33-state.projection._34));
        c.cameraFOV=2*std::atan(1/std::abs(state.projection._22));
        c.cameraAspectRatio=float(state.viewport.Width)/state.viewport.Height;c.jitterOffset={state.jitter[0],state.jitter[1]};
        c.mvecScale={1,1};c.cameraPinholeOffset={0,0};c.depthInverted=sl::Boolean::eFalse;c.cameraMotionIncluded=sl::Boolean::eTrue;
        c.motionVectors3D=sl::Boolean::eFalse;c.reset=resetHistory ? sl::Boolean::eTrue : sl::Boolean::eFalse;return c;
    }
    HRESULT endWorld(){if(!world)return S_OK;
        auto c=camera();HRESULT hr=context->scene.finish(c,frameIndex);world=false;state.jitter[0]=state.jitter[1]=0;resetHistory=false;
        if(SUCCEEDED(hr) && ++worldFrames==32)log(context->dlss.mode()==NeuralMode::Off ? "32 native W3D world frames completed" : "32 native W3D DLSS/DLAA world frames completed");
        return FAILED(hr) ? hr : bind();}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;
        if(id==IID_IUnknown || id==IID_IDirect3DDevice8){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel()override{return context->device.device()->GetDeviceRemovedReason();}
    UINT STDMETHODCALLTYPE GetAvailableTextureMem()override{return 1024u*1024u*1024u;}
    HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D8** out)override{if(!out)return E_POINTER;*out=parent.Get();(*out)->AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8* out)override{if(!out)return E_POINTER;caps(out);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE* out)override{if(!out)return E_POINTER;*out={parameters.BackBufferWidth,parameters.BackBufferHeight,60,D3DFMT_X8R8G8B8};return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* out)override{if(!out)return E_POINTER;*out={0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING};return S_OK;}
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* p)override{if(!p || context->device.commands())return D3DERR_INVALIDCALL;
        const auto mode=context->dlss.mode();
        context->scene.shutdown();sceneReady=false;HRESULT hr=context->device.resize(p->BackBufferWidth,p->BackBufferHeight);if(FAILED(hr))return hr;
        hr=context->dlss.configure(mode,context->device.width(),context->device.height());if(FAILED(hr))return hr;
        parameters=*p;back.Attach(new Surface8(meta(p->BackBufferWidth,p->BackBufferHeight,1,D3DFMT_A8R8G8B8,0,D3DPOOL_DEFAULT),0,true));
        defaultDepth.Reset();hr=CreateDepthStencilSurface(p->BackBufferWidth,p->BackBufferHeight,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&defaultDepth);if(FAILED(hr))return hr;
        target.Reset();zTarget.Reset();
        state.viewport={0,0,p->BackBufferWidth,p->BackBufferHeight,0,1};resetHistory=true;history.clear();return S_OK;}
    HRESULT STDMETHODCALLTYPE Present(const RECT*,const RECT*,HWND,const RGNDATA*)override{
        HRESULT hr=frame();if(FAILED(hr))return hr;hr=endWorld();if(FAILED(hr))return hr;
        context->dlss.beforePresent();std::vector<uint8_t> pixels;char path[MAX_PATH]{};
        const bool capture=GetEnvironmentVariableA("GENERALS_CAPTURE_FRAME",path,MAX_PATH)>0 && (frameIndex%120==0 || GetEnvironmentVariableA("GENERALS_CAPTURE_EVERY_FRAME",nullptr,0)>0);
        hr=context->device.endFrame(0,capture ? &pixels : nullptr);context->dlss.afterPresent();inScene=false;
        if(SUCCEEDED(hr) && capture){std::string name=std::string(path)+".native.bmp";FILE* file=nullptr;if(fopen_s(&file,name.c_str(),"wb")==0){
            BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);
            header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());info.biSize=sizeof(info);info.biWidth=LONG(context->device.width());info.biHeight=-LONG(context->device.height());
            info.biPlanes=1;info.biBitCount=32;for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
            fwrite(&header,1,sizeof(header),file);fwrite(&info,1,sizeof(info),file);fwrite(pixels.data(),1,pixels.size(),file);fclose(file);}}
        ++frameIndex;if(frameIndex==1)log("First native W3D Present completed");
        if(frameIndex==32)log(GetModuleHandleW(L"d3d12.dll") && !GetModuleHandleW(L"d3d8.dll") && !GetModuleHandleW(L"d3d9.dll") ?
            "Native runtime audit: D3D12 present, D3D8/D3D9 absent" : "Native runtime audit failed");
        if(frameIndex%120==0){for(auto i=history.begin();i!=history.end();){if(i->second.frame+10<frameIndex)i=history.erase(i);else ++i;}}
        return hr;
    }
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT n,D3DBACKBUFFER_TYPE,IDirect3DSurface8** out)override{if(!out || n)return D3DERR_INVALIDCALL;*out=back.Get();(*out)->AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS* out)override{if(!out)return E_POINTER;*out={FALSE,0};return S_OK;}
    void STDMETHODCALLTYPE SetGammaRamp(DWORD,const D3DGAMMARAMP*)override{}void STDMETHODCALLTYPE GetGammaRamp(D3DGAMMARAMP* out)override{if(out)for(UINT i=0;i<256;++i)out->red[i]=out->green[i]=out->blue[i]=WORD(i*257);}
    void STDMETHODCALLTYPE SetCursorPosition(UINT,UINT,DWORD)override{}BOOL STDMETHODCALLTYPE ShowCursor(BOOL b)override{return ::ShowCursor(b)>=0;}
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT,UINT,IDirect3DSurface8*)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT w,UINT h,UINT levels,DWORD usage,D3DFORMAT f,D3DPOOL pool,IDirect3DTexture8** out)override{
        if(!out)return E_POINTER;*out=nullptr;auto m=meta(w,h,levels,f,usage,pool);if(!m)return D3DERR_NOTAVAILABLE;*out=new Texture8(m);return S_OK;}
    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT length,DWORD usage,DWORD fvf,D3DPOOL pool,IDirect3DVertexBuffer8** out)override{
        if(!out || !length)return D3DERR_INVALIDCALL;D3DVERTEXBUFFER_DESC d{D3DFMT_VERTEXDATA,D3DRTYPE_VERTEXBUFFER,usage,pool,length,fvf};*out=new VertexBuffer8(context,d);return S_OK;}
    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT length,DWORD usage,D3DFORMAT f,D3DPOOL pool,IDirect3DIndexBuffer8** out)override{
        if(!out || !length || (f!=D3DFMT_INDEX16 && f!=D3DFMT_INDEX32))return D3DERR_INVALIDCALL;D3DINDEXBUFFER_DESC d{f,D3DRTYPE_INDEXBUFFER,usage,pool,length};*out=new IndexBuffer8(context,d);return S_OK;}
    HRESULT surface(UINT w,UINT h,D3DFORMAT f,DWORD usage,IDirect3DSurface8** out){if(!out)return E_POINTER;*out=nullptr;auto m=meta(w,h,1,f,usage,D3DPOOL_DEFAULT);
        if(!m)return D3DERR_NOTAVAILABLE;*out=new Surface8(m);return S_OK;}
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT w,UINT h,D3DFORMAT f,D3DMULTISAMPLE_TYPE ms,BOOL,IDirect3DSurface8** out)override{
        if(ms!=D3DMULTISAMPLE_NONE)return D3DERR_NOTAVAILABLE;return surface(w,h,f,D3DUSAGE_RENDERTARGET,out);}
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT w,UINT h,D3DFORMAT f,D3DMULTISAMPLE_TYPE ms,IDirect3DSurface8** out)override{
        if(ms!=D3DMULTISAMPLE_NONE)return D3DERR_NOTAVAILABLE;return surface(w,h,f,D3DUSAGE_DEPTHSTENCIL,out);}
    HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT w,UINT h,D3DFORMAT f,IDirect3DSurface8** out)override{return surface(w,h,f,0,out);}
    HRESULT STDMETHODCALLTYPE CopyRects(IDirect3DSurface8* source,const RECT* rects,UINT count,IDirect3DSurface8* destination,const POINT* points)override{
        if(!source || !destination)return D3DERR_INVALIDCALL;auto* a=static_cast<Surface8*>(source);auto* b=static_cast<Surface8*>(destination);
        if(a->meta->format!=b->meta->format)return D3DERR_INVALIDCALL;auto& from=a->meta->data->levels[a->level];auto& to=b->meta->data->levels[b->level];
        if(!count){if(from.rowBytes!=to.rowBytes || from.rows!=to.rows)return D3DERR_INVALIDCALL;to.bytes=from.bytes;}
        else{if(!rects || compressed(a->meta->format))return E_NOTIMPL;const UINT size=pixelBytes(a->meta->format);
            for(UINT i=0;i<count;++i){const auto& r=rects[i];const POINT p=points ? points[i] : POINT{r.left,r.top};
                if(r.left<0 || r.top<0 || r.right<r.left || r.bottom<r.top || UINT(r.right)>from.width || UINT(r.bottom)>from.height || p.x<0 || p.y<0 || UINT(p.x+r.right-r.left)>to.width || UINT(p.y+r.bottom-r.top)>to.height)return D3DERR_INVALIDCALL;
                for(LONG y=0;y<r.bottom-r.top;++y)std::memcpy(to.bytes.data()+size_t(p.y+y)*to.rowBytes+p.x*size,from.bytes.data()+size_t(r.top+y)*from.rowBytes+r.left*size,size_t(r.right-r.left)*size);
            }}b->meta->data->dirty=true;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture8* from,IDirect3DBaseTexture8* to)override{
        if(!from || !to || from->GetType()!=D3DRTYPE_TEXTURE || to->GetType()!=D3DRTYPE_TEXTURE)return D3DERR_INVALIDCALL;
        auto a=static_cast<Texture8*>(from)->meta->data,b=static_cast<Texture8*>(to)->meta->data;if(a->format!=b->format || a->levels.size()!=b->levels.size())return D3DERR_INVALIDCALL;
        for(size_t i=0;i<a->levels.size();++i){if(a->levels[i].bytes.size()!=b->levels[i].bytes.size())return D3DERR_INVALIDCALL;b->levels[i].bytes=a->levels[i].bytes;}b->dirty=true;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirect3DSurface8* t,IDirect3DSurface8* z)override{
        HRESULT hr=frame();if(FAILED(hr))return hr;
        if(target && !static_cast<Surface8*>(target.Get())->back){auto data=static_cast<Surface8*>(target.Get())->meta->data;textureBarrier(context->device.commands(),*data,D3D12_RESOURCE_STATE_COMMON);}
        target=t;zTarget=z;
        D3DSURFACE_DESC desc{};auto* bound=t ? t : back.Get();HRESULT description=bound->GetDesc(&desc);if(FAILED(description))return description;
        state.viewport={0,0,desc.Width,desc.Height,0,1};return bind();}
    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirect3DSurface8** out)override{if(!out)return E_POINTER;*out=target ? target.Get() : back.Get();(*out)->AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface8** out)override{if(!out)return E_POINTER;*out=zTarget ? zTarget.Get() : defaultDepth.Get();(*out)->AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE BeginScene()override{
        if(inScene)return D3DERR_INVALIDCALL;
        if(!sceneReady && !context->device.commands()){HRESULT hr=context->scene.initialize();if(FAILED(hr))return hr;sceneReady=true;}
        inScene=true;return frame();}
    HRESULT STDMETHODCALLTYPE EndScene()override{inScene=false;return endWorld();}
    HRESULT STDMETHODCALLTYPE Clear(DWORD,const D3DRECT*,DWORD flags,D3DCOLOR packed,float z,DWORD stencil)override;
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE type,const D3DMATRIX* matrix)override{if(!matrix || UINT(type)>=512)return D3DERR_INVALIDCALL;
        transforms[type]=*matrix;if(type==D3DTS_WORLD)state.world=*matrix;else if(type==D3DTS_VIEW)state.view=*matrix;else if(type==D3DTS_PROJECTION)state.projection=*matrix;
        else if(type>=D3DTS_TEXTURE0 && type<D3DTS_TEXTURE0+4)state.textureMatrix[type-D3DTS_TEXTURE0]=*matrix;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE type,D3DMATRIX* matrix)override{if(!matrix || UINT(type)>=512)return D3DERR_INVALIDCALL;*matrix=transforms[type];return S_OK;}
    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE type,const D3DMATRIX* matrix)override{if(!matrix || UINT(type)>=512)return D3DERR_INVALIDCALL;
        using namespace DirectX;D3DMATRIX result;XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&result),XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(matrix))*XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&transforms[type])));return SetTransform(type,&result);}
    HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT8* v)override{if(!v || !v->Width || !v->Height)return D3DERR_INVALIDCALL;state.viewport=*v;return context->device.commands() ? bind() : S_OK;}
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT8* v)override{if(!v)return E_POINTER;*v=state.viewport;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL8* m)override{if(!m)return E_POINTER;state.material=*m;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL8* m)override{if(!m)return E_POINTER;*m=state.material;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetLight(DWORD i,const D3DLIGHT8* l)override{if(i>=8 || !l)return D3DERR_INVALIDCALL;state.lights[i]=*l;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetLight(DWORD i,D3DLIGHT8* l)override{if(i>=8 || !l)return D3DERR_INVALIDCALL;*l=state.lights[i];return S_OK;}
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD i,BOOL enabled)override{if(i>=8)return D3DERR_INVALIDCALL;state.lightEnabled[i]=enabled;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD i,BOOL* enabled)override{if(i>=8 || !enabled)return D3DERR_INVALIDCALL;*enabled=state.lightEnabled[i];return S_OK;}
    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD i,const float* p)override{if(i>=6 || !p)return D3DERR_INVALIDCALL;std::memcpy(state.clipPlanes[i],p,16);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD i,float* p)override{if(i>=6 || !p)return D3DERR_INVALIDCALL;std::memcpy(p,state.clipPlanes[i],16);return S_OK;}
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE type,DWORD value)override{if(UINT(type)>=256)return D3DERR_INVALIDCALL;state.render[type]=value;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE type,DWORD* value)override{if(UINT(type)>=256 || !value)return D3DERR_INVALIDCALL;*value=state.render[type];return S_OK;}
    HRESULT STDMETHODCALLTYPE BeginStateBlock()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE EndStateBlock(DWORD* out)override{return CreateStateBlock(D3DSBT_ALL,out);}
    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE,DWORD* out)override{if(!out)return E_POINTER;Snapshot s{};s.state=state;std::memcpy(s.transforms,transforms,sizeof(transforms));s.vertexFormat=vertexFormat;
        const DWORD id=nextBlock++;blocks[id]=s;*out=id;return S_OK;}
    HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD id)override{auto found=blocks.find(id);if(found==blocks.end())return D3DERR_INVALIDCALL;state=found->second.state;
        std::memcpy(transforms,found->second.transforms,sizeof(transforms));vertexFormat=found->second.vertexFormat;return S_OK;}
    HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD id)override{auto found=blocks.find(id);if(found==blocks.end())return D3DERR_INVALIDCALL;found->second.state=state;std::memcpy(found->second.transforms,transforms,sizeof(transforms));found->second.vertexFormat=vertexFormat;return S_OK;}
    HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD id)override{return blocks.erase(id) ? S_OK : D3DERR_INVALIDCALL;}
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD i,IDirect3DBaseTexture8* t)override{if(i>=4)return t ? D3DERR_NOTAVAILABLE : S_OK;
        if(t && t->GetType()!=D3DRTYPE_TEXTURE)return D3DERR_NOTAVAILABLE;textures[i]=t;state.textures[i]=t ? static_cast<Texture8*>(t)->meta->data : nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD i,IDirect3DBaseTexture8** t)override{if(i>=4 || !t)return D3DERR_INVALIDCALL;*t=textures[i].Get();if(*t)(*t)->AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD i,D3DTEXTURESTAGESTATETYPE type,DWORD value)override{if(i>=4)return S_OK;if(UINT(type)>=32)return D3DERR_INVALIDCALL;state.stage[i][type]=value;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD i,D3DTEXTURESTAGESTATETYPE type,DWORD* value)override{if(i>=4 || UINT(type)>=32 || !value)return D3DERR_INVALIDCALL;*value=state.stage[i][type];return S_OK;}
    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* passes)override{if(!passes)return E_POINTER;*passes=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetVertexShader(DWORD fvf)override{if((fvf&0x80000000) && fvf!=0x90000001)return D3DERR_NOTAVAILABLE;
        state.waveVertex=fvf==0x90000001;vertexFormat=state.waveVertex ? D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1 : fvf;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetVertexShader(DWORD* fvf)override{if(!fvf)return E_POINTER;*fvf=state.waveVertex ? 0x90000001 : vertexFormat;return S_OK;}
    HRESULT STDMETHODCALLTYPE DeleteVertexShader(DWORD shader)override{if(shader!=0x90000001)return D3DERR_INVALIDCALL;if(state.waveVertex)state.waveVertex=false;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetPixelShader(DWORD shader)override{if(shader && (shader<0x80000001 || shader>0x80000004))return D3DERR_NOTAVAILABLE;
        pixelShader=shader;state.waterShader=shader ? shader-0x80000000 : 0;return S_OK;}
    HRESULT STDMETHODCALLTYPE DeletePixelShader(DWORD shader)override{if(shader<0x80000001 || shader>0x80000004)return D3DERR_INVALIDCALL;
        if(pixelShader==shader){pixelShader=0;state.waterShader=0;}return S_OK;}
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstant(DWORD start,const void* values,DWORD count)override{
        if(!values || start>16 || count>16-start)return D3DERR_INVALIDCALL;std::memcpy(state.vertexConstants[start],values,size_t(count)*16);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstant(DWORD start,void* values,DWORD count)override{
        if(!values || start>16 || count>16-start)return D3DERR_INVALIDCALL;std::memcpy(values,state.vertexConstants[start],size_t(count)*16);return S_OK;}
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstant(DWORD start,const void* values,DWORD count)override{
        if(!values || start>8 || count>8-start)return D3DERR_INVALIDCALL;std::memcpy(state.pixelConstants[start],values,size_t(count)*16);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstant(DWORD start,void* values,DWORD count)override{
        if(!values || start>8 || count>8-start)return D3DERR_INVALIDCALL;std::memcpy(values,state.pixelConstants[start],size_t(count)*16);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPixelShader(DWORD* shader)override{if(!shader)return E_POINTER;*shader=pixelShader;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT i,IDirect3DVertexBuffer8* buffer,UINT stride)override{if(i>=2)return D3DERR_INVALIDCALL;streams[i]=buffer;strides[i]=stride;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT i,IDirect3DVertexBuffer8** buffer,UINT* stride)override{if(i>=2 || !buffer || !stride)return D3DERR_INVALIDCALL;*buffer=streams[i].Get();*stride=strides[i];if(*buffer)(*buffer)->AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer8* buffer,UINT base)override{indices=buffer;baseVertex=base;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer8** buffer,UINT* base)override{if(!buffer || !base)return D3DERR_INVALIDCALL;*buffer=indices.Get();*base=baseVertex;if(*buffer)(*buffer)->AddRef();return S_OK;}
    HRESULT draw(D3DPRIMITIVETYPE type,UINT primitives,const uint8_t* vertices,UINT stride,UINT vertexCount,const void* indexData,D3DFORMAT indexFormat,UINT start);
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE type,UINT start,UINT count)override{if(!streams[0] || !strides[0])return D3DERR_INVALIDCALL;
        const auto& data=static_cast<VertexBuffer8*>(streams[0].Get())->bytes;return draw(type,count,data.data(),strides[0],UINT(data.size()/strides[0]),nullptr,D3DFMT_INDEX16,start);}
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE type,UINT,UINT,UINT start,UINT count)override{if(!streams[0] || !indices || !strides[0])return D3DERR_INVALIDCALL;
        const auto& data=static_cast<VertexBuffer8*>(streams[0].Get())->bytes;const auto& index=*static_cast<IndexBuffer8*>(indices.Get());
        const UINT n=type==D3DPT_TRIANGLELIST ? count*3 : type==D3DPT_LINELIST ? count*2 : count+2;
        if(UINT64(start+n)*(index.desc.Format==D3DFMT_INDEX32 ? 4 : 2)>index.bytes.size())return D3DERR_INVALIDCALL;
        return draw(type,count,data.data(),strides[0],UINT(data.size()/strides[0]),index.bytes.data()+start*(index.desc.Format==D3DFMT_INDEX32 ? 4 : 2),index.desc.Format,baseVertex);}
    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE type,UINT count,const void* data,UINT stride)override{
        return draw(type,count,static_cast<const uint8_t*>(data),stride,type==D3DPT_TRIANGLELIST ? count*3 : type==D3DPT_LINELIST ? count*2 : count+2,nullptr,D3DFMT_INDEX16,0);}
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE type,UINT minimum,UINT vertices,UINT count,const void* indicesData,D3DFORMAT f,const void* data,UINT stride)override{
        return draw(type,count,static_cast<const uint8_t*>(data),stride,minimum+vertices,indicesData,f,0);}
};

HRESULT Device8::Clear(DWORD count,const D3DRECT* rectangles,DWORD flags,D3DCOLOR packed,float z,DWORD stencil){
    if(count && !rectangles)return D3DERR_INVALIDCALL;
    if(flags&D3DCLEAR_TARGET){clearColor[0]=float((packed>>16)&255)/255;clearColor[1]=float((packed>>8)&255)/255;clearColor[2]=float(packed&255)/255;clearColor[3]=float(packed>>24)/255;}
    HRESULT hr=frame();if(FAILED(hr))return hr;auto* commands=context->device.commands();
    std::vector<D3D12_RECT> scaledRects;const D3D12_RECT* nativeRects=reinterpret_cast<const D3D12_RECT*>(rectangles);
    if(world && (!target || static_cast<Surface8*>(target.Get())->back) && count){
        const float sx=float(state.renderWidth)/context->device.width(),sy=float(state.renderHeight)/context->device.height();scaledRects.reserve(count);
        for(UINT i=0;i<count;++i)scaledRects.push_back({LONG(std::floor(rectangles[i].x1*sx)),LONG(std::floor(rectangles[i].y1*sy)),LONG(std::ceil(rectangles[i].x2*sx)),LONG(std::ceil(rectangles[i].y2*sy))});nativeRects=scaledRects.data();}
    ID3D12Resource* resource=target && !static_cast<Surface8*>(target.Get())->back ? static_cast<Surface8*>(target.Get())->meta->data->resource.Get() :
        world ? context->scene.colorTarget() : context->device.target();
    if(flags&D3DCLEAR_TARGET){Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;D3D12_DESCRIPTOR_HEAP_DESC d{};d.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;d.NumDescriptors=1;
        hr=context->device.device()->CreateDescriptorHeap(&d,IID_PPV_ARGS(&heap));if(FAILED(hr))return hr;auto h=heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=state.colorFormat;view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipSlice=target && !static_cast<Surface8*>(target.Get())->back ? static_cast<Surface8*>(target.Get())->level : 0;
        context->device.device()->CreateRenderTargetView(resource,&view,h);commands->ClearRenderTargetView(h,clearColor,count,nativeRects);}
    ID3D12Resource* depth=nullptr;
    if(target && !static_cast<Surface8*>(target.Get())->back){if(zTarget)depth=static_cast<Surface8*>(zTarget.Get())->meta->data->resource.Get();}
    else if(world)depth=context->scene.depthTarget();else if(defaultDepth)depth=static_cast<Surface8*>(defaultDepth.Get())->meta->data->resource.Get();
    if(depth && (flags&(D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL))){Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;D3D12_DESCRIPTOR_HEAP_DESC d{};d.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;d.NumDescriptors=1;
        hr=context->device.device()->CreateDescriptorHeap(&d,IID_PPV_ARGS(&heap));if(FAILED(hr))return hr;auto h=heap->GetCPUDescriptorHandleForHeapStart();
        context->device.device()->CreateDepthStencilView(depth,nullptr,h);
        commands->ClearDepthStencilView(h,static_cast<D3D12_CLEAR_FLAGS>(((flags&D3DCLEAR_ZBUFFER) ? D3D12_CLEAR_FLAG_DEPTH : 0)|((flags&D3DCLEAR_STENCIL) ? D3D12_CLEAR_FLAG_STENCIL : 0)),z,static_cast<UINT8>(stencil),count,nativeRects);}
    return S_OK;
}
HRESULT Device8::draw(D3DPRIMITIVETYPE type,UINT primitives,const uint8_t* data,UINT stride,UINT vertexCount,const void* indexData,D3DFORMAT indexFormat,UINT start){
    if(!data || !stride || !primitives || primitives>1000000)return D3DERR_INVALIDCALL;HRESULT hr=frame();if(FAILED(hr))return hr;
    const bool triangles=type==D3DPT_TRIANGLELIST || type==D3DPT_TRIANGLESTRIP || type==D3DPT_TRIANGLEFAN;
    const UINT expanded=triangles ? primitives*3 : type==D3DPT_POINTLIST ? primitives : primitives*2;
    std::vector<MaterialVertex> vertices;vertices.reserve(expanded);state.pretransformed=(vertexFormat&D3DFVF_POSITION_MASK)==D3DFVF_XYZRHW;
    auto emit=[&](UINT source)->bool{
        UINT index=source;if(indexData){index=indexFormat==D3DFMT_INDEX32 ? static_cast<const uint32_t*>(indexData)[source] : static_cast<const uint16_t*>(indexData)[source];}
        index+=start;if(index>=vertexCount)return false;const uint8_t* bytes=data+size_t(index)*stride;UINT offset=0;MaterialVertex v;
        auto copy=[&](void* out,UINT size)->bool{if(size>stride-offset)return false;std::memcpy(out,bytes+offset,size);offset+=size;return true;};
        if(!copy(v.position,state.pretransformed ? 16 : 12))return false;
        if(state.pretransformed){const float w=v.position[3] ? 1/v.position[3] : 1;
            v.position[0]=((v.position[0]+.5f-float(state.viewport.X))/state.viewport.Width*2-1)*w;
            v.position[1]=(1-(v.position[1]+.5f-float(state.viewport.Y))/state.viewport.Height*2)*w;v.position[2]*=w;v.position[3]=w;}
        else{const DWORD position=vertexFormat&D3DFVF_POSITION_MASK;if(position>=D3DFVF_XYZB1 && position<=D3DFVF_XYZB5)offset+=((position-D3DFVF_XYZB1)/2+1)*4;}
        if(vertexFormat&D3DFVF_NORMAL)if(!copy(v.normal,12))return false;
        if(vertexFormat&D3DFVF_PSIZE)offset+=4;
        auto unpack=[](DWORD c,float* out){out[0]=float((c>>16)&255)/255;out[1]=float((c>>8)&255)/255;out[2]=float(c&255)/255;out[3]=float(c>>24)/255;};
        DWORD color=0;if(vertexFormat&D3DFVF_DIFFUSE){if(!copy(&color,4))return false;unpack(color,v.color);}
        if(vertexFormat&D3DFVF_SPECULAR){if(!copy(&color,4))return false;unpack(color,v.specular);}
        const UINT count=(vertexFormat&D3DFVF_TEXCOUNT_MASK)>>D3DFVF_TEXCOUNT_SHIFT;
        for(UINT i=0;i<count;++i){const UINT code=(vertexFormat>>(16+i*2))&3,n=code==0 ? 2 : code==1 ? 3 : code==2 ? 4 : 1;
            float uv[4]={0,0,0,1};if(!copy(uv,n*4))return false;if(i<4)std::memcpy(v.uv[i],uv,16);}
        std::memcpy(v.previous,v.position,16);vertices.push_back(v);return true;
    };
    for(UINT p=0;p<primitives;++p){if(type==D3DPT_TRIANGLELIST){if(!emit(p*3)||!emit(p*3+1)||!emit(p*3+2))return D3DERR_INVALIDCALL;}
        else if(type==D3DPT_TRIANGLESTRIP){if(!emit(p+(p&1))||!emit(p+1-(p&1))||!emit(p+2))return D3DERR_INVALIDCALL;}
        else if(type==D3DPT_TRIANGLEFAN){if(!emit(0)||!emit(p+1)||!emit(p+2))return D3DERR_INVALIDCALL;}
        else if(type==D3DPT_LINELIST){if(!emit(p*2)||!emit(p*2+1))return D3DERR_INVALIDCALL;}
        else if(type==D3DPT_LINESTRIP){if(!emit(p)||!emit(p+1))return D3DERR_INVALIDCALL;}
        else if(type==D3DPT_POINTLIST){if(!emit(p))return D3DERR_INVALIDCALL;}else return D3DERR_INVALIDCALL;}
    using namespace DirectX;D3DMATRIX current;XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&current),XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&state.world))*
        XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&state.view))*XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&state.projection)));
    state.previousTransform=current;
    if(world && !state.pretransformed){std::array<uint64_t,3> key{objectId ? objectId : reinterpret_cast<uintptr_t>(streams[0].Get()),reinterpret_cast<uintptr_t>(indexData),UINT64(start)<<32|primitives};
        auto found=history.find(key);if(found!=history.end() && found->second.frame+1==frameIndex && found->second.vertices.size()==vertices.size()){
            state.previousTransform=found->second.transform;for(size_t i=0;i<vertices.size();++i)std::memcpy(vertices[i].previous,found->second.vertices[i].position,16);}
        history[key]={current,vertices,frameIndex};}
    hr=context->materials.draw(vertices.data(),static_cast<UINT>(vertices.size()),state,triangles ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST :
        type==D3DPT_POINTLIST ? D3D_PRIMITIVE_TOPOLOGY_POINTLIST : D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    if(SUCCEEDED(hr) && state.waterShader && !(loggedWater&(1u<<state.waterShader))){loggedWater|=1u<<state.waterShader;
        char text[80];sprintf_s(text,"Native water HLSL draw completed: program %u",state.waterShader);log(text);}
    if(SUCCEEDED(hr) && state.render[D3DRS_STENCILENABLE] && !loggedShadow){loggedShadow=true;log("Native stencil shadow draw completed");}
    if(SUCCEEDED(hr) && !draws++)log("First native W3D draw completed");if(FAILED(hr)){char text[80];sprintf_s(text,"Native W3D draw failed: 0x%08lX",static_cast<unsigned long>(hr));log(text);}return hr;
}

struct Base8 final:Base8Methods {
    std::atomic<ULONG> refs{1};std::vector<D3DDISPLAYMODE> modes;
    DXGI_ADAPTER_DESC1 adapter{};
    Base8(){const UINT sizes[][2]={{800,600},{1024,768},{1280,720},{1920,1080},{2560,1440},{3440,1440},{3840,2160}};
        for(const auto& s:sizes)modes.push_back({s[0],s[1],60,D3DFMT_X8R8G8B8});DEVMODEW mode{};mode.dmSize=sizeof(mode);
        if(EnumDisplaySettingsW(nullptr,ENUM_CURRENT_SETTINGS,&mode))modes.push_back({mode.dmPelsWidth,mode.dmPelsHeight,mode.dmDisplayFrequency,D3DFMT_X8R8G8B8});
        Microsoft::WRL::ComPtr<IDXGIFactory6> factory;if(SUCCEEDED(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)))){
            for(UINT i=0;;++i){Microsoft::WRL::ComPtr<IDXGIAdapter1> candidate;
                if(FAILED(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&candidate))))break;
                DXGI_ADAPTER_DESC1 desc{};candidate->GetDesc1(&desc);if(!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) && SUCCEEDED(D3D12CreateDevice(candidate.Get(),D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),nullptr))){adapter=desc;break;}}
        }
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown || id==IID_IDirect3D8){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    UINT STDMETHODCALLTYPE GetAdapterCount()override{return 1;}
    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT i,DWORD,D3DADAPTER_IDENTIFIER8* out)override{if(i || !out)return D3DERR_INVALIDCALL;*out={};strcpy_s(out->Driver,"NativeD3D12");
        WideCharToMultiByte(CP_ACP,0,adapter.Description,-1,out->Description,sizeof(out->Description),nullptr,nullptr);
        out->VendorId=adapter.VendorId;out->DeviceId=adapter.DeviceId;out->SubSysId=adapter.SubSysId;out->Revision=adapter.Revision;return S_OK;}
    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT i)override{return i ? 0 : static_cast<UINT>(modes.size());}
    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT i,UINT mode,D3DDISPLAYMODE* out)override{if(i || !out || mode>=modes.size())return D3DERR_INVALIDCALL;*out=modes[mode];return S_OK;}
    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT i,D3DDISPLAYMODE* out)override{if(i || !out)return D3DERR_INVALIDCALL;*out=modes.back();return S_OK;}
    HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT i,D3DDEVTYPE type,D3DFORMAT,D3DFORMAT,BOOL)override{return !i && type==D3DDEVTYPE_HAL ? S_OK : D3DERR_NOTAVAILABLE;}
    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT i,D3DDEVTYPE,D3DFORMAT,DWORD,D3DRESOURCETYPE type,D3DFORMAT f)override{
        return !i && type!=D3DRTYPE_CUBETEXTURE && type!=D3DRTYPE_VOLUMETEXTURE && format(f)!=DXGI_FORMAT_UNKNOWN ? S_OK : D3DERR_NOTAVAILABLE;}
    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT,D3DDEVTYPE,D3DFORMAT,BOOL,D3DMULTISAMPLE_TYPE type)override{return type==D3DMULTISAMPLE_NONE ? S_OK : D3DERR_NOTAVAILABLE;}
    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT,D3DDEVTYPE,D3DFORMAT,D3DFORMAT,D3DFORMAT f)override{return format(f)!=DXGI_FORMAT_UNKNOWN ? S_OK : D3DERR_NOTAVAILABLE;}
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT i,D3DDEVTYPE,D3DCAPS8* out)override{if(i || !out)return D3DERR_INVALIDCALL;caps(out);return S_OK;}
    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT)override{return MonitorFromWindow(GetDesktopWindow(),MONITOR_DEFAULTTOPRIMARY);}
    HRESULT STDMETHODCALLTYPE CreateDevice(UINT i,D3DDEVTYPE type,HWND focus,DWORD,D3DPRESENT_PARAMETERS* p,IDirect3DDevice8** out)override{
        if(i || type!=D3DDEVTYPE_HAL || !out)return D3DERR_INVALIDCALL;*out=nullptr;auto* device=new Device8(this);HRESULT hr=device->initialize(p,focus);
        if(FAILED(hr)){device->Release();char text[100]{};sprintf_s(text,"Native initialization failed 0x%08lX",static_cast<unsigned long>(hr));log(text);return hr;}*out=device;return S_OK;}
};
}
using namespace generals_mods::native12::w3d;
extern "C" __declspec(dllexport) IDirect3D8* WINAPI Direct3DCreate8(UINT){return new Base8;}
extern "C" __declspec(dllexport) BOOL WINAPI GeneralsNativeCreateWaterShaders(IDirect3DDevice8* device,DWORD* waveVertex,DWORD* wavePixel,DWORD* river,DWORD* reflection,DWORD* grid){
    if(!device || !waveVertex || !wavePixel || !river || !reflection || !grid)return FALSE;
    // These handles select compiled native HLSL programs, never legacy bytecode.
    *waveVertex=0x90000001;*wavePixel=0x80000001;*river=0x80000002;*reflection=0x80000003;*grid=0x80000004;
    log("Native HLSL water programs connected: waves, river, reflection, grid");return TRUE;
}
extern "C" __declspec(dllexport) BOOL WINAPI GeneralsNeuralAvailable(IDirect3DDevice8* d){if(!d)return FALSE;auto* device=static_cast<Device8*>(d);
    if(device->context->device.commands())return device->context->dlss.module()!=nullptr;
    const auto mode=device->context->dlss.mode();HRESULT hr=device->context->dlss.configure(generals_mods::native12::NeuralMode::DLAA,device->context->device.width(),device->context->device.height());
    if(SUCCEEDED(hr))device->context->dlss.configure(mode,device->context->device.width(),device->context->device.height());return SUCCEEDED(hr);}
extern "C" __declspec(dllexport) BOOL WINAPI GeneralsNeuralSetMode(IDirect3DDevice8* d,UINT mode){if(!d || mode>2)return FALSE;auto* device=static_cast<Device8*>(d);
    if(device->context->device.commands())return FALSE;device->context->scene.shutdown();device->sceneReady=false;device->resetHistory=true;
    HRESULT hr=device->context->dlss.configure(static_cast<generals_mods::native12::NeuralMode>(mode),device->context->device.width(),device->context->device.height());
    char text[120];sprintf_s(text,"Native neural mode %u, render %ux%u, output %ux%u: 0x%08lX",mode,device->context->dlss.renderWidth(),device->context->dlss.renderHeight(),device->context->device.width(),device->context->device.height(),static_cast<unsigned long>(hr));log(text);
    return SUCCEEDED(hr);}
extern "C" __declspec(dllexport) UINT WINAPI GeneralsNeuralGetMode(IDirect3DDevice8* d){return d ? static_cast<UINT>(static_cast<Device8*>(d)->context->dlss.mode()) : 0;}
extern "C" __declspec(dllexport) HRESULT WINAPI GeneralsNeuralBegin(IDirect3DDevice8* d,uint64_t scene){return d ? static_cast<Device8*>(d)->beginWorld(scene) : E_INVALIDARG;}
extern "C" __declspec(dllexport) HRESULT WINAPI GeneralsNeuralEnd(IDirect3DDevice8* d){return d ? static_cast<Device8*>(d)->endWorld() : E_INVALIDARG;}
extern "C" __declspec(dllexport) void WINAPI GeneralsNeuralObject(IDirect3DDevice8* d,uint64_t identity){if(d)static_cast<Device8*>(d)->objectId=identity;}
extern "C" __declspec(dllexport) void WINAPI GeneralsNeuralResetHistory(IDirect3DDevice8* d){if(d){auto* device=static_cast<Device8*>(d);device->resetHistory=true;device->history.clear();}}
extern "C" __declspec(dllexport) void WINAPI GeneralsNeuralAbort(IDirect3DDevice8* d){if(d)static_cast<Device8*>(d)->endWorld();}
extern "C" __declspec(dllexport) void WINAPI GeneralsNeuralShutdown(IDirect3DDevice8* d){if(d){auto* device=static_cast<Device8*>(d);device->endWorld();
    if(device->context->device.commands())device->context->device.endFrame();device->context->scene.shutdown();device->context->dlss.shutdown();device->sceneReady=false;}log("Explicit native shutdown completed");}
extern "C" __declspec(dllexport) void WINAPI GeneralsNeuralFinalize(){log("Native backend finalized");}
