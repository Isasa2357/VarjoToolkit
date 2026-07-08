#include "HmdD3DTestCommon.hpp"

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>
#include <VarjoToolkit/Rendering/VarjoSwapChain.hpp>

#include <Varjo_d3d11.h>

#include <algorithm>
#include <iostream>
#include <vector>

namespace {

varjo_Matrix copyMatrix(const double* values)
{
    varjo_Matrix out{};
    std::copy(values, values + 16, out.value);
    return out;
}

DXGI_FORMAT rtvFormatFor(DXGI_FORMAT resourceFormat)
{
    switch (resourceFormat) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    default:
        return resourceFormat;
    }
}

size_t targetIndex(int32_t imageIndex, int32_t arrayIndex, int32_t arraySize)
{
    return static_cast<size_t>(imageIndex) * static_cast<size_t>(arraySize) + static_cast<size_t>(arrayIndex);
}

void updateLayerViews(VarjoMultiProjLayer& layer, const VarjoFrameInfo& frameInfo, const VarjoSwapChain& swapChain)
{
    for (int32_t viewIndex = 0; viewIndex < frameInfo.viewCount(); ++viewIndex) {
        const auto& viewInfo = frameInfo.view(viewIndex);
        layer.setView(
            static_cast<size_t>(viewIndex),
            copyMatrix(viewInfo.projectionMatrix),
            copyMatrix(viewInfo.viewMatrix),
            swapChain.fullViewport(viewIndex));
    }
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit HMD D3D11 swapchain smoke test\n";
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

        ComPtr<ID3D11Device> device = hmdCreateD3D11Device(session.get());
        ComPtr<ID3D11DeviceContext> context;
        device->GetImmediateContext(&context);
        if (!context) {
            return hmdFail("D3D11 immediate context is null");
        }

        constexpr int32_t width = 512;
        constexpr int32_t height = 512;
        auto config = VarjoSwapChain::makeConfig(varjo_TextureFormat_R8G8B8A8_SRGB, width, height, 3, viewCount);
        VarjoSwapChain swapChain = VarjoSwapChain::createD3D11(session.shared(), device.Get(), config);
        if (!swapChain.valid()) {
            return hmdFail(std::string("failed to create D3D11 Varjo swapchain: ") + swapChain.lastError());
        }

        std::vector<ComPtr<ID3D11RenderTargetView>> renderTargets(static_cast<size_t>(config.numberOfTextures * config.textureArraySize));
        for (int32_t imageIndex = 0; imageIndex < config.numberOfTextures; ++imageIndex) {
            const varjo_Texture texture = swapChain.image(imageIndex);
            ID3D11Texture2D* d3dTexture = varjo_ToD3D11Texture(texture);
            if (!d3dTexture) {
                return hmdFail("varjo_ToD3D11Texture returned null");
            }

            D3D11_TEXTURE2D_DESC textureDesc{};
            d3dTexture->GetDesc(&textureDesc);

            for (int32_t arrayIndex = 0; arrayIndex < config.textureArraySize; ++arrayIndex) {
                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
                rtvDesc.Format = rtvFormatFor(textureDesc.Format);
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = 0;
                rtvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(arrayIndex);
                rtvDesc.Texture2DArray.ArraySize = 1;
                hmdThrowIfFailed(device->CreateRenderTargetView(d3dTexture, &rtvDesc, &renderTargets[targetIndex(imageIndex, arrayIndex, config.textureArraySize)]), "CreateRenderTargetView");
            }
        }

        VarjoLayerFrame layerFrame(session.shared());
        VarjoMultiProjLayer layer(viewCount, varjo_LayerFlagNone, varjo_SpaceLocal);

        if (!frameInfo.waitSync()) {
            return hmdFail("VarjoFrameInfo second waitSync failed");
        }
        if (!layerFrame.begin()) {
            return hmdFail(std::string("VarjoLayerFrame begin failed: ") + layerFrame.lastError());
        }

        int32_t imageIndex = -1;
        if (!swapChain.acquire(imageIndex)) {
            layerFrame.endEmpty(frameInfo.frameNumber());
            return hmdFail(std::string("D3D11 swapchain acquire failed: ") + swapChain.lastError());
        }

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context->RSSetViewports(1, &viewport);

        for (int32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
            auto* rtv = renderTargets[targetIndex(imageIndex, viewIndex, viewCount)].Get();
            if (!rtv) {
                swapChain.release();
                layerFrame.endEmpty(frameInfo.frameNumber());
                return hmdFail("D3D11 render target view is null");
            }
            const float clearColor[4] = {0.02f + 0.04f * static_cast<float>(viewIndex), 0.03f, 0.10f, 1.0f};
            context->OMSetRenderTargets(1, &rtv, nullptr);
            context->ClearRenderTargetView(rtv, clearColor);
        }
        ID3D11RenderTargetView* nullRtv = nullptr;
        context->OMSetRenderTargets(1, &nullRtv, nullptr);
        context->Flush();

        updateLayerViews(layer, frameInfo, swapChain);
        swapChain.release();

        if (!layerFrame.end(layer, frameInfo.frameNumber())) {
            return hmdFail(std::string("VarjoLayerFrame end failed: ") + layerFrame.lastError());
        }

        std::cout << "[PASS] HMD D3D11 swapchain smoke test passed\n";
        return 0;
    } catch (const std::exception& e) {
        return hmdFail(std::string("fatal exception: ") + e.what());
    }
}
