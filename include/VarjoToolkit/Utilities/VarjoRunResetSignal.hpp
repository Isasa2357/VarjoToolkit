#pragma once

#include <atomic>
#include <type_traits>

namespace VarjoToolkit {

// Atomic stop/run signal that resets an associated per-run performance object
// whenever store(false) starts a new run. The reset target must provide
// resetPerformance(). This keeps existing worker code compatible with the
// std::atomic_bool store/load interface while ensuring restart-safe metrics.
class RunResetSignal {
public:
    RunResetSignal() = default;

    template <typename Resettable>
    RunResetSignal(bool initialValue, Resettable* resettable) noexcept
        : value_(initialValue)
        , reset_target_(resettable)
        , reset_function_([](void* target) noexcept {
            static_cast<Resettable*>(target)->resetPerformance();
        })
    {}

    RunResetSignal(const RunResetSignal&) = delete;
    RunResetSignal& operator=(const RunResetSignal&) = delete;

    void store(bool value, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        if (!value && reset_target_ && reset_function_) {
            reset_function_(reset_target_);
        }
        value_.store(value, order);
    }

    bool load(std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return value_.load(order);
    }

    operator bool() const noexcept
    {
        return load();
    }

private:
    using ResetFunction = void (*)(void*) noexcept;

    std::atomic_bool value_{true};
    void* reset_target_ = nullptr;
    ResetFunction reset_function_ = nullptr;
};

} // namespace VarjoToolkit
