#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>

namespace VarjoToolkit {

// Lightweight rate calculator for services that already maintain a cumulative
// sample counter. update() returns the latest completed measurement in samples
// per second. Until one second has elapsed, it returns 0.0.
class SampleRateCounter {
public:
    double update(uint64_t totalSamples) const
    {
        const auto now = Clock::now();
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_) {
            initialized_ = true;
            previous_time_ = now;
            previous_total_ = totalSamples;
            samples_per_second_ = 0.0;
            return samples_per_second_;
        }

        const double elapsedSeconds = std::chrono::duration<double>(now - previous_time_).count();
        if (elapsedSeconds < 1.0) {
            return samples_per_second_;
        }

        const uint64_t delta = totalSamples >= previous_total_
            ? totalSamples - previous_total_
            : totalSamples;

        samples_per_second_ = static_cast<double>(delta) / elapsedSeconds;
        previous_time_ = now;
        previous_total_ = totalSamples;
        return samples_per_second_;
    }

    void reset(uint64_t totalSamples = 0) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_ = false;
        previous_time_ = Clock::time_point{};
        previous_total_ = totalSamples;
        samples_per_second_ = 0.0;
    }

private:
    using Clock = std::chrono::steady_clock;

    mutable std::mutex mutex_;
    mutable bool initialized_ = false;
    mutable Clock::time_point previous_time_{};
    mutable uint64_t previous_total_ = 0;
    mutable double samples_per_second_ = 0.0;
};

} // namespace VarjoToolkit
