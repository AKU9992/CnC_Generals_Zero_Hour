// Required legacy D3DX8 operations backed by the installed x64 D3DX9 runtime.
// The original x86 d3dx8.lib must never be linked into the x64 game.
#include <d3dx8.h>
#include <cstdio>
#include <cstring>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
static_assert(sizeof(D3DXMATRIX) == 64 && sizeof(D3DXVECTOR4) == 16 && sizeof(D3DXVECTOR3) == 12);

namespace {
const GUID device9 = {0xd0223b96,0xbf7a,0x43fd,{0x92,0xbd,0xa4,0x3b,0x0d,0x82,0xb9,0xeb}};
const GUID baseTexture9 = {0x580ca87e,0x1d3c,0x4d54,{0x99,0x1d,0xb7,0xd3,0xe3,0xc2,0x98,0xce}};
const GUID surface9 = {0xcfbaf3a,0x9ff6,0x429a,{0x99,0xb3,0xa2,0x79,0x6a,0xf8,0xb8,0x9b}};
template<class Function> Function resolve(const char* name)
{
    static HMODULE runtime = LoadLibraryExW(L"d3dx9_43.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return runtime ? reinterpret_cast<Function>(GetProcAddress(runtime, name)) : nullptr;
}
HRESULT proxy(IUnknown* object, REFIID iid, IUnknown** result)
{
    if (!object || !result) return E_POINTER;
    *result = nullptr;
    return object->QueryInterface(iid, reinterpret_cast<void**>(result));
}
HRESULT wrapTexture(IDirect3DDevice8* device, UINT type, IUnknown* texture, void** result)
{
    using Wrap = HRESULT (WINAPI*)(IDirect3DDevice8*, UINT, IUnknown*, void**);
    HMODULE bridge = GetModuleHandleW(L"generals-d3d12.dll");
    auto wrap = bridge ? reinterpret_cast<Wrap>(GetProcAddress(bridge, "GeneralsWrapResource9")) : nullptr;
    if (!wrap) { if (texture) texture->Release(); return E_NOINTERFACE; }
    const HRESULT status = wrap(device, type, texture, result);
    if (FAILED(status) && texture) texture->Release();
    return status;
}
}

#define FORWARD_MATH(Type, Name, Parameters, Arguments) \
Type WINAPI Name Parameters { using Function = Type (WINAPI*) Parameters; \
    auto function = resolve<Function>(#Name); return function ? function Arguments : nullptr; }
FORWARD_MATH(D3DXMATRIX*, D3DXMatrixMultiply, (D3DXMATRIX* out, const D3DXMATRIX* a, const D3DXMATRIX* b), (out,a,b))
FORWARD_MATH(D3DXMATRIX*, D3DXMatrixInverse, (D3DXMATRIX* out, FLOAT* determinant, const D3DXMATRIX* a), (out,determinant,a))
FORWARD_MATH(D3DXMATRIX*, D3DXMatrixTranspose, (D3DXMATRIX* out, const D3DXMATRIX* a), (out,a))
FORWARD_MATH(D3DXMATRIX*, D3DXMatrixScaling, (D3DXMATRIX* out, FLOAT x, FLOAT y, FLOAT z), (out,x,y,z))
FORWARD_MATH(D3DXMATRIX*, D3DXMatrixTranslation, (D3DXMATRIX* out, FLOAT x, FLOAT y, FLOAT z), (out,x,y,z))
FORWARD_MATH(D3DXMATRIX*, D3DXMatrixRotationZ, (D3DXMATRIX* out, FLOAT angle), (out,angle))
FORWARD_MATH(D3DXVECTOR4*, D3DXVec3Transform, (D3DXVECTOR4* out, const D3DXVECTOR3* vector, const D3DXMATRIX* matrix), (out,vector,matrix))
FORWARD_MATH(D3DXVECTOR4*, D3DXVec4Transform, (D3DXVECTOR4* out, const D3DXVECTOR4* vector, const D3DXMATRIX* matrix), (out,vector,matrix))
#undef FORWARD_MATH

UINT WINAPI D3DXGetFVFVertexSize(DWORD fvf)
{
    auto function = resolve<UINT (WINAPI*)(DWORD)>("D3DXGetFVFVertexSize");
    return function ? function(fvf) : 0;
}
HRESULT WINAPI D3DXGetErrorStringA(HRESULT code, LPSTR buffer, UINT length)
{
    if (!buffer || !length) return E_INVALIDARG;
    if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        code, 0, buffer, length, nullptr)) snprintf(buffer, length, "HRESULT 0x%08lX", static_cast<unsigned long>(code));
    return S_OK;
}
HRESULT WINAPI D3DXAssembleShader(LPCVOID data, UINT length, DWORD flags,
    LPD3DXBUFFER* constants, LPD3DXBUFFER* shader, LPD3DXBUFFER* errors)
{
    if (constants) { *constants = nullptr; return E_INVALIDARG; } // The game never requests this legacy output.
    using Assemble = HRESULT (WINAPI*)(LPCSTR, UINT, const void*, void*, DWORD, LPD3DXBUFFER*, LPD3DXBUFFER*);
    auto function = resolve<Assemble>("D3DXAssembleShader");
    return function ? function(static_cast<LPCSTR>(data), length, nullptr, nullptr, flags, shader, errors) : E_NOINTERFACE;
}
HRESULT WINAPI D3DXCreateTexture(IDirect3DDevice8* device, UINT width, UINT height, UINT levels,
    DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** output)
{
    if (!output) return E_POINTER; *output = nullptr;
    ComPtr<IUnknown> native; HRESULT status = proxy(device, device9, &native);
    if (FAILED(status)) return status;
    using Create = HRESULT (WINAPI*)(IUnknown*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IUnknown**);
    auto create = resolve<Create>("D3DXCreateTexture"); IUnknown* texture = nullptr;
    if (!create) return E_NOINTERFACE;
    status = create(native.Get(), width, height, levels, usage, format, pool, &texture);
    return FAILED(status) ? status : wrapTexture(device, D3DRTYPE_TEXTURE, texture, reinterpret_cast<void**>(output));
}
HRESULT WINAPI D3DXCreateCubeTexture(IDirect3DDevice8* device, UINT edge, UINT levels,
    DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DCubeTexture8** output)
{
    if (!output) return E_POINTER; *output = nullptr;
    ComPtr<IUnknown> native; HRESULT status = proxy(device, device9, &native);
    if (FAILED(status)) return status;
    using Create = HRESULT (WINAPI*)(IUnknown*, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IUnknown**);
    auto create = resolve<Create>("D3DXCreateCubeTexture"); IUnknown* texture = nullptr;
    if (!create) return E_NOINTERFACE;
    status = create(native.Get(), edge, levels, usage, format, pool, &texture);
    return FAILED(status) ? status : wrapTexture(device, D3DRTYPE_CUBETEXTURE, texture, reinterpret_cast<void**>(output));
}
HRESULT WINAPI D3DXCreateVolumeTexture(IDirect3DDevice8* device, UINT width, UINT height, UINT depth, UINT levels,
    DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DVolumeTexture8** output)
{
    if (!output) return E_POINTER; *output = nullptr;
    ComPtr<IUnknown> native; HRESULT status = proxy(device, device9, &native);
    if (FAILED(status)) return status;
    using Create = HRESULT (WINAPI*)(IUnknown*, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IUnknown**);
    auto create = resolve<Create>("D3DXCreateVolumeTexture"); IUnknown* texture = nullptr;
    if (!create) return E_NOINTERFACE;
    status = create(native.Get(), width, height, depth, levels, usage, format, pool, &texture);
    return FAILED(status) ? status : wrapTexture(device, D3DRTYPE_VOLUMETEXTURE, texture, reinterpret_cast<void**>(output));
}
HRESULT WINAPI D3DXCreateTextureFromFileExA(IDirect3DDevice8* device, LPCSTR path, UINT width, UINT height, UINT levels,
    DWORD usage, D3DFORMAT format, D3DPOOL pool, DWORD filter, DWORD mipFilter, D3DCOLOR colorKey,
    D3DXIMAGE_INFO* image, PALETTEENTRY* palette, IDirect3DTexture8** output)
{
    if (!output) return E_POINTER; *output = nullptr;
    ComPtr<IUnknown> native; HRESULT status = proxy(device, device9, &native);
    if (FAILED(status)) return status;
    using Create = HRESULT (WINAPI*)(IUnknown*, LPCSTR, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
        DWORD, DWORD, D3DCOLOR, D3DXIMAGE_INFO*, PALETTEENTRY*, IUnknown**);
    auto create = resolve<Create>("D3DXCreateTextureFromFileExA"); IUnknown* texture = nullptr;
    if (!create) return E_NOINTERFACE;
    status = create(native.Get(), path, width, height, levels, usage, format, pool, filter, mipFilter, colorKey, image, palette, &texture);
    return FAILED(status) ? status : wrapTexture(device, D3DRTYPE_TEXTURE, texture, reinterpret_cast<void**>(output));
}
HRESULT WINAPI D3DXFilterTexture(IDirect3DBaseTexture8* texture, const PALETTEENTRY* palette, UINT level, DWORD filter)
{
    ComPtr<IUnknown> native; HRESULT status = proxy(texture, baseTexture9, &native);
    if (FAILED(status)) return status;
    auto function = resolve<HRESULT (WINAPI*)(IUnknown*, const PALETTEENTRY*, UINT, DWORD)>("D3DXFilterTexture");
    return function ? function(native.Get(), palette, level, filter) : E_NOINTERFACE;
}
HRESULT WINAPI D3DXLoadSurfaceFromSurface(IDirect3DSurface8* destination, const PALETTEENTRY* destinationPalette,
    const RECT* destinationRect, IDirect3DSurface8* source, const PALETTEENTRY* sourcePalette, const RECT* sourceRect,
    DWORD filter, D3DCOLOR colorKey)
{
    ComPtr<IUnknown> destination9, source9;
    HRESULT status = proxy(destination, surface9, &destination9); if (FAILED(status)) return status;
    status = proxy(source, surface9, &source9); if (FAILED(status)) return status;
    using Load = HRESULT (WINAPI*)(IUnknown*, const PALETTEENTRY*, const RECT*, IUnknown*, const PALETTEENTRY*, const RECT*, DWORD, D3DCOLOR);
    auto function = resolve<Load>("D3DXLoadSurfaceFromSurface");
    return function ? function(destination9.Get(), destinationPalette, destinationRect, source9.Get(), sourcePalette, sourceRect, filter, colorKey) : E_NOINTERFACE;
}
