#include <d3d8.hpp>
#include <d3dx9.hpp>
#include <d3d9on12.h>
#include <wrl/client.h>
#include <cstdio>
#include <stdexcept>
#include "../../renderer/NeuralBridgeClient.h"
using Microsoft::WRL::ComPtr;

static void check(HRESULT value, const char* operation)
{
    if (FAILED(value))
    {
        fprintf(stderr, "FAIL: %s: 0x%08lX\n", operation, static_cast<unsigned long>(value));
        throw std::runtime_error(operation);
    }
}

static void renderFrame(IDirect3DDevice8* device, UINT width, UINT height)
{
    check(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0), "clear");
    check(device->BeginScene(), "begin scene");
    check(device->SetRenderState(D3DRS_LIGHTING, FALSE), "disable lighting");
    check(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "disable culling");
    check(device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE), "vertex declaration");
    struct Vertex { float x, y, z, rhw; DWORD color; };
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const Vertex triangle[] = {
        {w * .1f, h * .1f, .5f, 1, D3DCOLOR_XRGB(255, 0, 0)},
        {w * .9f, h * .1f, .5f, 1, D3DCOLOR_XRGB(255, 0, 0)},
        {w * .5f, h * .9f, .5f, 1, D3DCOLOR_XRGB(255, 0, 0)}
    };
    check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, sizeof(Vertex)), "draw triangle");
    check(device->EndScene(), "end scene");
    ComPtr<IDirect3DSurface8> back, readback;
    check(device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &back), "back buffer");
    check(device->CreateImageSurface(width, height, D3DFMT_A8R8G8B8, &readback), "readback surface");
    check(device->CopyRects(back.Get(), nullptr, 0, readback.Get(), nullptr), "GPU readback");
    D3DLOCKED_RECT locked = {};
    check(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY), "read pixels");
    const auto pixel = [&](UINT x, UINT y) {
        return *reinterpret_cast<const DWORD*>(static_cast<const char*>(locked.pBits) + y * locked.Pitch + x * 4);
    };
    const DWORD center = pixel(width / 2, height / 2) & 0xffffff;
    const DWORD corner = pixel(1, 1) & 0xffffff;
    check(readback->UnlockRect(), "unlock pixels");
    if (center != 0xff0000 || corner != 0x0000ff) throw std::runtime_error("triangle pixel mismatch");
    check(device->Present(nullptr, nullptr, nullptr, nullptr), "present");
    printf("PASS: %u x %u triangle, depth, readback, D3D12 presentation\n", width, height);
}

static void testHandles(IDirect3DDevice8* device)
{
    struct GuardedHandle { DWORD value = 0; DWORD canary = 0xA5D3C8F1; } state, vertex, pixel;
    const auto guard = [](const GuardedHandle& value) {
        if (!value.value || value.canary != 0xA5D3C8F1) throw std::runtime_error("DWORD handle memory overwritten");
    };
    check(device->CreateStateBlock(D3DSBT_ALL, &state.value), "create state block"); guard(state);
    check(device->CaptureStateBlock(state.value), "capture state block");
    check(device->ApplyStateBlock(state.value), "apply state block");
    check(device->DeleteStateBlock(state.value), "delete state block");
    check(device->BeginStateBlock(), "begin state block");
    check(device->SetRenderState(D3DRS_LIGHTING, FALSE), "record state");
    check(device->EndStateBlock(&state.value), "end state block"); guard(state);
    check(device->ApplyStateBlock(state.value), "apply recorded block");
    check(device->DeleteStateBlock(state.value), "delete recorded block");
    // D3D8 declaration tokens: stream 0, POSITION/FLOAT3, end.
    const DWORD declaration[] = {(1u << 29), (2u << 29) | (2u << 16), D3DVSD_END()};
    check(device->CreateVertexShader(declaration, nullptr, &vertex.value, 0), "create vertex declaration handle"); guard(vertex);
    check(device->SetVertexShader(vertex.value), "set vertex declaration handle");
    check(device->DeleteVertexShader(vertex.value), "delete vertex declaration handle");
    HMODULE compiler = LoadLibraryExW(L"d3dx9_43.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!compiler) throw std::runtime_error("shader compiler");
    const auto assemble = reinterpret_cast<PFN_D3DXAssembleShader>(GetProcAddress(compiler, "D3DXAssembleShader"));
    if (!assemble) throw std::runtime_error("shader assembler export");
    const char shader[] = "ps.1.1\nmov r0, v0\n";
    ComPtr<ID3DXBuffer> code, errors;
    check(assemble(shader, sizeof(shader) - 1, nullptr, nullptr, 0, &code, &errors), "assemble pixel shader");
    check(device->CreatePixelShader(static_cast<const DWORD*>(code->GetBufferPointer()), &pixel.value), "create pixel shader handle"); guard(pixel);
    check(device->SetPixelShader(pixel.value), "set pixel shader handle");
    DWORD size = 0;
    check(device->GetPixelShaderFunction(pixel.value, nullptr, &size), "get pixel shader bytecode size");
    if (!size) throw std::runtime_error("empty pixel bytecode");
    check(device->DeletePixelShader(pixel.value), "delete pixel shader handle");
    code.Reset(); errors.Reset(); FreeLibrary(compiler);
    puts("PASS: state blocks, vertex declarations, pixel shaders preserve DWORD slots and full pointers");
}

static void testNeural(IDirect3DDevice8* device,UINT width,UINT height)
{
    using namespace generals_mods;
    if(!neuralAvailable(device)) throw std::runtime_error("DLSS runtime unavailable");
    struct Vertex {float x,y,z;DWORD color;float u,v;};
    Vertex vertices[]={{-1,-1,2,0xffff0000,0,0},{1,-1,2,0xffff0000,1,0},{0,1,2,0xffff0000,.5f,1}};
    ComPtr<IDirect3DVertexBuffer8> buffer;
    check(device->CreateVertexBuffer(sizeof(vertices),D3DUSAGE_WRITEONLY,D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1,D3DPOOL_MANAGED,&buffer),"neural vertices");
    BYTE* bytes=nullptr;check(buffer->Lock(0,sizeof(vertices),&bytes,0),"neural vertex lock");std::memcpy(bytes,vertices,sizeof(vertices));check(buffer->Unlock(),"neural vertex unlock");
    D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
    D3DVIEWPORT8 viewport{0,0,width,height*3/4,0,1};
    D3DMATRIX projection{};projection._11=float(viewport.Height)/width;projection._22=1;projection._33=100.0f/99.9f;projection._34=1;projection._43=-10.0f/99.9f;
    uint64_t hashes[3]={}; unsigned mixedEdges[3]={};
    UINT bounds[3][4]{};
    for(UINT mode:{0u,1u,2u,1u}) {
        if(!neuralSetMode(device,mode) || neuralMode(device)!=mode) throw std::runtime_error("mode switching");
        for(UINT frame=0;frame<32;++frame) {
            if(mode && !neuralBegin(device,123)) throw std::runtime_error("neural begin");
            check(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff0000ff,1,0),"neural clear");check(device->BeginScene(),"neural begin scene");
            check(device->SetViewport(&viewport),"partial world viewport");
            D3DVIEWPORT8 reported{};check(device->GetViewport(&reported),"native viewport query");
            if(reported.Width!=viewport.Width || reported.Height!=viewport.Height) throw std::runtime_error("native viewport dimensions changed");
            check(device->SetTransform(D3DTS_WORLD,&identity),"neural world");check(device->SetTransform(D3DTS_VIEW,&identity),"neural view");check(device->SetTransform(D3DTS_PROJECTION,&projection),"neural projection");
            check(device->SetRenderState(D3DRS_LIGHTING,FALSE),"neural lighting");check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"neural culling");
            check(device->SetRenderState(D3DRS_ZENABLE,TRUE),"neural depth");check(device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE),"neural depth write");
            check(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"neural blend");check(device->SetVertexShader(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1),"neural declaration");
            check(device->SetStreamSource(0,buffer.Get(),sizeof(Vertex)),"neural stream");neuralObject(device,42);
            check(device->DrawPrimitive(D3DPT_TRIANGLELIST,0,1),"neural triangle");neuralObject(device,0);
            if(mode) neuralEnd(device);
            if(neuralMode(device)!=mode) throw std::runtime_error("neural evaluation disabled mode");
            D3DRECT rect{0,0,32,32};check(device->Clear(1,&rect,D3DCLEAR_TARGET,0xff00ff00,1,0),"native HUD");check(device->EndScene(),"neural end scene");
            ComPtr<IDirect3DSurface8> target,readback;check(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&target),"neural output");
            check(device->CreateImageSurface(width,height,D3DFMT_A8R8G8B8,&readback),"neural readback");check(device->CopyRects(target.Get(),nullptr,0,readback.Get(),nullptr),"neural copy output");
            D3DLOCKED_RECT locked{};check(readback->LockRect(&locked,nullptr,D3DLOCK_READONLY),"neural lock output");
            const DWORD hud=*static_cast<const DWORD*>(locked.pBits)&0xffffff;
            const DWORD center=*reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(locked.pBits)+size_t(viewport.Height/2)*locked.Pitch+(width/2)*4)&0xffffff;
            if(frame==31) {
                hashes[mode]=1469598103934665603ull; mixedEdges[mode]=0;
                bounds[mode][0]=width;bounds[mode][1]=height;bounds[mode][2]=bounds[mode][3]=0;
                for(UINT y=32;y<height;++y) {
                    const auto* row=reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(locked.pBits)+size_t(y)*locked.Pitch);
                    for(UINT x=32;x<width;++x) {
                        const DWORD rgb=row[x]&0xffffff;
                        hashes[mode]=(hashes[mode]^rgb)*1099511628211ull;
                        const unsigned r=(rgb>>16)&255,b=rgb&255;
                        if(r>200 && b<55) {bounds[mode][0]=std::min(bounds[mode][0],x);bounds[mode][1]=std::min(bounds[mode][1],y);bounds[mode][2]=std::max(bounds[mode][2],x);bounds[mode][3]=std::max(bounds[mode][3],y);}
                        if(r>5 && r<250 && b>5 && b<250) ++mixedEdges[mode];
                    }
                }
            }
            check(readback->UnlockRect(),"neural unlock output");
            if(hud!=0x00ff00 || ((center>>16)&255)<240 || ((center>>8)&255)>15 || (center&255)>15) throw std::runtime_error("neural triangle/HUD pixel mismatch");
            check(device->Present(nullptr,nullptr,nullptr,nullptr),"neural present");
        }
    }
    printf("Edge coverage: Off=%u DLSS=%u DLAA=%u\n",mixedEdges[0],mixedEdges[1],mixedEdges[2]);
    for(UINT mode=1;mode<=2;++mode) for(UINT edge=0;edge<4;++edge)
        if(std::abs(int(bounds[mode][edge])-int(bounds[0][edge]))>3) throw std::runtime_error("partial viewport distorted object bounds");
    puts("PASS: partial world viewport preserves object proportions and native viewport queries");
    if(hashes[0]==hashes[1] || hashes[0]==hashes[2] || hashes[1]==hashes[2] ||
        mixedEdges[1]<=mixedEdges[0] || mixedEdges[2]<=mixedEdges[0])
        throw std::runtime_error("AA modes did not change resolved edge coverage");
    if(!neuralSetMode(device,0)) throw std::runtime_error("disable neural mode");
    check(device->SetStreamSource(0,nullptr,0),"release neural stream");
    printf("PASS: %ux%u DLSS/DLAA live switching, temporal frames, HUD pixels, disable and resource cleanup\n",width,height);
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc != 2 && argc != 3) return 2;
        HMODULE bridge = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!bridge) throw std::runtime_error("load bridge");
        using Create = IDirect3D8* (WINAPI*)(UINT);
        const auto create = reinterpret_cast<Create>(GetProcAddress(bridge, "Direct3DCreate8"));
        if (!create) throw std::runtime_error("bridge export");
        ComPtr<IDirect3D8> renderer;
        renderer.Attach(create(220));
        if (!renderer) throw std::runtime_error("create bridge renderer");
        WNDCLASSW windowClass = {};
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"GeneralsD3D12Tests";
        if (!RegisterClassW(&windowClass)) throw std::runtime_error("register test window");
        HWND window = CreateWindowW(windowClass.lpszClassName, L"D3D12 test", WS_POPUP,
            0, 0, 64, 64, nullptr, nullptr, windowClass.hInstance, nullptr);
        if (!window) throw std::runtime_error("create hidden test window");
        D3DPRESENT_PARAMETERS8 options = {};
        options.BackBufferWidth = 3440;
        options.BackBufferHeight = 1440;
        options.BackBufferFormat = D3DFMT_A8R8G8B8;
        options.BackBufferCount = 1;
        options.SwapEffect = D3DSWAPEFFECT_DISCARD;
        options.hDeviceWindow = window;
        options.Windowed = TRUE;
        options.EnableAutoDepthStencil = TRUE;
        options.AutoDepthStencilFormat = D3DFMT_D24S8;
        ComPtr<IDirect3DDevice8> device;
        check(renderer->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &options, &device), "create device");
        ComPtr<IDirect3DDevice9On12> interop;
        check(device.As(&interop), "require 9On12 interop");
        ComPtr<ID3D12Device> nativeDevice;
        check(interop->GetD3D12Device(IID_PPV_ARGS(&nativeDevice)), "require native D3D12 device");
        testHandles(device.Get());
        renderFrame(device.Get(), 3440, 1440);
        if(argc==3) testNeural(device.Get(),3440,1440);
        options.MultiSampleType=D3DMULTISAMPLE_8_SAMPLES;
        check(device->Reset(&options), "switch from neural AA to high MSAA");
        {
            ComPtr<IDirect3DSurface8> target; D3DSURFACE_DESC8 description{};
            check(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&target),"MSAA target");
            check(target->GetDesc(&description),"MSAA description");
            if(description.MultiSampleType!=D3DMULTISAMPLE_8_SAMPLES) throw std::runtime_error("MSAA was not applied to the live target");
        }
        options.MultiSampleType=D3DMULTISAMPLE_NONE;
        options.BackBufferWidth = 3840;
        options.BackBufferHeight = 2160;
        check(device->Reset(&options), "reset to 4K");
        renderFrame(device.Get(), 3840, 2160);
        if(argc==3) testNeural(device.Get(),3840,2160);
        generals_mods::neuralShutdown(device.Get());
        nativeDevice.Reset(); interop.Reset();
        IDirect3DDevice8* finalDevice=device.Detach();
        if(finalDevice->Release()!=0) throw std::runtime_error("device references leaked after neural resources released");
        renderer.Reset();
        DestroyWindow(window);
        UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        FreeLibrary(bridge);
        puts("PASS: bridge resources released");
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
