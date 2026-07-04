#pragma once

#include <Varjo.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

// Utility for converting Varjo monotonic timestamps to Unix time.
//
// This class intentionally has no worker thread and no logging ownership. It is
// a lightweight helper that can be used from services, samples, or offline
// synchronization code whenever a Varjo timestamp needs to be mapped to the
// real-world Unix time domain.
class VarjoTimestampMapping {
public:
    struct Sample {
        varjo_Nanoseconds varjoTimestamp = 0;
        varjo_Nanoseconds varjoTimestampUnixNs = 0;
        int64_t varjoTimestampUnixUs = 0;
        int64_t systemTimestampUnixUs = 0;
        std::chrono::system_clock::time_point systemTimestamp{};
        int64_t deltaVarjoUnixMinusSystemUs = 0;
        bool valid = false;
    };

public:
    explicit VarjoTimestampMapping(std::shared_ptr<varjo_Session> session);

    bool convertVarjoTimestampToUnixNs(
        varjo_Nanoseconds varjoTimestamp,
        varjo_Nanoseconds& outUnixNs) const;

    bool convertVarjoTimestampToUnixUs(
        varjo_Nanoseconds varjoTimestamp,
        int64_t& outUnixUs) const;

    Sample sampleCurrentMapping() const;

    static int64_t systemTimeToUnixUs(std::chrono::system_clock::time_point timePoint);
    static std::string formatUtcIso8601(std::chrono::system_clock::time_point timePoint);
    static std::string formatLocalIso8601(std::chrono::system_clock::time_point timePoint);

private:
    std::shared_ptr<varjo_Session> session_;
};
