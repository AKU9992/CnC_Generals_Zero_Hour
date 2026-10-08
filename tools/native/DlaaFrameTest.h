#pragma once
#include "../../renderer/DlaaPass.h"
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <cmath>
#include <cstring>
#include <wrl/client.h>
#include <cstdio>
#include <functional>

// Static synthetic frame, not a substitute for game depth/motion data.
inline bool testDlaaFrame(ID3D12Device* device, HMODULE streamLine, UINT width = 3440, UINT height = 1440, uint32_t frameIndex = 0, sl::DLSSMode mode = sl::DLSSMode::eDLAA,
    std::function<sl::Result(const generals_mods::DlaaFrame&)> externalEvaluation = {}, UINT suppliedWidth = 0, UINT suppliedHeight = 0)
{
    using Microsoft::WRL::ComPtr;
    generals_mods::DlaaPass pass(streamLine);
    if (!externalEvaluation && pass.initialize(width, height, mode) != sl::Result::eOk) return false;
    const UINT renderWidth = externalEvaluation ? suppliedWidth : pass.renderWidth();
    const UINT renderHeight = externalEvaluation ? suppliedHeight : pass.renderHeight();
    if (!renderWidth || !renderHeight) return false;
    auto evaluate = [&](const generals_mods::DlaaFrame& frame) {
        return externalEvaluation ? externalEvaluation(frame) : pass.evaluate(frame);
    };
    if (mode == sl::DLSSMode::eMaxQuality && (renderWidth >= width || renderHeight >= height)) return false;
    if (evaluate({}) != sl::Result::eErrorInvalidParameter) return false;
    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)))) return false;
    ComPtr<ID3D12CommandAllocator> allocator;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) return false;
    ComPtr<ID3D12GraphicsCommandList> commands;
    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commands)))) return false;
    ComPtr<ID3D12Resource> textures[4];
    const DXGI_FORMAT formats[] = {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT};
    D3D12_HEAP_PROPERTIES gpuHeap{}; gpuHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    for (UINT i = 0; i < 4; ++i) {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = i == 3 ? width : renderWidth; desc.Height = i == 3 ? height : renderHeight;
        desc.DepthOrArraySize = 1; desc.MipLevels = 1;
        desc.Format = formats[i]; desc.SampleDesc.Count = 1;
        desc.Flags = i == 3 ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_RESOURCE_STATES state = i == 3 ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (FAILED(device->CreateCommittedResource(&gpuHeap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&textures[i])))) return false;
    }
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{}; heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heapDesc.NumDescriptors = 3;
    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap)))) return false;
    const float clearColors[3][4] = {{0.25f, 0.5f, 0.75f, 1.0f}, {0.5f, 0, 0, 0}, {0, 0, 0, 0}};
    auto handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < 3; ++i) {
        device->CreateRenderTargetView(textures[i].Get(), nullptr, handle);
        commands->ClearRenderTargetView(handle, clearColors[i], 0, nullptr);
        handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = textures[i].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commands->ResourceBarrier(1, &barrier);
    }
    generals_mods::DlaaFrame frame{};
    frame.frameIndex = frameIndex;
    frame.sceneColor = textures[0].Get(); frame.depth = textures[1].Get(); frame.motionVectors = textures[2].Get(); frame.outputColor = textures[3].Get();
    frame.commands = commands.Get();
    frame.sceneColorState = frame.depthState = frame.motionVectorsState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    frame.outputColorState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    auto& camera = frame.constants;
    DirectX::XMFLOAT4X4 projection, inverse;
    auto matrix = DirectX::XMMatrixPerspectiveFovLH(1.0f, float(width) / height, 0.1f, 1000.0f);
    DirectX::XMStoreFloat4x4(&projection, matrix);
    DirectX::XMStoreFloat4x4(&inverse, DirectX::XMMatrixInverse(nullptr, matrix));
    std::memcpy(&camera.cameraViewToClip, &projection, sizeof(projection));
    std::memcpy(&camera.clipToCameraView, &inverse, sizeof(inverse));
    DirectX::XMFLOAT4X4 identity;
    DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
    std::memcpy(&camera.clipToPrevClip, &identity, sizeof(identity));
    std::memcpy(&camera.prevClipToClip, &identity, sizeof(identity));
    camera.cameraPos = {0,0,0}; camera.cameraUp = {0,1,0}; camera.cameraRight = {1,0,0}; camera.cameraFwd = {0,0,1};
    camera.cameraNear = 0.1f; camera.cameraFar = 1000; camera.cameraFOV = 1; camera.cameraAspectRatio = float(width) / height;
    camera.mvecScale = {1,1}; camera.jitterOffset = {0,0};
    camera.cameraPinholeOffset = {0,0};
    camera.depthInverted = sl::Boolean::eFalse; camera.cameraMotionIncluded = sl::Boolean::eTrue;
    camera.motionVectors3D = sl::Boolean::eFalse; camera.reset = sl::Boolean::eTrue;
    auto invalid = frame;
    invalid.outputColor = invalid.sceneColor;
    if (evaluate(invalid) != sl::Result::eErrorInvalidParameter) return false;
    invalid = frame; invalid.depth = nullptr;
    if (evaluate(invalid) != sl::Result::eErrorInvalidParameter) return false;
    const sl::Result evaluation = evaluate(frame);
    std::printf("Synthetic %s frame %ux%u -> %ux%u evaluation: %d\n", mode == sl::DLSSMode::eDLAA ? "DLAA" : "DLSS Quality", renderWidth, renderHeight, width, height, static_cast<int>(evaluation));
    if (evaluation != sl::Result::eOk) { if (!externalEvaluation) pass.release(); return false; }
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = textures[3].Get(); barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE; barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commands->ResourceBarrier(1, &barrier);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT64 readbackBytes = 0;
    auto outputDesc = textures[3]->GetDesc();
    device->GetCopyableFootprints(&outputDesc, 0, 1, 0, &footprint, nullptr, nullptr, &readbackBytes);
    D3D12_HEAP_PROPERTIES cpuHeap{}; cpuHeap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bufferDesc{}; bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = readbackBytes; bufferDesc.Height = 1; bufferDesc.DepthOrArraySize = 1; bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1; bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> readback;
    if (FAILED(device->CreateCommittedResource(&cpuHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)))) return false;
    D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource = textures[3].Get(); source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION destination{}; destination.pResource = readback.Get(); destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; destination.PlacedFootprint = footprint;
    commands->CopyTextureRegion(&destination, 0,0,0, &source, nullptr);
    if (FAILED(commands->Close())) return false;
    ID3D12CommandList* lists[] = {commands.Get()}; queue->ExecuteCommandLists(1, lists);
    ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) || FAILED(queue->Signal(fence.Get(), 1))) return false;
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) return false;
    if (FAILED(fence->SetEventOnCompletion(1, event))) { CloseHandle(event); return false; }
    const DWORD wait = WaitForSingleObject(event, 30000); CloseHandle(event);
    if (wait != WAIT_OBJECT_0) return false;
    void* bytes = nullptr;
    D3D12_RANGE range{0, static_cast<SIZE_T>(readbackBytes)};
    if (FAILED(readback->Map(0, &range, &bytes))) return false;
    const SIZE_T offset = static_cast<SIZE_T>(footprint.Offset) + (height / 2) * footprint.Footprint.RowPitch + (width / 2) * 8;
    const auto* pixel = reinterpret_cast<const DirectX::PackedVector::HALF*>(static_cast<const unsigned char*>(bytes) + offset);
    const float red = DirectX::PackedVector::XMConvertHalfToFloat(pixel[0]);
    const float green = DirectX::PackedVector::XMConvertHalfToFloat(pixel[1]);
    const float blue = DirectX::PackedVector::XMConvertHalfToFloat(pixel[2]);
    D3D12_RANGE noWrites{0,0}; readback->Unmap(0, &noWrites);
    std::printf("DLAA output center RGB: %.4f %.4f %.4f\n", red, green, blue);
    const sl::Result released = externalEvaluation ? sl::Result::eOk : pass.release();
    return released == sl::Result::eOk && std::isfinite(red) && std::isfinite(green) && std::isfinite(blue) &&
        std::abs(red - 0.25f) < 0.15f && std::abs(green - 0.5f) < 0.15f && std::abs(blue - 0.75f) < 0.15f;
}
