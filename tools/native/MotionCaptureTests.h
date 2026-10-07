#pragma once
#include "../../renderer/MotionCapture9.h"
#include <DirectXPackedVector.h>
#include <cmath>
#include <cstdio>

inline bool testMotionCapture(ID3D12Device* nativeDevice) {
    using Microsoft::WRL::ComPtr;
    HWND window = CreateWindowExW(0,L"STATIC",L"Object motion test",WS_OVERLAPPEDWINDOW,0,0,256,256,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window) return false;
    struct Owner { HWND value; ~Owner(){DestroyWindow(value);} } owner{window};
    D3D9ON12_ARGS args{}; args.Enable9On12=TRUE; args.pD3D12Device=nativeDevice;
    ComPtr<IDirect3D9> d3d; d3d.Attach(Direct3DCreate9On12(D3D_SDK_VERSION,&args,1)); if(!d3d) return false;
    D3DPRESENT_PARAMETERS pp{}; pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=pp.BackBufferHeight=256; pp.BackBufferFormat=D3DFMT_A8R8G8B8; pp.hDeviceWindow=window;
    pp.EnableAutoDepthStencil=TRUE; pp.AutoDepthStencilFormat=D3DFMT_D24S8;
    ComPtr<IDirect3DDevice9> device;
    if(FAILED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device))) return false;
    generals_mods::MotionCapture9 capture;
    HRESULT hr=capture.initialize(device.Get());
    if(FAILED(hr)){std::printf("Motion shaders: %08lX\n",static_cast<unsigned long>(hr));return false;}
    ComPtr<IDirect3DTexture9> texture;
    ComPtr<IDirect3DSurface9> motion, color, readback;
    if(FAILED(device->CreateTexture(256,256,1,D3DUSAGE_RENDERTARGET,D3DFMT_G16R16F,D3DPOOL_DEFAULT,&texture,nullptr)) ||
       FAILED(texture->GetSurfaceLevel(0,&motion)) || FAILED(device->GetRenderTarget(0,&color)) ||
       FAILED(device->CreateOffscreenPlainSurface(256,256,D3DFMT_G16R16F,D3DPOOL_SYSTEMMEM,&readback,nullptr))) return false;
    struct Vertex { float x,y,z; DWORD color; float u,v; };
    Vertex vertices[]={{-.8f,-.8f,.5f,0xffffffff,0,0},{.8f,-.8f,.5f,0xffffffff,1,0},{0,.8f,.5f,0xffffffff,.5f,1}};
    ComPtr<IDirect3DVertexBuffer9> buffer;
    if(FAILED(device->CreateVertexBuffer(sizeof(vertices),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1,D3DPOOL_DEFAULT,&buffer,nullptr))) return false;
    D3DMATRIX identity{}; identity._11=identity._22=identity._33=identity._44=1;
    device->SetTransform(D3DTS_VIEW,&identity); device->SetTransform(D3DTS_PROJECTION,&identity); capture.matrices(identity);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE); device->SetRenderState(D3DRS_LIGHTING,FALSE);
    device->SetRenderState(D3DRS_ZENABLE,TRUE); device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
    device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1); device->SetStreamSource(0,buffer.Get(),0,sizeof(Vertex));
    for(uint32_t frame=0;frame<5;++frame) {
        if(frame==2) for(auto& vertex:vertices) vertex.x+=.2f;
        void* memory=nullptr;
        if(FAILED(buffer->Lock(0,sizeof(vertices),&memory,D3DLOCK_DISCARD))) return false;
        generals_mods::captureVertexLock(buffer.Get(),0,sizeof(vertices),memory); std::memcpy(memory,vertices,sizeof(vertices));
        generals_mods::captureVertexUnlock(buffer.Get()); buffer->Unlock();
        device->SetRenderTarget(0,motion.Get()); device->Clear(0,nullptr,D3DCLEAR_TARGET,0xffffffff,1,0);
        device->SetRenderTarget(0,color.Get()); device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0);
        D3DMATRIX world=identity; if(frame) world._41=frame==4 ? .4f:.2f; device->SetTransform(D3DTS_WORLD,&world);
        D3DMATRIX view=identity; if(frame>=3) view._41=.1f; device->SetTransform(D3DTS_VIEW,&view);
        if(FAILED(device->BeginScene()) || FAILED(device->DrawPrimitive(D3DPT_TRIANGLELIST,0,1))) return false;
        capture.begin(motion.Get(),frame); capture.object(42);
        hr=capture.primitive(D3DPT_TRIANGLELIST,0,1); capture.end(); device->EndScene();
        if(hr!=(frame==3 ? S_FALSE:S_OK)){std::printf("Motion replay: %08lX\n",static_cast<unsigned long>(hr));return false;}
        // A stationary mesh uses the camera reprojection fallback. Its pose
        // must still advance, so resumed movement uses the immediately previous frame.
        if(frame==3) continue;
        if(FAILED(device->GetRenderTargetData(motion.Get(),readback.Get()))) return false;
        D3DLOCKED_RECT locked{}; if(FAILED(readback->LockRect(&locked,nullptr,D3DLOCK_READONLY))) return false;
        const UINT sampleX=frame==4 ? 192:128;
        const auto* pixel=reinterpret_cast<const DirectX::PackedVector::HALF*>(static_cast<const unsigned char*>(locked.pBits)+128*locked.Pitch+sampleX*4);
        const float x=DirectX::PackedVector::XMConvertHalfToFloat(pixel[0]), y=DirectX::PackedVector::XMConvertHalfToFloat(pixel[1]);
        readback->UnlockRect();
        const float expected=frame ? -.1f:0;
        std::printf("Motion frame %u: %.5f %.5f; expected %.5f 0\n",frame,x,y,expected);
        if(std::abs(x-expected)>.003f || std::abs(y)>.003f) return false;
    }
    generals_mods::forgetVertexBuffer(buffer.Get());
    std::puts("PASS: GPU motion vectors include object transforms and vertex animation with previous-frame history.");
    return true;
}
