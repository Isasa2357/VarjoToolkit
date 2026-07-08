#include "HmdTestCommon.hpp"

#include <VarjoToolkit/Core/VarjoEventQueue.hpp>

#include <iostream>

int main()
{
    std::cout << "VarjoToolkit HMD event queue smoke test\n";
    if (!requireRuntimeAvailable()) {
        return 1;
    }

    VarjoSession session;
    if (!requireSession(session)) {
        return 1;
    }

    VarjoEventQueue events(session.shared());
    if (!events.valid()) {
        return hmdFail("VarjoEventQueue failed to initialize");
    }
    if (events.session() != session.get()) {
        return hmdFail("VarjoEventQueue session pointer mismatch");
    }
    if (!events.eventStorage()) {
        return hmdFail("VarjoEventQueue event storage is null");
    }

    const auto initialEvents = events.pollAll(32);
    std::cout << "initialEventCount=" << initialEvents.size() << "\n";
    for (const auto& event : initialEvents) {
        std::cout << "eventType=" << VarjoEventQueue::eventTypeToString(event.header.type)
                  << " raw=" << static_cast<int64_t>(event.header.type) << "\n";
    }

    varjo_Event singleEvent{};
    const bool hasSingleEvent = events.poll(singleEvent);
    std::cout << "singlePollHasEvent=" << (hasSingleEvent ? "true" : "false") << "\n";
    if (hasSingleEvent) {
        std::cout << "singleEventType=" << VarjoEventQueue::eventTypeToString(singleEvent.header.type) << "\n";
    }

    std::cout << "[PASS] HMD event queue smoke test passed\n";
    return 0;
}
