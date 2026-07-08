#include "HmdTestCommon.hpp"

#include <VarjoToolkit/MR/VarjoCameraProperties.hpp>

#include <iostream>

int main()
{
    std::cout << "VarjoToolkit HMD MR camera properties smoke test\n";
    if (!requireRuntimeAvailable()) {
        return 1;
    }

    VarjoSession session;
    if (!requireSession(session)) {
        return 1;
    }

    VarjoCameraProperties cameraProperties(session.shared());
    if (!cameraProperties.valid()) {
        return hmdFail("VarjoCameraProperties failed to initialize");
    }
    if (cameraProperties.session() != session.get()) {
        return hmdFail("VarjoCameraProperties session pointer mismatch");
    }
    if (!cameraProperties.ownsSession()) {
        return hmdFail("VarjoCameraProperties should retain shared session ownership");
    }

    if (!cameraProperties.enumerate(true)) {
        return hmdFail(std::string("VarjoCameraProperties enumerate failed: ") + cameraProperties.lastError());
    }

    int supportedCount = 0;
    for (const auto type : cameraProperties.propertyTypes()) {
        const auto info = cameraProperties.propertyInfoCopy(type);
        if (!info.has_value()) {
            continue;
        }
        std::cout << VarjoCameraProperties::propertyTypeToString(type, true)
                  << " supported=" << (info->supported ? "true" : "false")
                  << " modes=" << info->supportedModes.size()
                  << " values=" << info->supportedValues.size()
                  << " currentMode=" << VarjoCameraProperties::propertyModeToString(info->currentMode)
                  << " currentValue=" << VarjoCameraProperties::propertyValueToString(type, info->currentValue)
                  << "\n";
        if (info->supported) {
            ++supportedCount;
        }
    }

    if (supportedCount <= 0) {
        return hmdFail("No supported MR camera properties were found");
    }

    std::cout << "[PASS] HMD MR camera properties smoke test passed\n";
    return 0;
}
