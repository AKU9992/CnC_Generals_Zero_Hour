#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <cstdint>

namespace generals_mods::native12 {
static_assert(sizeof(void*) == 8, "The native renderer is AMD64 only");

struct Vertex {
    float position[3];
    float color[4];
};

// Owns native resources. No D3D8/9 objects or interop interfaces are accepted.
// All methods are called from the render thread. HWND belongs to the caller.
class Device {
    template<class T> using Ptr = Microsoft::WRL::ComPtr<T>;
public:
    static constexpr UINT frameCount = 3;
    ~Device();
    Device() = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    HRESULT initialize(HWND window, UINT width, UINT height, bool debug = false);
    HRESULT resize(UINT width, UINT height);
    HRESULT beginFrame(const float clear[4]);
    // Restore the native target/viewport after scene or DLSS commands.
    HRESULT bindTarget();
    HRESULT drawTriangles(const Vertex* vertices, UINT count, const float transform[16]);
    // Captures the actual target before Present, then waits for the readback.
    // Output pixels use RGBA8 order with no row padding.
    HRESULT endFrame(UINT syncInterval = 0, std::vector<uint8_t>* pixels = nullptr);
    HRESULT waitIdle();
    void shutdown();

    ID3D12Device* device() const { return device_.Get(); }
    ID3D12CommandQueue* queue() const { return queue_.Get(); }
    IDXGISwapChain3* swapchain() const { return swap_.Get(); }
    ID3D12GraphicsCommandList* commands() const { return recording_ ? commands_.Get() : nullptr; }
    UINT width() const { return width_; }
    UINT height() const { return height_; }
    const DXGI_ADAPTER_DESC1& adapterDescription() const { return adapterDescription_; }
    HRESULT checkDebugErrors() const;
private:
    struct Frame {
        Ptr<ID3D12CommandAllocator> allocator;
        Ptr<ID3D12Resource> target;
        Ptr<ID3D12Resource> upload;
        uint8_t* mapped = nullptr;
        UINT64 fenceValue = 0;
        UINT uploadOffset = 0;
    };
    static constexpr UINT uploadBytes = 4 * 1024 * 1024;
    HRESULT createTargets();
    HRESULT createPipeline();
    HRESULT wait(UINT64 value);
    HRESULT signal(UINT64& value);
    void barrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    D3D12_CPU_DESCRIPTOR_HANDLE targetHandle() const;
    Ptr<IDXGIFactory6> factory_;
    Ptr<ID3D12Device> device_;
    Ptr<ID3D12CommandQueue> queue_;
    Ptr<IDXGISwapChain3> swap_;
    Ptr<ID3D12DescriptorHeap> targets_;
    Ptr<ID3D12GraphicsCommandList> commands_;
    Ptr<ID3D12RootSignature> root_;
    Ptr<ID3D12PipelineState> pipeline_;
    Ptr<ID3D12Fence> fence_;
    std::array<Frame, frameCount> frames_;
    DXGI_ADAPTER_DESC1 adapterDescription_{};
    HANDLE event_ = nullptr;
    UINT width_ = 0, height_ = 0, index_ = 0, targetStride_ = 0;
    UINT64 nextFence_ = 1;
    bool recording_ = false;
};
}
