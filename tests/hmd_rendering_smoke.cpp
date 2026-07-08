#include "hmd_test_common.hpp"

#include <stdexcept>

namespace {

void runRenderingSmokeTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    VarjoFrameInfo frameInfo(session);
    VTK_HMD_TEST_REQUIRE(frameInfo.valid());
    VTK_HMD_TEST_REQUIRE(frameInfo.waitSync());
    VTK_HMD_TEST_REQUIRE(frameInfo.viewCount() > 0);

    const int32_t viewCount = frameInfo.viewCount();
    std::cout << "Rendering smoke viewCount=" << viewCount << " frameNumber=" << frameInfo.frameNumber() << '\n';

    VarjoMultiProjLayer layer(viewCount, varjo_LayerFlagNone, varjo_SpaceLocal);
    VTK_HMD_TEST_REQUIRE(layer.viewCount() == viewCount);
    VTK_HMD_TEST_REQUIRE(layer.get() != nullptr);
    VTK_HMD_TEST_REQUIRE(layer.header() != nullptr);
    VTK_HMD_TEST_REQUIRE(layer.header()->type == varjo_LayerMultiProjType);
    VTK_HMD_TEST_REQUIRE(layer.header()->flags == varjo_LayerFlag_BlendMode_AlphaBlend);
    VTK_HMD_TEST_REQUIRE(layer.get()->space == varjo_SpaceLocal);
    VTK_HMD_TEST_REQUIRE(layer.get()->viewCount == viewCount);
    VTK_HMD_TEST_REQUIRE(layer.get()->views != nullptr);

    layer.setFlags(varjo_LayerFlag_BlendMode_AlphaBlend | varjo_LayerFlag_DepthTesting);
    VTK_HMD_TEST_REQUIRE((layer.header()->flags & varjo_LayerFlag_DepthTesting) != 0);
    layer.setSpace(varjo_SpaceView);
    VTK_HMD_TEST_REQUIRE(layer.get()->space == varjo_SpaceView);
    layer.setSpace(varjo_SpaceLocal);

    for (int32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
        const auto& view = frameInfo.view(viewIndex);
        varjo_SwapChainViewport viewport{};
        viewport.swapChain = nullptr;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = std::max<int32_t>(1, view.preferredWidth);
        viewport.height = std::max<int32_t>(1, view.preferredHeight);
        viewport.arrayIndex = viewIndex;

        layer.setView(
            static_cast<size_t>(viewIndex),
            vtk_hmd_test::copyMatrix(view.projectionMatrix),
            vtk_hmd_test::copyMatrix(view.viewMatrix),
            viewport);

        const auto& layerView = layer.view(static_cast<size_t>(viewIndex));
        VTK_HMD_TEST_REQUIRE(layerView.viewport.width == viewport.width);
        VTK_HMD_TEST_REQUIRE(layerView.viewport.height == viewport.height);
        VTK_HMD_TEST_REQUIRE(layerView.viewport.arrayIndex == viewIndex);
        VTK_HMD_TEST_REQUIRE(layerView.projection.value[0] == view.projectionMatrix[0]);
        VTK_HMD_TEST_REQUIRE(layerView.view.value[0] == view.viewMatrix[0]);
    }

    bool outOfRangeThrew = false;
    try {
        (void)layer.view(static_cast<size_t>(viewCount));
    } catch (const std::out_of_range&) {
        outOfRangeThrew = true;
    }
    VTK_HMD_TEST_REQUIRE(outOfRangeThrew);

    auto* header = layer.header();
    std::vector<varjo_LayerHeader*> layers{header};
    const auto submitInfo = VarjoLayerFrame::makeSubmitInfo(layers, frameInfo.frameNumber());
    VTK_HMD_TEST_REQUIRE(submitInfo.frameNumber == frameInfo.frameNumber());
    VTK_HMD_TEST_REQUIRE(submitInfo.layerCount == 1);
    VTK_HMD_TEST_REQUIRE(submitInfo.layers != nullptr);
    VTK_HMD_TEST_REQUIRE(submitInfo.layers[0] == header);

    VarjoLayerFrame layerFrame(session.shared());
    VTK_HMD_TEST_REQUIRE(layerFrame.valid());
    VTK_HMD_TEST_REQUIRE(layerFrame.session() == session.get());
    VTK_HMD_TEST_REQUIRE(layerFrame.begin());
    VTK_HMD_TEST_REQUIRE(layerFrame.endEmpty(frameInfo.frameNumber()));

    VarjoLayerFrame invalidLayerFrame(static_cast<varjo_Session*>(nullptr));
    VTK_HMD_TEST_REQUIRE(!invalidLayerFrame.valid());
    VTK_HMD_TEST_REQUIRE(!invalidLayerFrame.begin());
    VTK_HMD_TEST_REQUIRE(!invalidLayerFrame.lastError().empty());
    VTK_HMD_TEST_REQUIRE(!invalidLayerFrame.endEmpty(frameInfo.frameNumber()));
}

} // namespace

int main()
{
    return vtk_hmd_test::runTest("VarjoToolkitHmdRenderingSmokeTest", runRenderingSmokeTest);
}
