#include "WrapperExamples.hpp"

#include <VarjoToolkit/MR/VarjoCameraProperties.hpp>
#include <VarjoToolkit/MR/VarjoChromaKey.hpp>

#include <iostream>

void printWrapperUsageExamples(const std::shared_ptr<varjo_Session>& session)
{
    std::cout << "Wrapper examples:\n";

    VarjoCameraProperties cameraProperties(session);
    if (cameraProperties.enumerate(true)) {
        std::cout << "  Camera property wrapper: available\n";
        for (const auto type : cameraProperties.propertyTypes()) {
            const auto* info = cameraProperties.propertyInfo(type);
            if (info && info->valid) {
                std::cout << "    " << cameraProperties.propertyAsString(type) << "\n";
                break;
            }
        }
    } else {
        std::cout << "  Camera property wrapper: " << cameraProperties.lastError() << "\n";
    }

    VarjoChromaKey chromaKey(session);
    std::cout << "  Chroma key wrapper: configSlots=" << chromaKey.configCount() << "\n";
    const auto disabledConfig = VarjoChromaKey::makeDisabledConfig();
    std::cout << "  Chroma key disabled config: " << VarjoChromaKey::configToString(disabledConfig) << "\n";
}
