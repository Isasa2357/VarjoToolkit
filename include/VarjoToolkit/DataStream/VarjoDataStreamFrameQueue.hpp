#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

// Small bounded producer/consumer queue for Varjo DataStream service callbacks.
//
// The callback thread should call push(), while the writer thread calls waitSwap()
// and drain(). The queue drops the oldest frames when capacity is exceeded and
// returns the number of dropped frames to the caller so each service can account
// for its own statistics without duplicating queue/cv logic.
template <typename Frame>
class VarjoDataStreamFrameQueue {
public:
    explicit VarjoDataStreamFrameQueue(size_t capacity = 1)
        : capacity_((capacity > 0) ? capacity : 1)
    {}

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
    }

    size_t push(Frame&& frame)
    {
        size_t dropped = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(frame));
            while (queue_.size() > capacity_) {
                queue_.pop_front();
                ++dropped;
            }
        }
        cv_.notify_one();
        return dropped;
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
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Frame> queue_;
    size_t capacity_ = 1;
};
