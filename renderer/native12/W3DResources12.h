#pragma once
#include "NativeDevice12.h"
#include <memory>

namespace generals_mods::native12 {
struct TextureLevel {
    UINT width=0,height=0,rowBytes=0,rows=0;
    std::vector<uint8_t> bytes;
};
struct TextureData {
    UINT width=0,height=0;
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    std::vector<TextureLevel> levels;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_RESOURCE_STATES state=D3D12_RESOURCE_STATE_COMMON;
    bool dirty=true,renderTarget=false,depthStencil=false;
    UINT componentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
};
HRESULT uploadTexture(Device& device,TextureData& texture);
HRESULT createTarget(Device& device,TextureData& texture);
void textureBarrier(ID3D12GraphicsCommandList* commands,TextureData& texture,D3D12_RESOURCE_STATES state);
}
