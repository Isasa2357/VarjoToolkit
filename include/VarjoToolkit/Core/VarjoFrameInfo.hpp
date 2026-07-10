#pragma once

#include <Varjo.h>

#include <cstdint>
#include <memory>
#include <vector>

#include <VarjoToolkit/Core/VarjoSession.hpp>

// Copyable snapshot of one externally synchronized Varjo frame.
//
// VarjoToolkit does not call varjo_WaitSync. The rendering owner must update the
// wrapped varjo_FrameInfo, then call snapshot() and distribute the result to
// services that need frame timing, views, or head pose.
struct VarjoFrameInfoSnapshot {
    std::vector<varjo_ViewInfo> views;
    varjo_Nanoseconds displayTime = 0;
    int64_t frameNumber = 0;

    // Center pose associated with the synchronized frame. snapshot() reads this
    // through varjo_FrameGetPose after the external owner has called WaitSync.
    varjo_Matrix centerPose{};
    bool centerPoseValid = false;

    bool valid = false;
};

// RAII wrapper for the storage owned by varjo_FrameInfo.
//
// This class only allocates, exposes, snapshots, and frees varjo_FrameInfo. It
// intentionally has no waitSync method: frame pacing belongs to the renderer or
// another single external synchronization owner.
class VarjoFrameInfo {
public:
    explicit VarjoFrameInfo(varjo_Session* session);
    explicit VarjoFrameInfo(std::shared_ptr<varjo_Session> session);
    explicit VarjoFrameInfo(const VarjoSession& session);
    ~VarjoFrameInfo();

    VarjoFrameInfo(const VarjoFrameInfo&) = delete;
    VarjoFrameInfo& operator=(const VarjoFrameInfo&) = delete;

    VarjoFrameInfo(VarjoFrameInfo&& other) noexcept;
    VarjoFrameInfo& operator=(VarjoFrameInfo&& other) noexcept;

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Session* session() const;
    std::shared_ptr<varjo_Session> sharedSession() const;
    bool ownsSession() const;

    varjo_FrameInfo* get();
    const varjo_FrameInfo* get() const;

    int32_t viewCount() const;
    const varjo_ViewInfo* views() const;
    const varjo_ViewInfo& view(int32_t index) const;

    varjo_Nanoseconds displayTime() const;
    int64_t frameNumber() const;

    // The caller must invoke varjo_WaitSync(session(), get()) before snapshot().
    VarjoFrameInfoSnapshot snapshot() const;

private:
    void release();

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    varjo_FrameInfo* frame_info_ = nullptr;
};
