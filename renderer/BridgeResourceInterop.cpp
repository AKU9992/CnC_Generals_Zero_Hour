#include "d3d8to9.hpp"

// Takes ownership of the one reference returned by D3DX9 on success.
extern "C" __declspec(dllexport) HRESULT WINAPI GeneralsWrapResource9(
    IDirect3DDevice8* device, UINT type, IUnknown* resource, void** output)
{
    if (!device || !resource || !output) return E_POINTER;
    *output = nullptr;
    auto* owner = static_cast<Direct3DDevice8*>(device);
    switch (type) {
        case D3DRTYPE_TEXTURE:
            *output = static_cast<IDirect3DTexture8*>(new Direct3DTexture8(owner, static_cast<IDirect3DTexture9*>(resource))); break;
        case D3DRTYPE_CUBETEXTURE:
            *output = static_cast<IDirect3DCubeTexture8*>(new Direct3DCubeTexture8(owner, static_cast<IDirect3DCubeTexture9*>(resource))); break;
        case D3DRTYPE_VOLUMETEXTURE:
            *output = static_cast<IDirect3DVolumeTexture8*>(new Direct3DVolumeTexture8(owner, static_cast<IDirect3DVolumeTexture9*>(resource))); break;
        default: return E_INVALIDARG;
    }
    return S_OK;
}
