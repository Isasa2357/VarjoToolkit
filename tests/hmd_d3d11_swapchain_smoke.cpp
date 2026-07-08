#include "hmd_test_common.hpp"

#include <Varjo_d3d11.h>

namespace {

void clearD3D11SwapChainImage(ID3D11Device* device, ID3D11DeviceContext* context, const VarjoSwapChain& swapChain, int32_t imageIndex)
{
    VTK_HMD_TEST_REQUIRE(device != nullptr);
    VTK_HMD_TEST_REQUIRE(context != nullptr);
    VTK_HMD_TEST_REQUIRE(swapChain.valid());
    VTK_HMD_TEST_REQUIRE(imageIndex >= 0);
    VTK_HMD_TEST_REQUIRE(imageIndex < swapChain.config().numberOfTextures);

    const varjo_Texture texture = swapChain.image(imageIndex);
    ID3D11Texture2D* d3dTexture = varjo_ToD3D11Texture(texture);
    VTK_HMD_TEST_REQUIRE(d3dTexture != nullptr);

    D3D11_TEXTURE2D_DESC textureDesc{};
    d3dTexture->GetDesc(&textureDesc);

    const auto& config = swapChain.config();
    std::cout
        << "D3D11 imageIndex=" << imageIndex
        << " texture=" << textureDesc.Width << 'x' << textureDesc.Height
        << " arraySize=" << textureDesc.ArraySize
        << " format=" << textureDesc.Format
        << '\n';

    VTK_HMD_TEST_REQUIRE(textureDesc.Width == static_cast<UINT>(config.textureWidth));
    VTK_HMD_TEST_REQUIRE(textureDesc.Height == static_cast<UINT>(config.textureHeight));
    VTK_HMD_TEST_REQUIRE(textureDesc.ArraySize >= static_cast<UINT>(config.textureArraySize));

    for (int32_t arrayIndex = 0; arrayIndex < config.textureArraySize; ++arrayIndex) {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = vtk_hmd_test::rtvFormatFor(textureDesc.Format);
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(arrayIndex);
        rtvDesc.Texture2DArray.ArraySize = 1;

        vtk_hmd_test::ComPtr<ID3D11RenderTargetView> rtv;
        vtk_hmd_test::throwIfFailed(device->CreateRenderTargetView(d3dTexture, &rtvDesc, &rtv), "CreateRenderTargetView");

        ID3D11RenderTargetView* rtvRaw = rtv.Get();
        const float clearColor[4] = {
            0.05f + 0.04f * static_cast<float>(arrayIndex),
            0.03f,
            0.10f,
            1.0f,
        };
        context->OMSetRenderTargets(1, &rtvRaw, nullptr);
        context->ClearRenderTargetView(rtv.Get(), clearColor);
    }

    ID3D11RenderTargetView* nullRtv = nullptr;
    context->OMSetRenderTargets(1, &nullRtv, nullptr);
    context->Flush();
}

void runD3D11SwapChainSmokeTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    VarjoFrameInfo frameInfo(session);
    VTK_HMD_TEST_REQUIRE(frameInfo.valid());
    VTK_HMD_TEST_REQUIRE(frameInfo.waitSync());
    VTK_HMD_TEST_REQUIRE(frameInfo.viewCount() > 0);

    vtk_hmd_test::ComPtr<ID3D11DeviceContext> context;
    auto device = vtk_hmd_test::createD3D11DeviceForVarjo(session.get(), context);
    VTK_HMD_TEST_REQUIRE(device != nullptr);
    VTK_HMD_TEST_REQUIRE(context != nullptr);

    auto config = vtk_hmd_test::makeSmokeColorSwapChainConfig(session.get(), frameInfo);
    VarjoSwapChain swapChain = VarjoSwapChain::createD3D11(session.shared(), device.Get(), config);
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

    clearD3D11SwapChainImage(device.Get(), context.Get(), swapChain, imageIndex);
    vtk_hmd_test::updateLayerViews(layer, frameInfo, swapChain);

    swapChain.release();
    VTK_HMD_TEST_REQUIRE(!swapChain.acquired());
    VTK_HMD_TEST_REQUIRE(swapChain.acquiredIndex() == -1);

    VTK_HMD_TEST_REQUIRE(layerFrame.end(layer, frameInfo.frameNumber()));
}

} // namespace

int main()
{
    return vtk_hmd_test::runTest("VarjoToolkitHmdD3D11SwapChainSmokeTest", runD3D11SwapChainSmokeTest);
}
