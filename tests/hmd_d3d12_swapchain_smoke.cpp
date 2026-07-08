#include "hmd_test_common.hpp"

#include <Varjo_d3d12.h>

#include <d3d12.h>

namespace {

D3D12_RESOURCE_BARRIER transitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

class D3D12SmokeContext {
public:
    ~D3D12SmokeContext()
    {
        if (queue_ && fence_) {
            try {
                waitForGpu();
            } catch (...) {
            }
        }
        if (fence_event_) {
            CloseHandle(fence_event_);
            fence_event_ = nullptr;
        }
    }

    void initialize(varjo_Session* session)
    {
        auto adapter = vtk_hmd_test::findVarjoAdapter(session);

        vtk_hmd_test::throwIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)), "D3D12CreateDevice");

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;
        vtk_hmd_test::throwIfFailed(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue_)), "CreateCommandQueue");
        vtk_hmd_test::throwIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)), "CreateCommandAllocator");
        vtk_hmd_test::throwIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&command_list_)), "CreateCommandList");
        vtk_hmd_test::throwIfFailed(command_list_->Close(), "Close initial command list");
        vtk_hmd_test::throwIfFailed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence");

        fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        VTK_HMD_TEST_REQUIRE(fence_event_ != nullptr);
    }

    ID3D12CommandQueue* commandQueue() const { return queue_.Get(); }

    void clearSwapChainImage(const VarjoSwapChain& swapChain, int32_t imageIndex)
    {
        VTK_HMD_TEST_REQUIRE(device_ != nullptr);
        VTK_HMD_TEST_REQUIRE(queue_ != nullptr);
        VTK_HMD_TEST_REQUIRE(allocator_ != nullptr);
        VTK_HMD_TEST_REQUIRE(command_list_ != nullptr);
        VTK_HMD_TEST_REQUIRE(swapChain.valid());
        VTK_HMD_TEST_REQUIRE(imageIndex >= 0);
        VTK_HMD_TEST_REQUIRE(imageIndex < swapChain.config().numberOfTextures);

        const varjo_Texture texture = swapChain.image(imageIndex);
        ID3D12Resource* resource = varjo_ToD3D12Texture(texture);
        VTK_HMD_TEST_REQUIRE(resource != nullptr);

        const auto resourceDesc = resource->GetDesc();
        const auto& config = swapChain.config();
        std::cout
            << "D3D12 imageIndex=" << imageIndex
            << " texture=" << resourceDesc.Width << 'x' << resourceDesc.Height
            << " arraySize=" << resourceDesc.DepthOrArraySize
            << " format=" << resourceDesc.Format
            << '\n';

        VTK_HMD_TEST_REQUIRE(resourceDesc.Width == static_cast<UINT64>(config.textureWidth));
        VTK_HMD_TEST_REQUIRE(resourceDesc.Height == static_cast<UINT>(config.textureHeight));
        VTK_HMD_TEST_REQUIRE(resourceDesc.DepthOrArraySize >= static_cast<UINT16>(config.textureArraySize));

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.NumDescriptors = static_cast<UINT>(config.textureArraySize);
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDesc.NodeMask = 0;

        vtk_hmd_test::ComPtr<ID3D12DescriptorHeap> rtvHeap;
        vtk_hmd_test::throwIfFailed(device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap)), "CreateDescriptorHeap");

        const UINT rtvDescriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        const D3D12_CPU_DESCRIPTOR_HANDLE heapStart = rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (int32_t arrayIndex = 0; arrayIndex < config.textureArraySize; ++arrayIndex) {
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
            rtvDesc.Format = vtk_hmd_test::rtvFormatFor(resourceDesc.Format);
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = 0;
            rtvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(arrayIndex);
            rtvDesc.Texture2DArray.ArraySize = 1;
            rtvDesc.Texture2DArray.PlaneSlice = 0;
            device_->CreateRenderTargetView(resource, &rtvDesc, handleFor(heapStart, rtvDescriptorSize, arrayIndex));
        }

        vtk_hmd_test::throwIfFailed(allocator_->Reset(), "CommandAllocator Reset");
        vtk_hmd_test::throwIfFailed(command_list_->Reset(allocator_.Get(), nullptr), "CommandList Reset");

        auto toRenderTarget = transitionBarrier(resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list_->ResourceBarrier(1, &toRenderTarget);

        for (int32_t arrayIndex = 0; arrayIndex < config.textureArraySize; ++arrayIndex) {
            const float clearColor[4] = {
                0.04f + 0.04f * static_cast<float>(arrayIndex),
                0.03f,
                0.12f,
                1.0f,
            };
            command_list_->ClearRenderTargetView(handleFor(heapStart, rtvDescriptorSize, arrayIndex), clearColor, 0, nullptr);
        }

        auto toCommon = transitionBarrier(resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
        command_list_->ResourceBarrier(1, &toCommon);

        vtk_hmd_test::throwIfFailed(command_list_->Close(), "CommandList Close");
        ID3D12CommandList* commandLists[] = {command_list_.Get()};
        queue_->ExecuteCommandLists(1, commandLists);
        waitForGpu();
    }

private:
    static D3D12_CPU_DESCRIPTOR_HANDLE handleFor(D3D12_CPU_DESCRIPTOR_HANDLE start, UINT descriptorSize, int32_t index)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = start;
        handle.ptr += static_cast<SIZE_T>(index) * static_cast<SIZE_T>(descriptorSize);
        return handle;
    }

    void waitForGpu()
    {
        const uint64_t waitValue = fence_value_++;
        vtk_hmd_test::throwIfFailed(queue_->Signal(fence_.Get(), waitValue), "Queue Signal");
        if (fence_->GetCompletedValue() < waitValue) {
            vtk_hmd_test::throwIfFailed(fence_->SetEventOnCompletion(waitValue, fence_event_), "SetEventOnCompletion");
            const DWORD waitResult = WaitForSingleObject(fence_event_, INFINITE);
            VTK_HMD_TEST_REQUIRE(waitResult == WAIT_OBJECT_0);
        }
    }

private:
    vtk_hmd_test::ComPtr<ID3D12Device> device_;
    vtk_hmd_test::ComPtr<ID3D12CommandQueue> queue_;
    vtk_hmd_test::ComPtr<ID3D12CommandAllocator> allocator_;
    vtk_hmd_test::ComPtr<ID3D12GraphicsCommandList> command_list_;
    vtk_hmd_test::ComPtr<ID3D12Fence> fence_;
    uint64_t fence_value_ = 1;
    HANDLE fence_event_ = nullptr;
};

void runD3D12SwapChainSmokeTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    VarjoFrameInfo frameInfo(session);
    VTK_HMD_TEST_REQUIRE(frameInfo.valid());
    VTK_HMD_TEST_REQUIRE(frameInfo.waitSync());
    VTK_HMD_TEST_REQUIRE(frameInfo.viewCount() > 0);

    D3D12SmokeContext context;
    context.initialize(session.get());
    VTK_HMD_TEST_REQUIRE(context.commandQueue() != nullptr);

    auto config = vtk_hmd_test::makeSmokeColorSwapChainConfig(session.get(), frameInfo);
    VarjoSwapChain swapChain = VarjoSwapChain::createD3D12(session.shared(), context.commandQueue(), config);
    VTK_HMD_TEST_REQUIRE_MESSAGE(swapChain.valid(), swapChain.lastError());
    VTK_HMD_TEST_REQUIRE(swapChain.session() == session.get());
    VTK_HMD_TEST_REQUIRE(swapChain.get() != nullptr);
    VTK_HMD_TEST_REQUIRE(swapChain.config().textureWidth == config.textureWidth);
    VTK_HMD_TEST_REQUIRE(swapChain.config().textureHeight == config.textureHeight);
    VTK_HMD_TEST_REQUIRE(swapChain.config().textureArraySize == config.textureArraySize);

    VarjoLayerFrame layerFrame(session.shared());
    VarjoMultiProjLayer layer(frameInfo.viewCount(), varjo_LayerFlagNone, varjo_SpaceLocal);

    VTK_HMD_TEST_REQUIRE(frameInfo.waitSync());
    VTK_HMD_TEST_REQUIRE(layerFrame.begin());

    int32_t imageIndex = -1;
    VTK_HMD_TEST_REQUIRE_MESSAGE(swapChain.acquire(imageIndex), swapChain.lastError());
    VTK_HMD_TEST_REQUIRE(swapChain.acquired());
    VTK_HMD_TEST_REQUIRE(swapChain.acquiredIndex() == imageIndex);
    VTK_HMD_TEST_REQUIRE(imageIndex >= 0);
    VTK_HMD_TEST_REQUIRE(imageIndex < swapChain.config().numberOfTextures);

    context.clearSwapChainImage(swapChain, imageIndex);
    vtk_hmd_test::updateLayerViews(layer, frameInfo, swapChain);

    swapChain.release();
    VTK_HMD_TEST_REQUIRE(!swapChain.acquired());
    VTK_HMD_TEST_REQUIRE(swapChain.acquiredIndex() == -1);

    VTK_HMD_TEST_REQUIRE(layerFrame.end(layer, frameInfo.frameNumber()));
}

} // namespace

int main()
{
    return vtk_hmd_test::runTest("VarjoToolkitHmdD3D12SwapChainSmokeTest", runD3D12SwapChainSmokeTest);
}
