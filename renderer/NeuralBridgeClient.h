#pragma once
#include <windows.h>
#include <cstdint>
struct IDirect3DDevice8;
namespace generals_mods {
template<class F> inline F neuralEntry(const char* name) {
    HMODULE bridge=GetModuleHandleW(L"generals-native12.dll");
    if(!bridge) bridge=GetModuleHandleW(L"generals-d3d12.dll");
    return bridge ? reinterpret_cast<F>(GetProcAddress(bridge,name)):nullptr;
}
inline void neuralShutdown(IDirect3DDevice8* device) {
    auto entry=neuralEntry<void (WINAPI*)(IDirect3DDevice8*)>("GeneralsNeuralShutdown"); if(entry && device) entry(device);
}
inline bool neuralAvailable(IDirect3DDevice8* device) {
    auto entry=neuralEntry<BOOL (WINAPI*)(IDirect3DDevice8*)>("GeneralsNeuralAvailable"); return entry && device && entry(device);
}
inline bool neuralSetMode(IDirect3DDevice8* device,UINT mode) {
    auto entry=neuralEntry<BOOL (WINAPI*)(IDirect3DDevice8*,UINT)>("GeneralsNeuralSetMode"); return entry && device && entry(device,mode);
}
inline UINT neuralMode(IDirect3DDevice8* device) {
    auto entry=neuralEntry<UINT (WINAPI*)(IDirect3DDevice8*)>("GeneralsNeuralGetMode"); return entry && device ? entry(device):0;
}
inline bool neuralBegin(IDirect3DDevice8* device,uint64_t scene) {
    auto entry=neuralEntry<HRESULT (WINAPI*)(IDirect3DDevice8*,uint64_t)>("GeneralsNeuralBegin"); return entry && device && entry(device,scene)==S_OK;
}
inline void neuralEnd(IDirect3DDevice8* device) {
    auto entry=neuralEntry<HRESULT (WINAPI*)(IDirect3DDevice8*)>("GeneralsNeuralEnd"); if(entry && device) entry(device);
}
inline void neuralAbort(IDirect3DDevice8* device) {
    auto entry=neuralEntry<void (WINAPI*)(IDirect3DDevice8*)>("GeneralsNeuralAbort"); if(entry && device) entry(device);
}
inline void neuralObject(IDirect3DDevice8* device,uint64_t identity) {
    auto entry=neuralEntry<void (WINAPI*)(IDirect3DDevice8*,uint64_t)>("GeneralsNeuralObject"); if(entry && device) entry(device,identity);
}
inline void neuralResetHistory(IDirect3DDevice8* device) {
    auto entry=neuralEntry<void (WINAPI*)(IDirect3DDevice8*)>("GeneralsNeuralResetHistory");if(entry && device) entry(device);
}
}
