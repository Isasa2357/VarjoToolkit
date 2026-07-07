#include <VarjoToolkit/DataStream/VarjoDataStreamBufferLock.hpp>

#include <utility>

VarjoDataStreamBufferLock::VarjoDataStreamBufferLock(varjo_Session* session, varjo_BufferId buffer_id)
    : session_(session)
    , buffer_id_(buffer_id)
{
    lock();
}

VarjoDataStreamBufferLock::VarjoDataStreamBufferLock(std::shared_ptr<varjo_Session> session, varjo_BufferId buffer_id)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
    , buffer_id_(buffer_id)
{
    lock();
}

VarjoDataStreamBufferLock::VarjoDataStreamBufferLock(const VarjoSession& session, varjo_BufferId buffer_id)
    : VarjoDataStreamBufferLock(session.shared(), buffer_id)
{}

VarjoDataStreamBufferLock::~VarjoDataStreamBufferLock()
{
    unlock();
}

VarjoDataStreamBufferLock::VarjoDataStreamBufferLock(VarjoDataStreamBufferLock&& other) noexcept
    : session_owner_(std::move(other.session_owner_))
    , session_(std::exchange(other.session_, nullptr))
    , buffer_id_(std::exchange(other.buffer_id_, varjo_InvalidId))
    , locked_(std::exchange(other.locked_, false))
{}

VarjoDataStreamBufferLock& VarjoDataStreamBufferLock::operator=(VarjoDataStreamBufferLock&& other) noexcept
{
    if (this != &other) {
        unlock();
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        buffer_id_ = std::exchange(other.buffer_id_, varjo_InvalidId);
        locked_ = std::exchange(other.locked_, false);
    }
    return *this;
}

bool VarjoDataStreamBufferLock::locked() const
{
    return locked_;
}

varjo_Session* VarjoDataStreamBufferLock::session() const
{
    return session_;
}

std::shared_ptr<varjo_Session> VarjoDataStreamBufferLock::sharedSession() const
{
    return session_owner_;
}

bool VarjoDataStreamBufferLock::ownsSession() const
{
    return static_cast<bool>(session_owner_);
}

varjo_BufferId VarjoDataStreamBufferLock::bufferId() const
{
    return buffer_id_;
}

varjo_BufferMetadata VarjoDataStreamBufferLock::metadata() const
{
    if (!locked_) {
        return varjo_BufferMetadata{};
    }
    return varjo_GetBufferMetadata(session_, buffer_id_);
}

const void* VarjoDataStreamBufferLock::cpuData() const
{
    if (!locked_) {
        return nullptr;
    }
    return varjo_GetBufferCPUData(session_, buffer_id_);
}

void* VarjoDataStreamBufferLock::cpuData()
{
    if (!locked_) {
        return nullptr;
    }
    return varjo_GetBufferCPUData(session_, buffer_id_);
}

void VarjoDataStreamBufferLock::unlock()
{
    if (locked_) {
        varjo_UnlockDataStreamBuffer(session_, buffer_id_);
        locked_ = false;
    }
    session_ = nullptr;
    buffer_id_ = varjo_InvalidId;
    session_owner_.reset();
}

void VarjoDataStreamBufferLock::lock()
{
    if (!session_ || buffer_id_ == varjo_InvalidId) {
        return;
    }

    varjo_LockDataStreamBuffer(session_, buffer_id_);
    locked_ = true;
}
