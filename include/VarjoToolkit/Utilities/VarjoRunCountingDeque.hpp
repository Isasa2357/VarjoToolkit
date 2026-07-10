#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <utility>

#include <VarjoToolkit/Utilities/VarjoSampleRateCounter.hpp>

namespace VarjoToolkit {

// std::deque-compatible container that keeps a per-run cumulative push count and
// sample-rate measurement. resetPerformance() clears queued data and resets the
// counters, allowing an existing service implementation to remain restart-safe.
template <typename T>
class RunCountingDeque : public std::deque<T> {
public:
    using Base = std::deque<T>;
    using Base::Base;

    void push_back(const T& value)
    {
        Base::push_back(value);
        received_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void push_back(T&& value)
    {
        Base::push_back(std::move(value));
        received_count_.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t receivedCount() const noexcept
    {
        return received_count_.load(std::memory_order_relaxed);
    }

    double samplesPerSecond() const
    {
        return rate_counter_.update(receivedCount());
    }

    void resetPerformance() noexcept
    {
        Base::clear();
        received_count_.store(0, std::memory_order_relaxed);
        rate_counter_.reset(0);
    }

private:
    std::atomic<uint64_t> received_count_{0};
    mutable SampleRateCounter rate_counter_;
};

} // namespace VarjoToolkit
