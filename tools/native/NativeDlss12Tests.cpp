#include "../../renderer/native12/NativeDlss12.h"
#include "DlaaFrameTest.h"
#include <cstdio>

int main() {
    using namespace generals_mods::native12;
    const HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=instance;wc.lpszClassName=L"NativeDlss12Tests";
    if(!RegisterClassW(&wc)) return 2;
    const HWND window=CreateWindowExW(0,wc.lpszClassName,L"Native DLSS validation",WS_OVERLAPPEDWINDOW,
                                    0,0,640,480,nullptr,nullptr,instance,nullptr);
    if(!window) {UnregisterClassW(wc.lpszClassName,instance);return 2;}
    int result=0;
    {
        Device renderer;
        Dlss neural(renderer);
        HRESULT hr=renderer.initialize(window,640,480);
        if(FAILED(hr)) {std::printf("Native device failed: 0x%08lX\n",static_cast<unsigned long>(hr));result=1;}
        if(!result){
            // A user can change options before the first evaluated scene.
            if(FAILED(neural.configure(NeuralMode::DLAA,640,480)) ||
               FAILED(neural.configure(NeuralMode::Off,640,480))) result=1;
        }
        const UINT dimensions[][2]={{640,480},{3440,1440},{3840,2160}};
        uint32_t frameIndex=0;
        for(const auto& size:dimensions) {
            if(result) break;
            hr=renderer.resize(size[0],size[1]);
            if(FAILED(hr)) {result=1;break;}
            for(auto mode:{NeuralMode::DLAA,NeuralMode::DLSSQuality}) {
                hr=neural.configure(mode,size[0],size[1]);
                if(FAILED(hr)) {
                    std::printf("DLSS configure failed: HRESULT 0x%08lX, SL %d\n",
                        static_cast<unsigned long>(hr),static_cast<int>(neural.lastResult()));result=1;break;
                }
                if(mode==NeuralMode::DLSSQuality &&
                   (neural.renderWidth()>=size[0] || neural.renderHeight()>=size[1])) {result=1;break;}
                if(neural.evaluate({})!=E_INVALIDARG) {result=1;break;}
                // Isolate resource/evaluation/readback from the future W3D scene adapter.
                const auto slMode=mode==NeuralMode::DLAA ? sl::DLSSMode::eDLAA : sl::DLSSMode::eMaxQuality;
                auto evaluate=[&](const generals_mods::DlaaFrame& frame) {
                    const HRESULT evaluated=neural.evaluate(frame);
                    return evaluated==E_INVALIDARG ? sl::Result::eErrorInvalidParameter : neural.lastResult();
                };
                if(!testDlaaFrame(renderer.device(),neural.module(),size[0],size[1],frameIndex++,slMode,
                                  evaluate,neural.renderWidth(),neural.renderHeight())) {
                    std::puts("Native DLSS GPU output check failed");result=1;break;
                }
                const float clear[4]={0,0,0,1};
                if(FAILED(renderer.beginFrame(clear))) {result=1;break;}
                neural.beforePresent();
                hr=renderer.endFrame();
                neural.afterPresent();
                if(FAILED(hr)) {result=1;break;}
                std::printf("Native D3D12 %s %ux%u -> %ux%u, GPU output verified\n",
                    mode==NeuralMode::DLAA ? "DLAA" : "DLSS Quality",neural.renderWidth(),neural.renderHeight(),size[0],size[1]);
            }
            if(FAILED(neural.configure(NeuralMode::Off,size[0],size[1]))) result=1;
            if(neural.evaluate({})!=E_UNEXPECTED) result=1;
        }
        if(FAILED(neural.shutdown())) result=1;
    }
    DestroyWindow(window);UnregisterClassW(wc.lpszClassName,instance);
    if(!result) std::puts("PASS: AMD64 native D3D12 DLSS/DLAA isolated GPU tests; no game scene tested");
    return result;
}
