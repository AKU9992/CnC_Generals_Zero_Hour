#pragma once
#include "D3D9On12Resources.h"
#include <dxgi1_6.h>
#include <cstdio>
#include <memory>

namespace generals_mods {
// Give the D3D12 output its own HWND: one flip swap chain per HWND is allowed.
// The parent still owns game input; the child only displays the finished frame.
class NativePresentation12 {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
public:
    ~NativePresentation12() { reset(); }
    void reset() {
        if(resources_) resources_->wait();
        commands_.Reset(); allocator_.Reset(); swap_.Reset(); resources_.reset(); resolved_.Reset();
        if(window_) {
            DestroyWindow(window_); window_=nullptr;
            UnregisterClassW(L"GeneralsGPTD3D12Output",GetModuleHandleW(L"generals-d3d12.dll"));
        }
        width_=height_=0;
    }
    HRESULT present(IDirect3DDevice9* device) {
        Ptr<IDirect3DSurface9> source9;
        HRESULT hr=device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&source9);
        D3DSURFACE_DESC desc{};
        if(SUCCEEDED(hr)) hr=source9->GetDesc(&desc);
        if(FAILED(hr)) return hr;
        if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE) {
            if(!resolved_ || width_!=desc.Width || height_!=desc.Height) {
                resolved_.Reset();
                hr=device->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&resolved_,nullptr);
                if(FAILED(hr)) return hr;
            }
            hr=device->StretchRect(source9.Get(),nullptr,resolved_.Get(),nullptr,D3DTEXF_NONE);
            if(FAILED(hr)) return hr;
            source9=resolved_; desc.MultiSampleType=D3DMULTISAMPLE_NONE;
        }
        if(!swap_ || width_!=desc.Width || height_!=desc.Height) {
            reset(); hr=initialize(device,desc); if(FAILED(hr)) {reset(); return hr;}
        }
        hr=resources_->wait();
        if(FAILED(hr)) return hr;
        Ptr<ID3D12Resource> source, destination;
        hr=swap_->GetBuffer(swap_->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&destination));
        if(FAILED(hr)) return hr;
        hr=allocator_->Reset();
        if(SUCCEEDED(hr)) hr=commands_->Reset(allocator_.Get(),nullptr);
        if(FAILED(hr)) return hr;
        Ptr<ID3D12Resource> readback;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 readbackSize=0;
        char capturePath[MAX_PATH]={};
        const DWORD pathLength=GetEnvironmentVariableA("GENERALS_CAPTURE_FRAME",capturePath,sizeof(capturePath));
        const bool capture=pathLength && pathLength<MAX_PATH-12 && GetTickCount()-lastCapture_>=5000;
        if(capture) {
            const auto targetDesc=destination->GetDesc();
            resources_->device()->GetCopyableFootprints(&targetDesc,0,1,0,&footprint,nullptr,nullptr,&readbackSize);
            D3D12_HEAP_PROPERTIES heap{}; heap.Type=D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC buffer{}; buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
            buffer.Width=readbackSize; buffer.Height=1; buffer.DepthOrArraySize=1;
            buffer.MipLevels=1; buffer.SampleDesc.Count=1; buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            hr=resources_->device()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,
                D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback));
            if(FAILED(hr)) {commands_->Close(); return hr;}
        }
        hr=resources_->acquire(source9.Get(),&source);
        if(FAILED(hr)) {commands_->Close(); return hr;}
        barrier(source.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE);
        barrier(destination.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST);
        commands_->CopyResource(destination.Get(),source.Get());
        barrier(source.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);
        if(capture) {
            barrier(destination.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION output{}; output.pResource=readback.Get();
            output.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; output.PlacedFootprint=footprint;
            D3D12_TEXTURE_COPY_LOCATION input{}; input.pResource=destination.Get();
            input.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            commands_->CopyTextureRegion(&output,0,0,0,&input,nullptr);
            barrier(destination.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT);
        } else barrier(destination.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PRESENT);
        hr=commands_->Close();
        if(FAILED(hr)) {resources_->submit(nullptr); return hr;}
        hr=resources_->submit(commands_.Get());
        if(FAILED(hr)) return hr;
        // DXGI waits on this queue's copy before displaying its destination.
        hr=swap_->Present(0,0);
        if(SUCCEEDED(hr) && capture) {
            HRESULT captureResult=resources_->wait();
            void* bytes=nullptr; D3D12_RANGE range{0,static_cast<SIZE_T>(readbackSize)};
            if(SUCCEEDED(captureResult)) captureResult=readback->Map(0,&range,&bytes);
            if(SUCCEEDED(captureResult)) {
                strcat_s(capturePath,".native.bmp");
                FILE* file=nullptr;
                if(fopen_s(&file,capturePath,"wb")==0 && file) {
                    BITMAPFILEHEADER header{}; BITMAPINFOHEADER info{};
                    header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(info);
                    header.bfSize=header.bfOffBits+width_*height_*4;
                    info.biSize=sizeof(info); info.biWidth=width_; info.biHeight=-static_cast<LONG>(height_);
                    info.biPlanes=1; info.biBitCount=32; info.biCompression=BI_RGB;
                    fwrite(&header,sizeof(header),1,file); fwrite(&info,sizeof(info),1,file);
                    for(UINT y=0;y<height_;++y) fwrite(static_cast<const char*>(bytes)+footprint.Offset+y*footprint.Footprint.RowPitch,4,width_,file);
                    fclose(file);
                } else captureResult=E_FAIL;
                D3D12_RANGE written{0,0}; readback->Unmap(0,&written);
            }
            log("Native DXGI output readback",captureResult);
            lastCapture_=GetTickCount();
        }
        if(SUCCEEDED(hr) && ++frames_==32) log("32 native DXGI frames presented",hr);
        if(FAILED(hr)) log("Native DXGI presentation failed",hr);
        return hr==DXGI_ERROR_DEVICE_REMOVED || hr==DXGI_ERROR_DEVICE_RESET ? D3DERR_DEVICELOST:hr;
    }
private:
    static void log(const char* message,HRESULT hr) {
        FILE* file=nullptr;
        if(fopen_s(&file,"GeneralsD3D12.log","a")==0 && file) {
            fprintf(file,"PID %lu: %s: 0x%08lX\n",GetCurrentProcessId(),message,static_cast<unsigned long>(hr)); fclose(file);
        }
    }
    static LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
        if(message==WM_NCHITTEST) return HTTRANSPARENT;
        if(message==WM_ERASEBKGND) return 1;
        return DefWindowProcW(window,message,wParam,lParam);
    }
    HRESULT initialize(IDirect3DDevice9* device,const D3DSURFACE_DESC& desc) {
        Ptr<IDirect3DSwapChain9> legacy;
        D3DPRESENT_PARAMETERS parameters{};
        HRESULT hr=device->GetSwapChain(0,&legacy);
        if(SUCCEEDED(hr)) hr=legacy->GetPresentParameters(&parameters);
        if(FAILED(hr)) return hr;
        if(!parameters.Windowed) return D3DERR_INVALIDCALL;
        const wchar_t* name=L"GeneralsGPTD3D12Output";
        const HINSTANCE module=GetModuleHandleW(L"generals-d3d12.dll");
        WNDCLASSW windowClass{};
        windowClass.hInstance=module; windowClass.lpfnWndProc=windowProc;
        windowClass.lpszClassName=name;
        if(!RegisterClassW(&windowClass) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)
            return HRESULT_FROM_WIN32(GetLastError());
        RECT rect{}; GetClientRect(parameters.hDeviceWindow,&rect);
        window_=CreateWindowExW(WS_EX_NOPARENTNOTIFY,name,L"",WS_CHILD|WS_VISIBLE,
            0,0,rect.right,rect.bottom,parameters.hDeviceWindow,nullptr,module,nullptr);
        if(!window_) return HRESULT_FROM_WIN32(GetLastError());
        resources_=std::make_unique<D3D9On12Resources>();
        hr=resources_->initialize(device);
        Ptr<IDXGIFactory2> factory;
        if(SUCCEEDED(hr)) hr=CreateDXGIFactory1(IID_PPV_ARGS(&factory));
        DXGI_SWAP_CHAIN_DESC1 options{};
        options.Width=desc.Width; options.Height=desc.Height;
        options.Format=desc.Format==D3DFMT_A8R8G8B8 ? DXGI_FORMAT_B8G8R8A8_UNORM:
            desc.Format==D3DFMT_A8B8G8R8 ? DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_UNKNOWN;
        if(options.Format==DXGI_FORMAT_UNKNOWN) return D3DERR_INVALIDCALL;
        options.SampleDesc.Count=1; options.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        options.BufferCount=2; options.Scaling=DXGI_SCALING_STRETCH;
        options.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD; options.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
        Ptr<IDXGISwapChain1> swap;
        if(SUCCEEDED(hr)) hr=factory->CreateSwapChainForHwnd(resources_->queue(),window_,&options,nullptr,nullptr,&swap);
        if(SUCCEEDED(hr)) hr=swap.As(&swap_);
        if(SUCCEEDED(hr)) factory->MakeWindowAssociation(window_,DXGI_MWA_NO_ALT_ENTER);
        if(SUCCEEDED(hr)) hr=resources_->device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator_));
        if(SUCCEEDED(hr)) hr=resources_->device()->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator_.Get(),nullptr,IID_PPV_ARGS(&commands_));
        if(SUCCEEDED(hr)) hr=commands_->Close();
        if(SUCCEEDED(hr)) {width_=desc.Width; height_=desc.Height; frames_=0;}
        log("Native DXGI child output initialization",hr);
        return hr;
    }
    void barrier(ID3D12Resource* resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER value{}; value.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        value.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};
        commands_->ResourceBarrier(1,&value);
    }
    std::unique_ptr<D3D9On12Resources> resources_;
    Ptr<IDXGISwapChain3> swap_;
    Ptr<ID3D12CommandAllocator> allocator_;
    Ptr<ID3D12GraphicsCommandList> commands_;
    Ptr<IDirect3DSurface9> resolved_;
    HWND window_=nullptr;
    UINT width_=0,height_=0,frames_=0;
    DWORD lastCapture_=GetTickCount();
};
}
