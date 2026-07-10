#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>
#include <VarjoToolkit/Rendering/VarjoSwapChain.hpp>

#include <Varjo_d3d12.h>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

namespace {

std::atomic_bool g_stopRequested{false};

BOOL WINAPI consoleCtrlHandler(DWORD eventType)
{
    switch (eventType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        g_stopRequested.store(true);
        return TRUE;
    default:
        return FALSE;
    }
}

void throwIfFailed(HRESULT result, const char* operation)
{
    if (FAILED(result)) {
        throw std::runtime_error(std::string(operation) + " failed");
    }
}

bool sameLuid(const DXGI_ADAPTER_DESC1& description, const varjo_Luid& luid)
{
    return description.AdapterLuid.HighPart == luid.high &&
        static_cast<uint32_t>(description.AdapterLuid.LowPart) == luid.low;
}

ComPtr<IDXGIAdapter1> findVarjoAdapter(varjo_Session* session)
{
    const varjo_Luid luid = varjo_D3D11GetLuid(session);
    ComPtr<IDXGIFactory1> factory;
    throwIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");

    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 description{};
        if (SUCCEEDED(adapter->GetDesc1(&description)) &&
            sameLuid(description, luid)) {
            return adapter;
        }
    }
    throw std::runtime_error("Varjo DXGI adapter was not found");
}

varjo_Matrix copyMatrix(const double values[16])
{
    varjo_Matrix output{};
    std::copy(values, values + 16, output.value);
    return output;
}

DXGI_FORMAT rtvFormatFor(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    default:
        return format;
    }
}

int recommendedWidth(const VarjoFrameInfo& frameInfo)
{
    int output = 1;
    for (int32_t index = 0; index < frameInfo.viewCount(); ++index) {
        output = std::max(output, frameInfo.view(index).preferredWidth);
    }
    return output;
}

int recommendedHeight(const VarjoFrameInfo& frameInfo)
{
    int output = 1;
    for (int32_t index = 0; index < frameInfo.viewCount(); ++index) {
        output = std::max(output, frameInfo.view(index).preferredHeight);
    }
    return output;
}

void updateLayerViews(
    VarjoMultiProjLayer& layer,
    const VarjoFrameInfo& frameInfo,
    const VarjoSwapChain& swapChain)
{
    for (int32_t index = 0; index < frameInfo.viewCount(); ++index) {
        const auto& view = frameInfo.view(index);
        layer.setView(
            static_cast<size_t>(index),
            copyMatrix(view.projectionMatrix),
            copyMatrix(view.viewMatrix),
            swapChain.fullViewport(index));
    }
}

D3D12_RESOURCE_BARRIER transition(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

} // namespace

int main(int argc, char** argv)
{
    int frameLimit = 300;
    if (argc == 3 && std::string(argv[1]) == "--frames") {
        frameLimit = std::max(0, std::stoi(argv[2]));
    }

    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    try {
        VarjoSession session;
        if (!session.valid()) {
            std::cerr << session.lastError() << '\n';
            return 1;
        }

        VarjoFrameInfo frameInfo(session);
        if (!frameInfo.valid()) return 1;
        varjo_WaitSync(session.get(), frameInfo.get());

        const int32_t viewCount = frameInfo.viewCount();
        const int width = recommendedWidth(frameInfo);
        const int height = recommendedHeight(frameInfo);

        auto adapter = findVarjoAdapter(session.get());
        ComPtr<ID3D12Device> device;
        throwIfFailed(
            D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device)),
            "D3D12CreateDevice");

        D3D12_COMMAND_QUEUE_DESC queueDescription{};
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ComPtr<ID3D12CommandQueue> queue;
        throwIfFailed(
            device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue)),
            "CreateCommandQueue");

        auto config = VarjoSwapChain::makeConfig(
            varjo_TextureFormat_R8G8B8A8_SRGB,
            width,
            height,
            3,
            viewCount);
        auto swapChain = VarjoSwapChain::createD3D12(
            session.shared(),
            queue.Get(),
            config);
        if (!swapChain.valid()) {
            std::cerr << swapChain.lastError() << '\n';
            return 1;
        }

        D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
        heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDescription.NumDescriptors =
            static_cast<UINT>(config.numberOfTextures * config.textureArraySize);
        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        throwIfFailed(
            device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&rtvHeap)),
            "CreateDescriptorHeap");
        const UINT descriptorSize = device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        const auto heapStart = rtvHeap->GetCPUDescriptorHandleForHeapStart();

        auto handleFor = [&](int32_t imageIndex, int32_t viewIndex) {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            const size_t offset = static_cast<size_t>(imageIndex) * viewCount +
                static_cast<size_t>(viewIndex);
            handle.ptr += offset * descriptorSize;
            return handle;
        };

        for (int32_t imageIndex = 0;
             imageIndex < config.numberOfTextures;
             ++imageIndex) {
            ID3D12Resource* texture = varjo_ToD3D12Texture(
                swapChain.image(imageIndex));
            const auto description = texture->GetDesc();
            for (int32_t viewIndex = 0;
                 viewIndex < viewCount;
                 ++viewIndex) {
                D3D12_RENDER_TARGET_VIEW_DESC rtvDescription{};
                rtvDescription.Format = rtvFormatFor(description.Format);
                rtvDescription.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDescription.Texture2DArray.MipSlice = 0;
                rtvDescription.Texture2DArray.FirstArraySlice = viewIndex;
                rtvDescription.Texture2DArray.ArraySize = 1;
                device->CreateRenderTargetView(
                    texture,
                    &rtvDescription,
                    handleFor(imageIndex, viewIndex));
            }
        }

        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> commandList;
        throwIfFailed(
            device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&allocator)),
            "CreateCommandAllocator");
        throwIfFailed(
            device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                allocator.Get(),
                nullptr,
                IID_PPV_ARGS(&commandList)),
            "CreateCommandList");
        throwIfFailed(commandList->Close(), "Close command list");

        ComPtr<ID3D12Fence> fence;
        throwIfFailed(
            device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)),
            "CreateFence");
        HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        uint64_t fenceValue = 1;

        auto waitForGpu = [&]() {
            const uint64_t value = fenceValue++;
            throwIfFailed(queue->Signal(fence.Get(), value), "Queue Signal");
            if (fence->GetCompletedValue() < value) {
                throwIfFailed(
                    fence->SetEventOnCompletion(value, fenceEvent),
                    "SetEventOnCompletion");
                WaitForSingleObject(fenceEvent, INFINITE);
            }
        };

        VarjoLayerFrame layerFrame(session.shared());
        VarjoMultiProjLayer layer(
            viewCount,
            varjo_LayerFlagNone,
            varjo_SpaceLocal);

        int renderedFrames = 0;
        while (!g_stopRequested.load() &&
               (frameLimit == 0 || renderedFrames < frameLimit)) {
            // The sample application is the sole synchronization owner.
            varjo_WaitSync(session.get(), frameInfo.get());
            if (!layerFrame.begin()) break;

            int32_t imageIndex = -1;
            if (!swapChain.acquire(imageIndex)) {
                layerFrame.endEmpty(frameInfo.frameNumber());
                break;
            }

            ID3D12Resource* texture = varjo_ToD3D12Texture(
                swapChain.image(imageIndex));
            throwIfFailed(allocator->Reset(), "Allocator Reset");
            throwIfFailed(
                commandList->Reset(allocator.Get(), nullptr),
                "CommandList Reset");

            auto toRenderTarget = transition(
                texture,
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            commandList->ResourceBarrier(1, &toRenderTarget);
            for (int32_t viewIndex = 0;
                 viewIndex < viewCount;
                 ++viewIndex) {
                const float clear[4] = {
                    0.04f + 0.03f * static_cast<float>(viewIndex),
                    0.08f,
                    0.16f,
                    1.0f};
                commandList->ClearRenderTargetView(
                    handleFor(imageIndex, viewIndex),
                    clear,
                    0,
                    nullptr);
            }
            auto toCommon = transition(
                texture,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_COMMON);
            commandList->ResourceBarrier(1, &toCommon);
            throwIfFailed(commandList->Close(), "CommandList Close");
            ID3D12CommandList* lists[] = {commandList.Get()};
            queue->ExecuteCommandLists(1, lists);
            waitForGpu();

            updateLayerViews(layer, frameInfo, swapChain);
            swapChain.release();
            if (!layerFrame.end(layer, frameInfo.frameNumber())) break;
            ++renderedFrames;
        }

        waitForGpu();
        CloseHandle(fenceEvent);
        std::cout << "Rendered frames: " << renderedFrames << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
