#include "HmdTestCommon.hpp"

#include <VarjoToolkit/DataStream/VarjoDataStream.hpp>

#include <iostream>

int main()
{
    std::cout << "VarjoToolkit HMD data stream config smoke test\n";
    if (!requireRuntimeAvailable()) {
        return 1;
    }

    VarjoSession session;
    if (!requireSession(session)) {
        return 1;
    }

    VarjoDataStream dataStream(session.shared());
    if (!dataStream.valid()) {
        return hmdFail("VarjoDataStream failed to initialize");
    }
    if (dataStream.session() != session.get()) {
        return hmdFail("VarjoDataStream session pointer mismatch");
    }
    if (!dataStream.ownsSession()) {
        return hmdFail("VarjoDataStream should retain shared session ownership");
    }

    const auto configs = dataStream.enumerateConfigs();
    std::cout << "dataStreamConfigCount=" << configs.size() << "\n";
    if (configs.empty()) {
        return hmdFail("No Varjo data stream configs were reported by the runtime");
    }

    for (size_t i = 0; i < configs.size(); ++i) {
        const auto& config = configs[i];
        std::cout << "config[" << i << "]"
                  << " streamId=" << config.streamId
                  << " streamType=" << static_cast<int64_t>(config.streamType)
                  << " channelFlags=" << static_cast<int64_t>(config.channelFlags)
                  << " bufferType=" << static_cast<int64_t>(config.bufferType)
                  << " format=" << static_cast<int64_t>(config.format)
                  << " width=" << config.width
                  << " height=" << config.height
                  << " frameRate=" << config.frameRate << "\n";
    }

    std::cout << "[PASS] HMD data stream config smoke test passed\n";
    return 0;
}
