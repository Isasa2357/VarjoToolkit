#include "HmdTestCommon.hpp"

#include <VarjoToolkit/Rendering/VarjoOcclusionMesh.hpp>

#include <cstddef>
#include <iostream>

namespace {

bool testWinding(const VarjoSession& session, int32_t viewCount, varjo_WindingOrder windingOrder, const char* windingName)
{
    std::cout << "Testing occlusion mesh winding=" << windingName << "\n";
    for (int32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
        VarjoOcclusionMesh mesh(session.shared(), viewIndex, windingOrder);
        if (!mesh.valid()) {
            std::cerr << "[FAIL] Failed to create occlusion mesh for view " << viewIndex
                      << ": " << mesh.lastError() << "\n";
            return false;
        }
        if (mesh.viewIndex() != viewIndex) {
            std::cerr << "[FAIL] Occlusion mesh view index mismatch\n";
            return false;
        }
        if (mesh.windingOrder() != windingOrder) {
            std::cerr << "[FAIL] Occlusion mesh winding mismatch\n";
            return false;
        }

        const auto snapshot = mesh.snapshot();
        if (!snapshot.valid) {
            std::cerr << "[FAIL] Occlusion mesh snapshot is invalid for view " << viewIndex << "\n";
            return false;
        }
        if (snapshot.vertexCount != static_cast<int64_t>(snapshot.vertices.size())) {
            std::cerr << "[FAIL] Occlusion mesh vertex count mismatch for view " << viewIndex << "\n";
            return false;
        }
        if (!snapshot.vertices.empty() && (snapshot.vertices.size() % 3) != 0) {
            std::cerr << "[FAIL] Occlusion mesh vertex count is not divisible by 3 for view " << viewIndex << "\n";
            return false;
        }

        std::cout << "view=" << viewIndex
                  << " vertices=" << snapshot.vertices.size()
                  << " triangles=" << (snapshot.vertices.size() / 3) << "\n";
    }
    return true;
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit HMD occlusion mesh smoke test\n";
    if (!requireRuntimeAvailable()) {
        return 1;
    }

    VarjoSession session;
    if (!requireSession(session)) {
        return 1;
    }

    int32_t viewCount = 0;
    if (!requirePositiveViewCount(session, viewCount)) {
        return 1;
    }

    if (!testWinding(session, viewCount, varjo_WindingOrder_Clockwise, "clockwise")) {
        return 1;
    }
    if (!testWinding(session, viewCount, varjo_WindingOrder_CounterClockwise, "counterclockwise")) {
        return 1;
    }

    std::cout << "[PASS] HMD occlusion mesh smoke test passed\n";
    return 0;
}
