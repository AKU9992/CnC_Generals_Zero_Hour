#include "d3d8to9.hpp"
#include "NeuralRenderer9.h"
using namespace generals_mods;
extern "C" __declspec(dllexport) void WINAPI GeneralsNeuralFinalize() {
    neuralRenderers().clear(); vertexShadows().clear();
    neuralLog("All neural backends finalized");
}
extern "C" __declspec(dllexport) void WINAPI GeneralsNeuralShutdown(IDirect3DDevice8* device) {
    neuralLog("Explicit neural shutdown started");
    if(device) {
        char message[100]; std::snprintf(message,sizeof(message),"Neural backends at shutdown: %zu",neuralRenderers().size()); neuralLog(message);
        releaseNeuralRenderer(static_cast<Direct3DDevice8*>(device)->GetProxyInterface());
    }
    neuralLog("Explicit neural shutdown completed");
}
void neuralBeforePresent(IDirect3DDevice9* device) {neuralRenderer(device).beforePresent();}
void neuralAfterPresent(IDirect3DDevice9* device) {neuralRenderer(device).afterPresent();}
static NeuralRenderer9* renderer(IDirect3DDevice8* device) {
    return device ? &neuralRenderer(static_cast<Direct3DDevice8*>(device)->GetProxyInterface()):nullptr;
}
extern "C" __declspec(dllexport) BOOL WINAPI GeneralsNeuralAvailable(IDirect3DDevice8* device) {
    auto* backend=renderer(device);return backend && backend->available();
}
extern "C" __declspec(dllexport) BOOL WINAPI GeneralsNeuralSetMode(IDirect3DDevice8* device,UINT mode) {
    auto* backend=renderer(device);return backend && backend->setMode(mode);
}
extern "C" __declspec(dllexport) UINT WINAPI GeneralsNeuralGetMode(IDirect3DDevice8* device) {
    auto* backend=renderer(device);return backend ? backend->mode():0;
}
extern "C" __declspec(dllexport) HRESULT WINAPI GeneralsNeuralBegin(IDirect3DDevice8* device,uint64_t scene) {
    auto* backend=renderer(device);return backend ? backend->begin(scene):E_INVALIDARG;
}
extern "C" __declspec(dllexport) HRESULT WINAPI GeneralsNeuralEnd(IDirect3DDevice8* device) {
    auto* backend=renderer(device);return backend ? backend->end():E_INVALIDARG;
}
extern "C" __declspec(dllexport) void WINAPI GeneralsNeuralObject(IDirect3DDevice8* device,uint64_t object) {
    auto* backend=renderer(device);if(backend) backend->object(object);
}
extern "C" __declspec(dllexport) void WINAPI GeneralsNeuralAbort(IDirect3DDevice8* device) {
    auto* backend=renderer(device);if(backend) backend->abort();
}
extern "C" __declspec(dllexport) void WINAPI GeneralsNeuralResetHistory(IDirect3DDevice8* device) {
    auto* backend=renderer(device);if(backend) backend->resetHistory();
}
