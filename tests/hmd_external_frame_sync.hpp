#pragma once

#include "hmd_test_common.hpp"

#include <atomic>
#include <functional>
#include <thread>
#include <utility>

namespace vtk_hmd_test {

inline bool waitSyncExternally(
    VarjoSession& session,
    VarjoFrameInfo& frameInfo)
{
    if (!session.valid() || !frameInfo.valid()) return false;
    varjo_WaitSync(session.get(), frameInfo.get());
    return true;
}

inline VarjoFrameInfoSnapshot captureFrameInfoExternally(
    VarjoSession& session,
    VarjoFrameInfo& frameInfo)
{
    if (!waitSyncExternally(session, frameInfo)) return {};
    return frameInfo.snapshot();
}

class ExternalFrameInfoPump {
public:
    using Callback = std::function<void(const VarjoFrameInfoSnapshot&)>;

    ExternalFrameInfoPump(VarjoSession& session, Callback callback)
        : session_(session)
        , frame_info_(session)
        , callback_(std::move(callback))
    {}

    ~ExternalFrameInfoPump()
    {
        stop();
    }

    ExternalFrameInfoPump(const ExternalFrameInfoPump&) = delete;
    ExternalFrameInfoPump& operator=(const ExternalFrameInfoPump&) = delete;

    bool start()
    {
        stop();
        if (!session_.valid() || !frame_info_.valid() || !callback_) {
            return false;
        }

        stop_requested_.store(false);
        worker_ = std::thread([this]() {
            while (!stop_requested_.load()) {
                const auto snapshot = captureFrameInfoExternally(
                    session_,
                    frame_info_);
                if (snapshot.valid) callback_(snapshot);
            }
        });
        return true;
    }

    void stop()
    {
        stop_requested_.store(true);
        if (worker_.joinable()) worker_.join();
    }

private:
    VarjoSession& session_;
    VarjoFrameInfo frame_info_;
    Callback callback_;
    std::atomic_bool stop_requested_{true};
    std::thread worker_;
};

} // namespace vtk_hmd_test
