#pragma once

#include <Varjo.h>
#include <Varjo_datastream.h>

#include <memory>

#include <VarjoToolkit/Core/VarjoSession.hpp>

// RAII wrapper for varjo_LockDataStreamBuffer / varjo_UnlockDataStreamBuffer.
//
// This class is intended for use inside Varjo DataStream callbacks. It accepts
// raw varjo_Session*, std::shared_ptr<varjo_Session>, or VarjoSession for the
// same interop style as the other Toolkit wrappers.
class VarjoDataStreamBufferLock {
public:
    VarjoDataStreamBufferLock(varjo_Session* session, varjo_BufferId buffer_id);
    VarjoDataStreamBufferLock(std::shared_ptr<varjo_Session> session, varjo_BufferId buffer_id);
    VarjoDataStreamBufferLock(const VarjoSession& session, varjo_BufferId buffer_id);
    ~VarjoDataStreamBufferLock();

    VarjoDataStreamBufferLock(const VarjoDataStreamBufferLock&) = delete;
    VarjoDataStreamBufferLock& operator=(const VarjoDataStreamBufferLock&) = delete;

    VarjoDataStreamBufferLock(VarjoDataStreamBufferLock&& other) noexcept;
    VarjoDataStreamBufferLock& operator=(VarjoDataStreamBufferLock&& other) noexcept;

    bool locked() const;
    explicit operator bool() const { return locked(); }

    varjo_Session* session() const;
    std::shared_ptr<varjo_Session> sharedSession() const;
    bool ownsSession() const;

    varjo_BufferId bufferId() const;
    varjo_BufferMetadata metadata() const;

    const void* cpuData() const;
    void* cpuData();

    void unlock();

private:
    void lock();

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    varjo_BufferId buffer_id_ = varjo_InvalidId;
    bool locked_ = false;
};
