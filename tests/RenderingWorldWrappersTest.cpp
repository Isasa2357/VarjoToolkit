#include <VarjoToolkit/Core/VarjoEventQueue.hpp>
#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>
#include <VarjoToolkit/Rendering/VarjoOcclusionMesh.hpp>
#include <VarjoToolkit/Rendering/VarjoSwapChain.hpp>
#include <VarjoToolkit/World/VarjoMarkerTracker.hpp>
#include <VarjoToolkit/World/VarjoWorld.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

bool expectEqual(const std::string& actual, const std::string& expected, const std::string& label)
{
    if (actual != expected) {
        std::cerr << "[FAIL] " << label << " expected=" << expected << " actual=" << actual << "\n";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit rendering/world wrappers test\n";

    if (!expectEqual(VarjoEventQueue::eventTypeToString(varjo_EventType_Visibility), "Visibility", "event type string")) {
        return 1;
    }
    if (!expectEqual(VarjoEventQueue::eventTypeToString(varjo_EventType_VisibilityMeshChange), "VisibilityMeshChange", "visibility mesh event type string")) {
        return 1;
    }

    VarjoWorld nullWorld(nullptr);
    if (nullWorld.valid()) {
        return fail("VarjoWorld constructed from nullptr should be invalid");
    }
    varjo_WorldObject object{};
    object.typeMask = varjo_WorldComponentTypeMask_Pose | varjo_WorldComponentTypeMask_ObjectMarker;
    if (!VarjoWorld::hasComponent(object, varjo_WorldComponentTypeMask_Pose)) {
        return fail("VarjoWorld::hasComponent should detect pose bit");
    }
    if (!expectEqual(VarjoWorld::markerErrorToString(varjo_WorldObjectMarkerError_None), "None", "marker error none")) {
        return 1;
    }

    VarjoMarkerTracker nullMarkerTracker(nullptr);
    if (nullMarkerTracker.valid()) {
        return fail("VarjoMarkerTracker constructed from nullptr should be invalid");
    }
    if (!nullMarkerTracker.markers(0).empty()) {
        return fail("null marker tracker should return no markers");
    }

    VarjoOcclusionMesh nullMesh(nullptr, 0);
    if (nullMesh.valid()) {
        return fail("VarjoOcclusionMesh constructed from nullptr should be invalid");
    }
    if (nullMesh.snapshot().valid) {
        return fail("invalid occlusion mesh snapshot should not be valid");
    }

    const auto config = VarjoSwapChain::makeConfig(varjo_TextureFormat_R8G8B8A8_UNORM, 640, 480, 3, 1);
    if (config.textureFormat != varjo_TextureFormat_R8G8B8A8_UNORM || config.textureWidth != 640 || config.textureHeight != 480 || config.numberOfTextures != 3) {
        return fail("VarjoSwapChain::makeConfig should store requested values");
    }
    VarjoSwapChain nullSwapChain = VarjoSwapChain::createD3D11(nullptr, nullptr, config);
    if (nullSwapChain.valid()) {
        return fail("null D3D11 swap chain creation should be invalid");
    }
    const auto viewport = nullSwapChain.fullViewport();
    if (viewport.width != 640 || viewport.height != 480 || viewport.swapChain != nullptr) {
        return fail("fullViewport should use config dimensions and current swapchain pointer");
    }

    VarjoMultiProjLayer layer(2, varjo_LayerFlag_BlendMode_AlphaBlend, varjo_SpaceView);
    if (layer.viewCount() != 2) {
        return fail("VarjoMultiProjLayer should store view count");
    }
    if (layer.get()->header.type != varjo_LayerMultiProjType || layer.get()->header.flags != varjo_LayerFlag_BlendMode_AlphaBlend || layer.get()->space != varjo_SpaceView) {
        return fail("VarjoMultiProjLayer header should be initialized");
    }
    layer.resize(1);
    if (layer.viewCount() != 1 || layer.get()->viewCount != 1 || layer.get()->views == nullptr) {
        return fail("VarjoMultiProjLayer resize should refresh native pointers");
    }

    std::vector<varjo_LayerHeader*> headers{layer.header()};
    const auto submitInfo = VarjoLayerFrame::makeSubmitInfo(headers, 123);
    if (submitInfo.frameNumber != 123 || submitInfo.layerCount != 1 || submitInfo.layers == nullptr) {
        return fail("VarjoLayerFrame::makeSubmitInfo should describe layers");
    }
    VarjoLayerFrame nullFrame(nullptr);
    if (nullFrame.begin()) {
        return fail("VarjoLayerFrame::begin should fail for null session");
    }

    std::cout << "[PASS] VarjoToolkit rendering/world wrappers test passed\n";
    return 0;
}
