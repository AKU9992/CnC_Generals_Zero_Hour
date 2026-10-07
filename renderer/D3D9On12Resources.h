#pragma once
#include <d3d9.h>
#include <d3d12.h>
#include <d3d9on12.h>
#include <wrl/client.h>
#include <vector>

namespace generals_mods {
// A batch is checked out on one queue and returned together after submission.
// Never issue D3D9 work against its resources between acquire and submit.
class D3D9On12Resources {
public:
    HRESULT initialize(IDirect3DDevice9* device) {
        if (!device || interop_) return E_INVALIDARG;
        HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&interop_));
        if (SUCCEEDED(hr)) hr = interop_->GetD3D12Device(IID_PPV_ARGS(&device_));
        D3D12_COMMAND_QUEUE_DESC desc{};
        if (SUCCEEDED(hr)) hr = device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue_));
        if (SUCCEEDED(hr)) hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
        return hr;
    }
    ID3D12Device* device() const { return device_.Get(); }
    ID3D12CommandQueue* queue() const { return queue_.Get(); }
    HRESULT acquire(IDirect3DResource9* resource, ID3D12Resource** output) {
        if (!interop_ || !resource || !output) return E_INVALIDARG;
        *output = nullptr;
        for (const auto& held : held_) if (held.Get() == resource) return E_INVALIDARG;
        HRESULT hr = interop_->UnwrapUnderlyingResource(resource, queue_.Get(), IID_PPV_ARGS(output));
        if (SUCCEEDED(hr)) held_.emplace_back(resource);
        return hr;
    }
    // All resources must be in COMMON at the end of the supplied command list.
    HRESULT submit(ID3D12CommandList* commands) {
        if (!queue_ || held_.empty()) return E_INVALIDARG;
        if (commands) queue_->ExecuteCommandLists(1, &commands);
        const UINT64 value = ++value_;
        HRESULT hr = queue_->Signal(fence_.Get(), value);
        if (FAILED(hr)) return hr;
        HRESULT first = S_OK;
        for (auto& resource : held_) {
            ID3D12Fence* fence = fence_.Get();
            UINT64 signal = value;
            HRESULT result = interop_->ReturnUnderlyingResource(resource.Get(), 1, &signal, &fence);
            if (FAILED(result) && SUCCEEDED(first)) first = result;
        }
        held_.clear();
        return first;
    }
    HRESULT wait(DWORD milliseconds = 10000) {
        if (!fence_) return E_UNEXPECTED;
        if (fence_->GetCompletedValue() >= value_) return S_OK;
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event) return HRESULT_FROM_WIN32(GetLastError());
        HRESULT hr = fence_->SetEventOnCompletion(value_, event);
        if (SUCCEEDED(hr)) {
            DWORD result = WaitForSingleObject(event, milliseconds);
            if (result != WAIT_OBJECT_0) hr = result == WAIT_TIMEOUT ? HRESULT_FROM_WIN32(ERROR_TIMEOUT) : HRESULT_FROM_WIN32(GetLastError());
        }
        CloseHandle(event);
        return hr;
    }
    ~D3D9On12Resources() {
        // Abandoning a batch before recording commands still has to check it in.
        // Production owners explicitly drain before releasing GPU resources.
        if (!held_.empty()) submit(nullptr);
    }
    D3D9On12Resources() = default;
    D3D9On12Resources(const D3D9On12Resources&) = delete;
    D3D9On12Resources& operator=(const D3D9On12Resources&) = delete;
private:
    Microsoft::WRL::ComPtr<IDirect3DDevice9On12> interop_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    std::vector<Microsoft::WRL::ComPtr<IDirect3DResource9>> held_;
    UINT64 value_ = 0;
};
}
