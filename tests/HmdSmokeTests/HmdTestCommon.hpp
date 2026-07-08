#pragma once

#include <VarjoToolkit/Core/VarjoSession.hpp>

#include <cstdint>
#include <iostream>

inline int hmdFail(const char* message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

inline int hmdFail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

inline bool requireRuntimeAvailable()
{
    if (!VarjoSession::runtimeAvailable()) {
        std::cerr << "[FAIL] Varjo runtime is not available. Start Varjo Base and connect a Varjo HMD.\n";
        return false;
    }
    return true;
}

inline bool requireSession(VarjoSession& session)
{
    if (!session.valid()) {
        std::cerr << "[FAIL] VarjoSession failed to initialize. lastError=" << session.lastError() << "\n";
        return false;
    }
    if (!session.get()) {
        std::cerr << "[FAIL] VarjoSession::get returned null\n";
        return false;
    }
    return true;
}

inline bool requirePositiveViewCount(const VarjoSession& session, int32_t& outViewCount)
{
    outViewCount = session.viewCount();
    std::cout << "viewCount=" << outViewCount << "\n";
    if (outViewCount <= 0) {
        std::cerr << "[FAIL] varjo_GetViewCount returned no views\n";
        return false;
    }
    return true;
}
