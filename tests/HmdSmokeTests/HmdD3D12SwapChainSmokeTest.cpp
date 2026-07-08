#include "HmdD3DTestCommon.hpp"

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>
#include <VarjoToolkit/Rendering/VarjoSwapChain.hpp>

#include <Varjo_d3d12.h>

#include <iostream>

int main()
{
    std::cout << "VarjoToolkit HMD D3D12 swapchain smoke test\n";
    if (!requireRuntimeAvailable()) {
        return 1;
    }

    try {
        VarjoSession session;
        if (!requireSession(session)) {
            return 1;
        }

        VarjoFrameInfo frameInfo(session);
        if (!frameInfo.valid() || !frameInfo.waitSync()) {
            return hmdFail("VarjoFrameInfo waitSync failed");
        }
        const int32_t viewCount = frameInfo.viewCount();
        if (viewCount <= 0) {
            return hmdFail("VarjoFrameInfo reported no views");
        }

        HmdD3D12Context d3d12 = hmdCreateD3D12Context(session.get());
        if (!d3d12.device || !d3d12.queue) {
            return hmdFail("D3D12 device or command queue is null");
        }

        constexpr int32_t width = 512;
        constexpr int32_t height = 512;
        auto config = VarjoSwapChain::makeConfig(varjo_TextureFormat_R8G8B8A8_SRGB, width, height, 3, viewCount);
        VarjoSwapChain swapChain = VarjoSwapChain::createD3D12(session.shared(), d3d12.queue.Get(), config);
        if (!swapChain.valid()) {
            return hmdFail(std::string("failed to create D3D12 Varjo swapchain: ") + swapChain.lastError());
        }

        for (int32_t imageIndex = 0; imageIndex < config.numberOfTextures; ++imageIndex) {
            const varjo_Texture texture = swapChain.image(imageIndex);
            ID3D12Resource* resource = varjo_ToD3D12Texture(texture);
            if (!resource) {
                return hmdFail("varjo_ToD3D12Texture returned null");
            }
            const auto desc = resource->GetDesc();
            std::cout << "image=" << imageIndex
                      << " width=" << desc.Width
                      << " height=" << desc.Height
                      << " arraySize=" << desc.DepthOrArraySize
                      << " format=" << static_cast<int64_t>(desc.Format) << "\n";
        }

        int32_t acquiredIndex = -1;
        if (!swapChain.acquire(acquiredIndex)) {
            return hmdFail(std::string("D3D12 swapchain acquire failed: ") + swapChain.lastError());
        }
        if (acquiredIndex < 0 || acquiredIndex >= config.numberOfTextures) {
            swapChain.release();
            return hmdFail("D3D12 swapchain acquired index is out of range");
        }
        swapChain.release();

        VarjoLayerFrame layerFrame(session.shared());
        if (!frameInfo.waitSync()) {
            return hmdFail("VarjoFrameInfo second waitSync failed");
        }
        if (!layerFrame.begin()) {
            return hmdFail(std::string("VarjoLayerFrame begin failed: ") + layerFrame.lastError());
        }
        if (!layerFrame.endEmpty(frameInfo.frameNumber())) {
            return hmdFail(std::string("VarjoLayerFrame empty end failed: ") + layerFrame.lastError());
        }

        std::cout << "[PASS] HMD D3D12 swapchain smoke test passed\n";
        return 0;
    } catch (const std::exception& e) {
        return hmdFail(std::string("fatal exception: ") + e.what());
    }
}
