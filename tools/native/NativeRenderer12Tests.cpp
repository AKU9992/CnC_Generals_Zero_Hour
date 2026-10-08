#include "../../renderer/native12/NativeDevice12.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

using generals_mods::native12::Device;
using generals_mods::native12::Vertex;
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void check(HRESULT hr, const char* operation) {
    if (FAILED(hr)) {
        char text[256]{};
        sprintf_s(text,"%s: HRESULT 0x%08lX",operation,static_cast<unsigned long>(hr));
        throw std::runtime_error(text);
    }
}
static constexpr float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static constexpr float background[4]={0.125f,0.25f,0.5f,1};
static constexpr Vertex triangle[]={
    {{-0.75f,-0.75f,0.5f},{1,0,0,1}},
    {{ 0.00f, 0.75f,0.5f},{1,0,0,1}},
    {{ 0.75f,-0.75f,0.5f},{1,0,0,1}}
};
static void inspect(const std::vector<uint8_t>& pixels,UINT width,UINT height) {
    require(pixels.size()==static_cast<size_t>(width)*height*4,"incorrect readback size");
    auto* center=pixels.data()+(static_cast<size_t>(height/2)*width+width/2)*4;
    require(center[0]>250 && center[1]<3 && center[2]<3 && center[3]>250,"native triangle not rendered");
    auto* corner=pixels.data()+(static_cast<size_t>(height/16)*width+width/16)*4;
    require(std::abs(int(corner[0])-32)<=1 && std::abs(int(corner[1])-64)<=1 &&
            std::abs(int(corner[2])-128)<=1 && corner[3]==255,"background or RGBA readback incorrect");
    size_t red=0;
    for(size_t i=0;i<pixels.size();i+=4) if(pixels[i]>250 && pixels[i+1]<3 && pixels[i+2]<3) ++red;
    const double coverage=static_cast<double>(red)/(static_cast<double>(width)*height);
    require(coverage>0.27 && coverage<0.29,"triangle dimensions or viewport incorrect");
    std::printf("native draw/readback %ux%u: coverage %.4f\n",width,height,coverage);
}
int main(int argc,char** argv) {
    const bool debug=argc>1 && std::string(argv[1])=="--debug";
    HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSW wc{}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=instance;
    wc.lpszClassName=L"GeneralsNative12Tests";
    if(!RegisterClassW(&wc)) return 2;
    HWND window=CreateWindowExW(0,wc.lpszClassName,L"Native D3D12 validation",WS_OVERLAPPEDWINDOW,
        0,0,640,480,nullptr,nullptr,instance,nullptr);
    if(!window) {UnregisterClassW(wc.lpszClassName,instance); return 2;}
    int result=0;
    try {
        Device renderer;
        require(renderer.initialize(nullptr,640,480,debug)==E_INVALIDARG,"invalid HWND accepted");
        for(UINT cycle=0;cycle<2;++cycle) {
            check(renderer.initialize(window,640,480,debug),"initialize");
            std::printf("AMD64 hardware adapter: %ls; vendor 0x%04X\n",
                renderer.adapterDescription().Description,renderer.adapterDescription().VendorId);
            require(renderer.drawTriangles(triangle,3,identity)==E_INVALIDARG,"draw outside frame accepted");
            require(renderer.resize(0,480)==E_INVALIDARG,"zero width accepted");
            const UINT dimensions[][2]={{640,480},{3440,1440},{3840,2160},{641,479},{640,480}};
            for(const auto& size:dimensions) {
                check(renderer.resize(size[0],size[1]),"resize");
                // More than the number of allocators; capture only after the ring has wrapped.
                for(UINT frame=0;frame<9;++frame) {
                    check(renderer.beginFrame(background),"beginFrame");
                    require(renderer.beginFrame(background)==E_UNEXPECTED,"nested frame accepted");
                    require(renderer.resize(800,600)==E_INVALIDARG,"resize during frame accepted");
                    require(renderer.drawTriangles(triangle,2,identity)==E_INVALIDARG,"invalid triangle count accepted");
                    check(renderer.drawTriangles(triangle,3,identity),"drawTriangles");
                    if(frame==8) {
                        std::vector<uint8_t> pixels;
                        check(renderer.endFrame(0,&pixels),"endFrame/readback");
                        inspect(pixels,size[0],size[1]);
                    } else check(renderer.endFrame(),"endFrame");
                }
            }
            check(renderer.waitIdle(),"waitIdle");
            if(debug) check(renderer.checkDebugErrors(),"D3D12 debug layer");
            renderer.shutdown();
        }
        std::puts("PASS: native D3D12 draw, GPU pixels, frame ring, resize, AMD64, recreate");
        std::puts("Game W3D integration and DLSS are separate checks.");
    } catch(const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());result=1;}
    DestroyWindow(window);UnregisterClassW(wc.lpszClassName,instance);
    return result;
}
