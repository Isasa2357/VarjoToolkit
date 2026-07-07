#include <VarjoToolkit/Core/VarjoEventQueue.hpp>
#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>
#include <VarjoToolkit/Rendering/VarjoOcclusionMesh.hpp>
#include <VarjoToolkit/Rendering/VarjoSwapChain.hpp>
#include <VarjoToolkit/World/VarjoMarkerTracker.hpp>
#include <VarjoToolkit/World/VarjoWorld.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

bool expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }
    return true;
}

bool expectEqualString(const std::string& actual, const std::string& expected, const std::string& message)
{
    if (actual != expected) {
        std::cerr << "[FAIL] " << message << " expected=" << expected << " actual=" << actual << "\n";
        return false;
    }
    return true;
}

varjo_Matrix makeMatrix(double base)
{
    varjo_Matrix m{};
    for (int i = 0; i < 16; ++i) {
        m.value[i] = base + i;
    }
    return m;
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit rendering/world detailed test\n";

    VarjoEventQueue eventQueue(static_cast<varjo_Session*>(nullptr));
    if (eventQueue.valid()) return fail("event queue with null session should be invalid even if storage exists");
    varjo_Event event{};
    if (eventQueue.poll(event)) return fail("poll should fail for null session");
    if (eventQueue.lastError().empty()) return fail("poll failure should set lastError");
    if (!eventQueue.pollAll(4).empty()) return fail("pollAll should return empty for null session");
    VarjoEventQueue movedQueue(std::move(eventQueue));
    if (movedQueue.poll(event)) return fail("moved null queue should still fail to poll without session");

    if (!expectEqualString(VarjoEventQueue::eventTypeToString(varjo_EventType_Button), "Button", "button event string")) return 1;
    if (!expectEqualString(VarjoEventQueue::eventTypeToString(varjo_EventType_DataStreamStart), "DataStreamStart", "datastream start string")) return 1;
    if (!expectEqualString(VarjoEventQueue::eventTypeToString(varjo_EventType_MRChromaKeyConfigChange), "MRChromaKeyConfigChange", "chroma event string")) return 1;

    VarjoWorld world(static_cast<varjo_Session*>(nullptr));
    if (world.valid()) return fail("null world should be invalid");
    if (world.objectCount(varjo_WorldComponentTypeMask_Pose) != 0) return fail("null world object count should be zero");
    if (!world.objects(varjo_WorldComponentTypeMask_Pose).empty()) return fail("null world objects should be empty");
    world.sync();
    if (world.lastError().empty()) return fail("sync on null world should set lastError");
    varjo_WorldPoseComponent pose{};
    if (world.getPoseComponent(1, pose, 0)) return fail("null world pose component should fail");
    varjo_WorldObjectMarkerComponent markerComponent{};
    if (world.getObjectMarkerComponent(1, markerComponent)) return fail("null world marker component should fail");
    world.setObjectMarkerTimeouts({}, 1000);
    world.setObjectMarkerFlags({}, varjo_WorldObjectMarkerFlags_DoPrediction);

    varjo_WorldObject object{};
    object.typeMask = varjo_WorldComponentTypeMask_ObjectMarker;
    if (!VarjoWorld::hasComponent(object, varjo_WorldComponentTypeMask_ObjectMarker)) return fail("marker component bit should be detected");
    if (VarjoWorld::hasComponent(object, varjo_WorldComponentTypeMask_Pose)) return fail("absent pose bit should not be detected");
    if (!expectEqualString(VarjoWorld::markerErrorToString(varjo_WorldObjectMarkerError_DuplicateID), "DuplicateID", "duplicate marker error")) return 1;

    VarjoMarkerTracker tracker(static_cast<varjo_Session*>(nullptr));
    if (tracker.valid()) return fail("null marker tracker should be invalid");
    tracker.setTimeouts({1, 2, 3}, 1000);
    tracker.setPredictionEnabled({1, 2, 3}, true);
    tracker.setPredictionEnabled({1, 2, 3}, false);
    if (!tracker.markers(0, false).empty()) return fail("null marker tracker should not return markers");

    VarjoOcclusionMesh mesh(static_cast<varjo_Session*>(nullptr), 2, varjo_WindingOrder_CounterClockwise);
    if (mesh.valid()) return fail("null occlusion mesh should be invalid");
    if (mesh.viewIndex() != 2) return fail("occlusion mesh view index should be stored");
    if (mesh.windingOrder() != varjo_WindingOrder_CounterClockwise) return fail("occlusion mesh winding order should be stored");
    if (mesh.get() != nullptr) return fail("invalid occlusion mesh native pointer should be null");
    auto movedMesh = std::move(mesh);
    if (movedMesh.valid()) return fail("moved invalid occlusion mesh should remain invalid");

    const auto swapConfig = VarjoSwapChain::makeConfig(varjo_TextureFormat_R8G8B8A8_UNORM, 1280, 720, 4, 2);
    if (swapConfig.textureWidth != 1280 || swapConfig.textureHeight != 720 || swapConfig.numberOfTextures != 4 || swapConfig.textureArraySize != 2) return fail("swapchain config values incorrect");
    VarjoSwapChain swap = VarjoSwapChain::createD3D12(static_cast<varjo_Session*>(nullptr), nullptr, swapConfig);
    if (swap.valid()) return fail("null D3D12 swapchain should be invalid");
    if (swap.acquired()) return fail("invalid swapchain should not be acquired");
    int32_t imageIndex = 99;
    if (swap.acquire(imageIndex)) return fail("invalid swapchain acquire should fail");
    if (imageIndex != -1) return fail("failed acquire should reset image index");
    swap.release();
    if (swap.acquiredIndex() != -1) return fail("release should reset acquired index");
    const auto viewport1 = swap.fullViewport(1);
    if (viewport1.arrayIndex != 1 || viewport1.width != 1280 || viewport1.height != 720) return fail("fullViewport should include requested array index and dimensions");
    VarjoSwapChain movedSwap = std::move(swap);
    if (movedSwap.valid()) return fail("moved invalid swapchain should remain invalid");

    VarjoMultiProjLayer layer(0);
    if (layer.viewCount() != 0 || layer.get()->viewCount != 0 || layer.get()->views != nullptr) return fail("empty layer should have no native views");
    layer.resize(2);
    if (layer.viewCount() != 2 || layer.get()->views == nullptr) return fail("layer resize should allocate native views");
    layer.setFlags(varjo_LayerFlag_BlendMode_AlphaBlend);
    layer.setSpace(varjo_SpaceLocal);
    if (layer.get()->header.flags != varjo_LayerFlag_BlendMode_AlphaBlend || layer.get()->space != varjo_SpaceLocal) return fail("layer flags/space should update");

    const auto projection = makeMatrix(10.0);
    const auto view = makeMatrix(20.0);
    layer.setView(0, projection, view, viewport1, nullptr);
    if (layer.view(0).projection.value[0] != 10.0 || layer.view(0).view.value[0] != 20.0 || layer.view(0).viewport.width != 1280) return fail("setView should store projection/view/viewport");
    bool threw = false;
    try {
        (void)layer.view(5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    if (!threw) return fail("view out of range should throw");

    VarjoLayerFrame frame(static_cast<varjo_Session*>(nullptr));
    if (frame.valid()) return fail("null layer frame should be invalid");
    if (frame.begin()) return fail("begin should fail for null layer frame");
    if (frame.lastError().empty()) return fail("begin failure should set lastError");
    if (frame.endEmpty(1)) return fail("endEmpty should fail for null layer frame");
    std::vector<varjo_LayerHeader*> headers;
    auto submitEmpty = VarjoLayerFrame::makeSubmitInfo(headers, 10);
    if (submitEmpty.frameNumber != 10 || submitEmpty.layerCount != 0 || submitEmpty.layers != nullptr) return fail("empty submit info should be valid");
    headers.push_back(layer.header());
    auto submitOne = VarjoLayerFrame::makeSubmitInfo(headers, 11);
    if (submitOne.frameNumber != 11 || submitOne.layerCount != 1 || submitOne.layers == nullptr) return fail("single layer submit info should be valid");

    std::cout << "[PASS] VarjoToolkit rendering/world detailed test passed\n";
    return 0;
}
