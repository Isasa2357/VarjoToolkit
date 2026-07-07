#pragma once

#include <Varjo.h>

#include <cstdint>
#include <vector>

#include <VarjoToolkit/Core/VarjoSession.hpp>

// Copyable snapshot of varjo_FrameInfo.
//
// varjo_FrameInfo itself is allocated/freed through the Varjo C API. This
// snapshot is safe to store in buffers, queues, logs, and service data objects.
struct VarjoFrameInfoSnapshot {
    std::vector<varjo_ViewInfo> views;
    varjo_Nanoseconds displayTime = 0;
    int64_t frameNumber = 0;
    bool valid = false;
};

// RAII wrapper for varjo_FrameInfo.
//
// This class owns a varjo_FrameInfo* created with varjo_CreateFrameInfo and frees
// it with varjo_FreeFrameInfo. waitSync() updates the owned frame info through
// varjo_WaitSync.
class VarjoFrameInfo {
public:
    explicit VarjoFrameInfo(varjo_Session* session);
    explicit VarjoFrameInfo(const VarjoSession& session);
    ~VarjoFrameInfo();

    VarjoFrameInfo(const VarjoFrameInfo&) = delete;
    VarjoFrameInfo& operator=(const VarjoFrameInfo&) = delete;

    VarjoFrameInfo(VarjoFrameInfo&& other) noexcept;
    VarjoFrameInfo& operator=(VarjoFrameInfo&& other) noexcept;

    bool valid() const;
    explicit operator bool() const { return valid(); }

    bool waitSync();

    varjo_FrameInfo* get();
    const varjo_FrameInfo* get() const;

    int32_t viewCount() const;
    const varjo_ViewInfo* views() const;
    const varjo_ViewInfo& view(int32_t index) const;

    varjo_Nanoseconds displayTime() const;
    int64_t frameNumber() const;

    VarjoFrameInfoSnapshot snapshot() const;

private:
    void release();

private:
    varjo_Session* session_ = nullptr;
    varjo_FrameInfo* frame_info_ = nullptr;
};
