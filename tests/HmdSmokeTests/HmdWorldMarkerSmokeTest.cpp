#include "HmdTestCommon.hpp"

#include <VarjoToolkit/World/VarjoMarkerTracker.hpp>
#include <VarjoToolkit/World/VarjoWorld.hpp>

#include <iostream>

int main()
{
    std::cout << "VarjoToolkit HMD world/marker smoke test\n";
    if (!requireRuntimeAvailable()) {
        return 1;
    }

    VarjoSession session;
    if (!requireSession(session)) {
        return 1;
    }

    VarjoWorld world(session.shared());
    if (!world.valid()) {
        return hmdFail(std::string("VarjoWorld failed to initialize: ") + world.lastError());
    }
    if (world.session() != session.get()) {
        return hmdFail("VarjoWorld session pointer mismatch");
    }

    world.sync();
    const int64_t markerObjectCount = world.objectCount(varjo_WorldComponentTypeMask_ObjectMarker);
    std::cout << "markerObjectCount=" << markerObjectCount << "\n";
    if (markerObjectCount < 0) {
        return hmdFail("VarjoWorld objectCount returned a negative value");
    }

    const auto objects = world.objects(varjo_WorldComponentTypeMask_ObjectMarker);
    std::cout << "markerObjectVectorSize=" << objects.size() << "\n";
    if (markerObjectCount != static_cast<int64_t>(objects.size())) {
        return hmdFail("VarjoWorld objects size does not match objectCount");
    }

    VarjoMarkerTracker tracker(session.shared());
    if (!tracker.valid()) {
        return hmdFail("VarjoMarkerTracker failed to initialize");
    }

    const auto markers = tracker.markers(session.currentTime(), true);
    std::cout << "trackedMarkerCount=" << markers.size() << "\n";
    for (const auto& marker : markers) {
        std::cout << "marker objectId=" << marker.object.id
                  << " hasMarker=" << (marker.hasMarker ? "true" : "false")
                  << " hasPose=" << (marker.hasPose ? "true" : "false") << "\n";
    }

    std::cout << "[PASS] HMD world/marker smoke test passed\n";
    return 0;
}
