#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>

// Small bounded producer/consumer queue for Varjo DataStream service callbacks.
//
// The callback thread should call push(), while the writer thread calls waitSwap()
// and drain(). The queue drops the oldest frames when capacity is exceeded and
// returns the number of dropped frames to the caller so each service can account
// for its own statistics without duplicating queue/cv logic.
//
// The queue also maintains per-run receive counters. clear() starts a new run and
// resets the total/per-channel counters and their lightweight samples-per-second
// measurements. If Frame has a public channel_index member, per-channel metrics
// are recorded automatically.
template <typename Frame>
class VarjoDataStreamFrameQueue {
public:
    explicit VarjoDataStreamFrameQueue(size_t capacity = 1)
        : capacity_((capacity > 0) ? capacity : 1)
    {
        resetMetricsLocked(Clock::now());
    }

    VarjoDataStreamFrameQueue(const VarjoDataStreamFrameQueue&) = delete;
    VarjoDataStreamFrameQueue& operator=(const VarjoDataStreamFrameQueue&) = delete;

    void setCapacity(size_t capacity)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_ = (capacity > 0) ? capacity : 1;
        while (queue_.size() > capacity_) {
            queue_.pop_front();
        }
    }

    size_t capacity() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return capacity_;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        resetMetricsLocked(Clock::now());
    }

    size_t push(Frame&& frame)
    {
        size_t dropped = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++total_pushed_;
            if constexpr (HasChannelIndex<Frame>::value) {
                const int64_t channel = static_cast<int64_t>(frame.channel_index);
                ++pushed_by_channel_[channel];
                ensureChannelRateStateLocked(channel);
            }

            queue_.push_back(std::move(frame));
            while (queue_.size() > capacity_) {
                queue_.pop_front();
                ++dropped;
            }
        }
        cv_.notify_one();
        return dropped;
    }

    uint64_t pushedCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return total_pushed_;
    }

    uint64_t pushedCountForChannel(int64_t channel) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = pushed_by_channel_.find(channel);
        return it == pushed_by_channel_.end() ? 0 : it->second;
    }

    double pushedRatePerSecond() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return updateRateLocked(total_pushed_, total_rate_state_);
    }

    double pushedRatePerSecondForChannel(int64_t channel) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureChannelRateStateLocked(channel);
        const auto count_it = pushed_by_channel_.find(channel);
        const uint64_t count = count_it == pushed_by_channel_.end() ? 0 : count_it->second;
        return updateRateLocked(count, channel_rate_states_[channel]);
    }

    template <typename StopPredicate>
    void waitSwap(std::deque<Frame>& out, StopPredicate stopPredicate)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&]() {
            return stopPredicate() || !queue_.empty();
        });
        out.swap(queue_);
    }

    void drain(std::deque<Frame>& out)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        out.swap(queue_);
    }

    void notifyAll()
    {
        cv_.notify_all();
    }

private:
    using Clock = std::chrono::steady_clock;

    template <typename T, typename = void>
    struct HasChannelIndex : std::false_type {};

    template <typename T>
    struct HasChannelIndex<T, std::void_t<decltype(std::declval<const T&>().channel_index)>> : std::true_type {};

    struct RateState {
        Clock::time_point previous_time{};
        uint64_t previous_total = 0;
        double samples_per_second = 0.0;
    };

    void resetMetricsLocked(Clock::time_point now)
    {
        run_start_ = now;
        total_pushed_ = 0;
        pushed_by_channel_.clear();
        total_rate_state_ = RateState{run_start_, 0, 0.0};
        channel_rate_states_.clear();
    }

    void ensureChannelRateStateLocked(int64_t channel) const
    {
        if (channel_rate_states_.find(channel) == channel_rate_states_.end()) {
            channel_rate_states_.emplace(channel, RateState{run_start_, 0, 0.0});
        }
    }

    double updateRateLocked(uint64_t total, RateState& state) const
    {
        const auto now = Clock::now();
        const double elapsed_seconds = std::chrono::duration<double>(now - state.previous_time).count();
        if (elapsed_seconds < 1.0) {
            return state.samples_per_second;
        }

        const uint64_t delta = total >= state.previous_total ? total - state.previous_total : total;
        state.samples_per_second = static_cast<double>(delta) / elapsed_seconds;
        state.previous_time = now;
        state.previous_total = total;
        return state.samples_per_second;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Frame> queue_;
    size_t capacity_ = 1;

    Clock::time_point run_start_{};
    uint64_t total_pushed_ = 0;
    std::unordered_map<int64_t, uint64_t> pushed_by_channel_;
    mutable RateState total_rate_state_{};
    mutable std::unordered_map<int64_t, RateState> channel_rate_states_;
};
