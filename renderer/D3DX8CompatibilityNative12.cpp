// CPU-side W3D helper ABI. Creates resources through the native device interface.
#include <d3dx8.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <fstream>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
using namespace DirectX;

static XMMATRIX matrix(const D3DXMATRIX* m){return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(m));}
static D3DXMATRIX* store(D3DXMATRIX* out,FXMMATRIX m){if(out)XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(out),m);return out;}
D3DXMATRIX* WINAPI D3DXMatrixMultiply(D3DXMATRIX* out,const D3DXMATRIX* a,const D3DXMATRIX* b){return out && a && b ? store(out,matrix(a)*matrix(b)) : nullptr;}
D3DXMATRIX* WINAPI D3DXMatrixInverse(D3DXMATRIX* out,FLOAT* determinant,const D3DXMATRIX* m){if(!out || !m)return nullptr;XMVECTOR d;auto inverse=XMMatrixInverse(&d,matrix(m));
    const float value=XMVectorGetX(d);if(determinant)*determinant=value;if(value==0)return nullptr;return store(out,inverse);}
D3DXMATRIX* WINAPI D3DXMatrixTranspose(D3DXMATRIX* out,const D3DXMATRIX* m){return out && m ? store(out,XMMatrixTranspose(matrix(m))) : nullptr;}
D3DXMATRIX* WINAPI D3DXMatrixScaling(D3DXMATRIX* out,FLOAT x,FLOAT y,FLOAT z){return store(out,XMMatrixScaling(x,y,z));}
D3DXMATRIX* WINAPI D3DXMatrixTranslation(D3DXMATRIX* out,FLOAT x,FLOAT y,FLOAT z){return store(out,XMMatrixTranslation(x,y,z));}
D3DXMATRIX* WINAPI D3DXMatrixRotationZ(D3DXMATRIX* out,FLOAT angle){return store(out,XMMatrixRotationZ(angle));}
D3DXVECTOR4* WINAPI D3DXVec3Transform(D3DXVECTOR4* out,const D3DXVECTOR3* v,const D3DXMATRIX* m){if(!out || !v || !m)return nullptr;
    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(out),XMVector3Transform(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(v)),matrix(m)));return out;}
D3DXVECTOR4* WINAPI D3DXVec4Transform(D3DXVECTOR4* out,const D3DXVECTOR4* v,const D3DXMATRIX* m){if(!out || !v || !m)return nullptr;
    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(out),XMVector4Transform(XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(v)),matrix(m)));return out;}
UINT WINAPI D3DXGetFVFVertexSize(DWORD f){UINT n=(f&D3DFVF_POSITION_MASK)==D3DFVF_XYZRHW ? 16 : 12;
    DWORD position=f&D3DFVF_POSITION_MASK;if(position>=D3DFVF_XYZB1 && position<=D3DFVF_XYZB5)n+=((position-D3DFVF_XYZB1)/2+1)*4;
    if(f&D3DFVF_NORMAL)n+=12;if(f&D3DFVF_PSIZE)n+=4;if(f&D3DFVF_DIFFUSE)n+=4;if(f&D3DFVF_SPECULAR)n+=4;
    UINT count=(f&D3DFVF_TEXCOUNT_MASK)>>D3DFVF_TEXCOUNT_SHIFT;for(UINT i=0;i<count;++i){UINT c=(f>>(16+i*2))&3;n+=(c==0 ? 2 : c==1 ? 3 : c==2 ? 4 : 1)*4;}return n;}
HRESULT WINAPI D3DXGetErrorStringA(HRESULT code,LPSTR text,UINT length){if(!text || !length)return E_INVALIDARG;
    if(!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,text,length,nullptr))snprintf(text,length,"HRESULT 0x%08lX",static_cast<unsigned long>(code));return S_OK;}
HRESULT WINAPI D3DXAssembleShader(LPCVOID,UINT,DWORD,LPD3DXBUFFER* constants,LPD3DXBUFFER* shader,LPD3DXBUFFER* errors){
    if(constants)*constants=nullptr;if(shader)*shader=nullptr;if(errors)*errors=nullptr;return E_NOTIMPL;}
HRESULT WINAPI D3DXCreateTexture(IDirect3DDevice8* d,UINT w,UINT h,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture8** out){
    if(!d)return E_INVALIDARG;if(format==D3DFMT_UNKNOWN)format=D3DFMT_A8R8G8B8;return d->CreateTexture(w,h,levels,usage,format,pool,out);}
HRESULT WINAPI D3DXCreateCubeTexture(IDirect3DDevice8* d,UINT edge,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DCubeTexture8** out){return d ? d->CreateCubeTexture(edge,levels,usage,format,pool,out) : E_INVALIDARG;}
HRESULT WINAPI D3DXCreateVolumeTexture(IDirect3DDevice8* d,UINT w,UINT h,UINT depth,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DVolumeTexture8** out){return d ? d->CreateVolumeTexture(w,h,depth,levels,usage,format,pool,out) : E_INVALIDARG;}

namespace {
struct Pixel{unsigned r=0,g=0,b=0,a=255;};
Pixel rgb565(unsigned c){return {((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31,255};}
Pixel read(const uint8_t* bytes,int pitch,D3DFORMAT format,UINT x,UINT y){
    if(format>=D3DFMT_DXT1 && format<=D3DFMT_DXT5){const UINT size=format==D3DFMT_DXT1 ? 8 : 16;
        const uint8_t* block=bytes+size_t(y/4)*pitch+(x/4)*size,*colors=block+(size==16 ? 8 : 0);
        const UINT a=colors[0]|UINT(colors[1])<<8,b=colors[2]|UINT(colors[3])<<8;Pixel c[4]={rgb565(a),rgb565(b)};
        if(a>b || size==16){c[2]={(2*c[0].r+c[1].r)/3,(2*c[0].g+c[1].g)/3,(2*c[0].b+c[1].b)/3,255};
            c[3]={(c[0].r+2*c[1].r)/3,(c[0].g+2*c[1].g)/3,(c[0].b+2*c[1].b)/3,255};}
        else{c[2]={(c[0].r+c[1].r)/2,(c[0].g+c[1].g)/2,(c[0].b+c[1].b)/2,255};c[3]={0,0,0,0};}
        uint32_t bits=0;std::memcpy(&bits,colors+4,4);const UINT pixel=(y%4)*4+x%4;Pixel p=c[(bits>>(pixel*2))&3];
        if(format==D3DFMT_DXT2 || format==D3DFMT_DXT3){uint64_t alpha=0;std::memcpy(&alpha,block,8);p.a=UINT((alpha>>(pixel*4))&15)*17;}
        else if(format==D3DFMT_DXT4 || format==D3DFMT_DXT5){UINT alpha[8]={block[0],block[1]};if(alpha[0]>alpha[1])for(UINT i=2;i<8;++i)alpha[i]=((8-i)*alpha[0]+(i-1)*alpha[1])/7;
            else{for(UINT i=2;i<6;++i)alpha[i]=((6-i)*alpha[0]+(i-1)*alpha[1])/5;alpha[6]=0;alpha[7]=255;}
            uint64_t values=0;std::memcpy(&values,block+2,6);p.a=alpha[(values>>(pixel*3))&7];}
        return p;
    }
    const uint8_t* row=bytes+size_t(y)*pitch;
    if(format==D3DFMT_A8R8G8B8 || format==D3DFMT_X8R8G8B8){auto* p=row+x*4;return {p[2],p[1],p[0],format==D3DFMT_A8R8G8B8 ? p[3] : 255u};}
    if(format==D3DFMT_L8)return {row[x],row[x],row[x],255};if(format==D3DFMT_A8)return {255,255,255,row[x]};
    if(format==D3DFMT_A8L8)return {row[x*2],row[x*2],row[x*2],row[x*2+1]};
    unsigned packed=0;std::memcpy(&packed,row+x*2,2);if(format==D3DFMT_R5G6B5)return rgb565(packed);
    if(format==D3DFMT_A1R5G5B5 || format==D3DFMT_X1R5G5B5)return {((packed>>10)&31)*255/31,((packed>>5)&31)*255/31,(packed&31)*255/31,format==D3DFMT_X1R5G5B5 || packed&0x8000 ? 255u : 0u};
    if(format==D3DFMT_A4R4G4B4)return {((packed>>8)&15)*17,((packed>>4)&15)*17,(packed&15)*17,((packed>>12)&15)*17};return {};
}
bool write(uint8_t* bytes,int pitch,D3DFORMAT f,UINT x,UINT y,Pixel p){auto* row=bytes+size_t(y)*pitch;
    if(f==D3DFMT_A8R8G8B8 || f==D3DFMT_X8R8G8B8){auto* out=row+x*4;out[0]=uint8_t(p.b);out[1]=uint8_t(p.g);out[2]=uint8_t(p.r);out[3]=uint8_t(f==D3DFMT_X8R8G8B8 ? 255 : p.a);return true;}
    if(f==D3DFMT_A8 || f==D3DFMT_L8){row[x]=uint8_t(f==D3DFMT_A8 ? p.a : (p.r+p.g+p.b)/3);return true;}
    if(f==D3DFMT_A8L8){row[x*2]=uint8_t((p.r+p.g+p.b)/3);row[x*2+1]=uint8_t(p.a);return true;}
    unsigned packed=0;if(f==D3DFMT_R5G6B5)packed=(p.r*31/255)<<11|(p.g*63/255)<<5|(p.b*31/255);
    else if(f==D3DFMT_A1R5G5B5 || f==D3DFMT_X1R5G5B5)packed=(p.r*31/255)<<10|(p.g*31/255)<<5|(p.b*31/255)|((f==D3DFMT_X1R5G5B5 || p.a>=128) ? 0x8000 : 0);
    else if(f==D3DFMT_A4R4G4B4)packed=(p.a/17)<<12|(p.r/17)<<8|(p.g/17)<<4|p.b/17;else return false;
    std::memcpy(row+x*2,&packed,2);return true;
}
}
HRESULT WINAPI D3DXLoadSurfaceFromSurface(IDirect3DSurface8* dst,const PALETTEENTRY*,const RECT* destination,IDirect3DSurface8* src,const PALETTEENTRY*,const RECT* source,DWORD filter,D3DCOLOR key){
    if(!dst || !src)return E_INVALIDARG;D3DSURFACE_DESC a{},b{};HRESULT hr=src->GetDesc(&a);if(SUCCEEDED(hr))hr=dst->GetDesc(&b);if(FAILED(hr))return hr;
    RECT from=source ? *source : RECT{0,0,LONG(a.Width),LONG(a.Height)},to=destination ? *destination : RECT{0,0,LONG(b.Width),LONG(b.Height)};
    if(from.left<0 || from.top<0 || from.right<=from.left || from.bottom<=from.top || UINT(from.right)>a.Width || UINT(from.bottom)>a.Height ||
       to.left<0 || to.top<0 || to.right<=to.left || to.bottom<=to.top || UINT(to.right)>b.Width || UINT(to.bottom)>b.Height)return E_INVALIDARG;
    D3DLOCKED_RECT in{},out{};hr=src->LockRect(&in,nullptr,D3DLOCK_READONLY);if(FAILED(hr))return hr;
    hr=dst->LockRect(&out,nullptr,0);if(FAILED(hr)){src->UnlockRect();return hr;}
    bool good=true;const UINT width=UINT(to.right-to.left),height=UINT(to.bottom-to.top),inputWidth=UINT(from.right-from.left),inputHeight=UINT(from.bottom-from.top);
    for(UINT y=0;y<height && good;++y)for(UINT x=0;x<width;++x){UINT x0=UINT(from.left)+x*inputWidth/width,y0=UINT(from.top)+y*inputHeight/height;
        Pixel p=read(static_cast<const uint8_t*>(in.pBits),in.Pitch,a.Format,x0,y0);
        if((filter&255)==D3DX_FILTER_BOX){UINT x1=UINT(from.left)+(x+1)*inputWidth/width,y1=UINT(from.top)+(y+1)*inputHeight/height;
            uint64_t r=0,g=0,bColor=0,alpha=0,count=0;for(UINT sy=y0;sy<std::max(y0+1,y1);++sy)for(UINT sx=x0;sx<std::max(x0+1,x1);++sx){Pixel sample=read(static_cast<const uint8_t*>(in.pBits),in.Pitch,a.Format,sx,sy);r+=sample.r;g+=sample.g;bColor+=sample.b;alpha+=sample.a;++count;}
            p={UINT(r/count),UINT(g/count),UINT(bColor/count),UINT(alpha/count)};}
        if(key && ((p.r<<16)|(p.g<<8)|p.b)==(key&0xffffff))p.a=0;
        good=write(static_cast<uint8_t*>(out.pBits),out.Pitch,b.Format,UINT(to.left)+x,UINT(to.top)+y,p);if(!good)break;
    }
    dst->UnlockRect();src->UnlockRect();return good ? S_OK : D3DERR_NOTAVAILABLE;
}
HRESULT WINAPI D3DXFilterTexture(IDirect3DBaseTexture8* texture,const PALETTEENTRY*,UINT level,DWORD filter){
    if(!texture || texture->GetType()!=D3DRTYPE_TEXTURE)return E_INVALIDARG;auto* t=static_cast<IDirect3DTexture8*>(texture);
    if(level>=t->GetLevelCount())return E_INVALIDARG;
    for(UINT i=level+1;i<t->GetLevelCount();++i){IDirect3DSurface8* from=nullptr,*to=nullptr;HRESULT hr=t->GetSurfaceLevel(i-1,&from);
        if(SUCCEEDED(hr))hr=t->GetSurfaceLevel(i,&to);if(SUCCEEDED(hr))hr=D3DXLoadSurfaceFromSurface(to,nullptr,nullptr,from,nullptr,nullptr,filter,0);
        if(from)from->Release();if(to)to->Release();if(FAILED(hr))return hr;}return S_OK;
}
HRESULT WINAPI D3DXCreateTextureFromFileExA(IDirect3DDevice8* device,LPCSTR path,UINT width,UINT height,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,DWORD filter,DWORD mipFilter,D3DCOLOR key,D3DXIMAGE_INFO* info,PALETTEENTRY*,IDirect3DTexture8** out){
    if(!out)return E_POINTER;*out=nullptr;if(!device || !path)return E_INVALIDARG;
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return D3DXERR_INVALIDDATA;
    const auto length=file.tellg();if(length<=0 || length>INT_MAX)return D3DXERR_INVALIDDATA;
    std::vector<uint8_t> bytes(static_cast<size_t>(length));file.seekg(0);if(!file.read(reinterpret_cast<char*>(bytes.data()),length))return D3DXERR_INVALIDDATA;
    UINT sourceWidth=0,sourceHeight=0,sourceLevels=1,sourcePitch=0;D3DFORMAT sourceFormat=D3DFMT_A8R8G8B8;bool dds=false;
    std::vector<uint8_t> decoded;
    if(bytes.size()>=128 && std::memcmp(bytes.data(),"DDS ",4)==0){
        auto field=[&](UINT offset){DWORD value;std::memcpy(&value,bytes.data()+offset,4);return value;};
        if(field(4)!=124 || field(76)!=32 || field(112)!=0)return D3DXERR_INVALIDDATA; // 2D textures only
        sourceHeight=field(12);sourceWidth=field(16);sourceLevels=std::max<UINT>(1u,field(28));dds=true;
        const DWORD fourcc=field(84);
        if(fourcc==D3DFMT_DXT1 || fourcc==D3DFMT_DXT2 || fourcc==D3DFMT_DXT3 || fourcc==D3DFMT_DXT4 || fourcc==D3DFMT_DXT5)sourceFormat=static_cast<D3DFORMAT>(fourcc);
        else if(fourcc==0 && field(88)==32 && field(92)==0xff0000 && field(96)==0xff00 && field(100)==0xff)
            sourceFormat=field(104)==0xff000000 ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
        else return D3DERR_NOTAVAILABLE;
        const bool bc=sourceFormat>=D3DFMT_DXT1 && sourceFormat<=D3DFMT_DXT5;
        sourcePitch=bc ? std::max(1u,(sourceWidth+3)/4)*(sourceFormat==D3DFMT_DXT1 ? 8 : 16) : sourceWidth*4;
    }else{
        int w=0,h=0,channels=0;auto* image=stbi_load_from_memory(bytes.data(),int(bytes.size()),&w,&h,&channels,4);
        if(!image)return D3DXERR_INVALIDDATA;sourceWidth=UINT(w);sourceHeight=UINT(h);sourcePitch=sourceWidth*4;
        decoded.assign(image,image+size_t(w)*h*4);stbi_image_free(image);
        for(size_t i=0;i<decoded.size();i+=4)std::swap(decoded[i],decoded[i+2]);
    }
    if(!sourceWidth || !sourceHeight || sourceWidth>16384 || sourceHeight>16384 || sourceLevels>15)return D3DXERR_INVALIDDATA;
    if(!width || width==D3DX_DEFAULT)width=sourceWidth;
    if(!height || height==D3DX_DEFAULT)height=sourceHeight;
    if(format==D3DFMT_UNKNOWN)format=dds && !key ? sourceFormat : D3DFMT_A8R8G8B8;
    if(levels==D3DX_DEFAULT)levels=dds ? sourceLevels : 0;
    IDirect3DTexture8* texture=nullptr;HRESULT hr=device->CreateTexture(width,height,levels,usage,format,pool,&texture);if(FAILED(hr))return hr;
    const bool bc=sourceFormat>=D3DFMT_DXT1 && sourceFormat<=D3DFMT_DXT5;
    size_t offset=128;UINT loaded=0;
    if(dds && width==sourceWidth && height==sourceHeight && format==sourceFormat && !key){
        for(UINT i=0;i<std::min<UINT>(sourceLevels,texture->GetLevelCount());++i){
            const UINT w=std::max(1u,sourceWidth>>i),h=std::max(1u,sourceHeight>>i),rows=bc ? std::max(1u,(h+3)/4) : h;
            const UINT pitch=bc ? std::max(1u,(w+3)/4)*(sourceFormat==D3DFMT_DXT1 ? 8 : 16) : w*4;
            if(size_t(pitch)*rows>bytes.size()-std::min(offset,bytes.size())){hr=D3DXERR_INVALIDDATA;break;}
            D3DLOCKED_RECT lock{};hr=texture->LockRect(i,&lock,nullptr,0);if(FAILED(hr))break;
            for(UINT y=0;y<rows;++y)std::memcpy(static_cast<uint8_t*>(lock.pBits)+size_t(y)*lock.Pitch,bytes.data()+offset+size_t(y)*pitch,pitch);
            texture->UnlockRect(i);offset+=size_t(pitch)*rows;++loaded;
        }
        if(SUCCEEDED(hr) && loaded<texture->GetLevelCount()){
            if(bc)hr=D3DERR_NOTAVAILABLE;else hr=D3DXFilterTexture(texture,nullptr,loaded-1,mipFilter==D3DX_DEFAULT ? D3DX_FILTER_BOX : mipFilter);
        }
    }else{
        const UINT rows=bc ? std::max(1u,(sourceHeight+3)/4) : sourceHeight;
        if(dds && size_t(sourcePitch)*rows>bytes.size()-128)hr=D3DXERR_INVALIDDATA;
        IDirect3DSurface8* source=nullptr,*destination=nullptr;
        if(SUCCEEDED(hr))hr=device->CreateImageSurface(sourceWidth,sourceHeight,sourceFormat,&source);
        D3DLOCKED_RECT lock{};if(SUCCEEDED(hr))hr=source->LockRect(&lock,nullptr,0);
        if(SUCCEEDED(hr)){const uint8_t* input=dds ? bytes.data()+128 : decoded.data();
            for(UINT y=0;y<rows;++y)std::memcpy(static_cast<uint8_t*>(lock.pBits)+size_t(y)*lock.Pitch,input+size_t(y)*sourcePitch,sourcePitch);source->UnlockRect();
            hr=texture->GetSurfaceLevel(0,&destination);
        }
        if(SUCCEEDED(hr))hr=D3DXLoadSurfaceFromSurface(destination,nullptr,nullptr,source,nullptr,nullptr,filter==D3DX_DEFAULT ? D3DX_FILTER_BOX : filter,key);
        if(source)source->Release();if(destination)destination->Release();
        if(SUCCEEDED(hr))hr=D3DXFilterTexture(texture,nullptr,0,mipFilter==D3DX_DEFAULT ? D3DX_FILTER_BOX : mipFilter);
    }
    if(FAILED(hr)){texture->Release();return hr;}
    if(info)*info={sourceWidth,sourceHeight,1,sourceLevels,sourceFormat,D3DRTYPE_TEXTURE,dds ? D3DXIFF_DDS : D3DXIFF_TGA};
    *out=texture;return S_OK;
}
