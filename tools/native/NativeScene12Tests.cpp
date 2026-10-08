#include "../../renderer/native12/NativeScene12.h"
#include <DirectXMath.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <stdexcept>

using namespace generals_mods::native12;
static void check(HRESULT hr,const char* operation){
    if(FAILED(hr)){char message[256]{};sprintf_s(message,"%s HRESULT 0x%08lX",operation,static_cast<unsigned long>(hr));throw std::runtime_error(message);}
}
static sl::Constants camera(UINT width,UINT height,sl::float2 jitter,bool reset){
    using namespace DirectX;
    sl::Constants c{};XMFLOAT4X4 projection,inverse,identity;
    const auto matrix=XMMatrixPerspectiveFovLH(1,float(width)/height,.1f,1000);
    XMStoreFloat4x4(&projection,matrix);XMStoreFloat4x4(&inverse,XMMatrixInverse(nullptr,matrix));XMStoreFloat4x4(&identity,XMMatrixIdentity());
    std::memcpy(&c.cameraViewToClip,&projection,64);std::memcpy(&c.clipToCameraView,&inverse,64);
    std::memcpy(&c.clipToPrevClip,&identity,64);std::memcpy(&c.prevClipToClip,&identity,64);
    c.cameraPos={0,0,0};c.cameraUp={0,1,0};c.cameraRight={1,0,0};c.cameraFwd={0,0,1};
    c.cameraNear=.1f;c.cameraFar=1000;c.cameraFOV=1;c.cameraAspectRatio=float(width)/height;
    c.mvecScale={1,1};c.jitterOffset=jitter;c.cameraPinholeOffset={0,0};
    c.depthInverted=sl::Boolean::eFalse;c.cameraMotionIncluded=sl::Boolean::eTrue;
    c.motionVectors3D=sl::Boolean::eFalse;c.reset=reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    return c;
}
static float halton(UINT index,UINT base){float result=0,f=1;while(index){f/=float(base);result+=f*float(index%base);index/=base;}return result;}
int main(){
    HINSTANCE instance=GetModuleHandleW(nullptr);WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=instance;wc.lpszClassName=L"NativeScene12Tests";
    if(!RegisterClassW(&wc))return 2;
    HWND window=CreateWindowExW(0,wc.lpszClassName,L"Native scene validation",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,instance,nullptr);
    if(!window){UnregisterClassW(wc.lpszClassName,instance);return 2;}
    int result=0;
    try{
        Device device;Dlss dlss(device);Scene scene(device,dlss);check(device.initialize(window,640,480),"initialize");
        const UINT sizes[][2]={{640,480},{3440,1440},{3840,2160}};uint32_t frameIndex=0;
        const float clear[4]={.125f,.25f,.5f,1};
        constexpr Vertex red[]={{{-.9f,-.9f,2},{1,0,0,1}},{{0,.9f,2},{1,0,0,1}},{{.9f,-.9f,2},{1,0,0,1}}};
        constexpr Vertex blue[]={{{-3,-3,4},{0,0,1,1}},{{0,3,4},{0,0,1,1}},{{3,-3,4},{0,0,1,1}}};
        constexpr Vertex hud[]={{{-.95f,.95f,.1f},{0,1,0,1}},{{-.70f,.95f,.1f},{0,1,0,1}},{{-.95f,.70f,.1f},{0,1,0,1}}};
        constexpr float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        for(const auto& size:sizes)for(auto mode:{NeuralMode::Off,NeuralMode::DLAA,NeuralMode::DLSSQuality}){
            check(scene.shutdown(),"scene shutdown");check(device.resize(size[0],size[1]),"resize");
            check(dlss.configure(mode,size[0],size[1]),"configure neural");check(scene.initialize(),"scene initialize");
            DirectX::XMFLOAT4X4 previous{};
            for(UINT i=0;i<40;++i){
                const sl::float2 jitter=mode==NeuralMode::Off ? sl::float2{0,0} : sl::float2{halton(i+1,2)-.5f,halton(i+1,3)-.5f};
                auto c=camera(size[0],size[1],jitter,i==0);DirectX::XMFLOAT4X4 projection,current;
                std::memcpy(&projection,&c.cameraViewToClip,64);
                const auto p=DirectX::XMLoadFloat4x4(&projection);
                DirectX::XMStoreFloat4x4(&current,DirectX::XMMatrixTranslation(.15f*std::sin(float(i)*.12f),0,0)*p);
                if(i==0)previous=current;
                check(device.beginFrame(clear),"begin frame");check(scene.begin(clear),"begin scene");
                check(scene.draw(red,3,&current._11,&previous._11,jitter),"draw near object");
                // Draw the farther object second. Z must retain the red foreground.
                check(scene.draw(blue,3,&projection._11,&projection._11,jitter),"draw far object");
                check(scene.finish(c,frameIndex++),"finish/evaluate");
                // HUD must be crisp and unaffected by temporal processing.
                check(device.drawTriangles(hud,3,identity),"draw HUD");
                dlss.beforePresent();std::vector<uint8_t> pixels;
                check(device.endFrame(0,i==39 ? &pixels : nullptr),"present/readback");dlss.afterPresent();
                if(i==39){
                    const auto center=(static_cast<size_t>(size[1]/2)*size[0]+size[0]/2)*4;
                    const auto corner=(static_cast<size_t>(size[1]/100)*size[0]+size[0]/100)*4;
                    const auto overlay=(static_cast<size_t>(size[1]/25)*size[0]+size[0]/25)*4;
                    if(pixels[center]<230 || pixels[center+2]>25)throw std::runtime_error("depth occlusion / scene color failed");
                    if(std::abs(int(pixels[corner])-32)>8 || std::abs(int(pixels[corner+2])-128)>8)throw std::runtime_error("scene background failed");
                    if(pixels[overlay]>3 || pixels[overlay+1]<250 || pixels[overlay+2]>3)throw std::runtime_error("HUD after DLSS failed");
                    ScenePixel input;
                    check(scene.readPixel(dlss.renderWidth()/2,dlss.renderHeight()/2,input),"native temporal input readback");
                    const float delta=.15f*(std::sin(float(i-1)*.12f)-std::sin(float(i)*.12f));
                    const float expectedMotion=delta*projection._11/4;
                    const float expectedDepth=projection._33+projection._43/2;
                    if(std::abs(input.motionX-expectedMotion)>.00005f || std::abs(input.motionY)>.00005f ||
                       std::abs(input.depth-expectedDepth)>.0001f)throw std::runtime_error("GPU depth or object motion incorrect");
                    std::printf("PASS native scene %ux%u mode %d: motion, depth occlusion, DLSS output, HUD\n",size[0],size[1],static_cast<int>(mode));
                }
                previous=current;
            }
        }
        check(scene.shutdown(),"scene shutdown");check(dlss.shutdown(),"DLSS shutdown");
        std::puts("PASS: native scene GPU tests; W3D game integration remains separate");
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());result=1;}
    DestroyWindow(window);UnregisterClassW(wc.lpszClassName,instance);return result;
}
