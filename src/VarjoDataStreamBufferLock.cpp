#include <VarjoToolkit/DataStream/VarjoDataStreamBufferLock.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>

#include <utility>

VarjoDataStreamBufferLock::VarjoDataStreamBufferLock(varjo_Session* session, varjo_BufferId buffer_id)
    : session_(session)
    , buffer_id_(buffer_id)
{
    VTK_SD_LOG("VarjoDataStreamBufferLock raw constructor session=" << session_ << " bufferId=" << buffer_id_);
    lock();
}

VarjoDataStreamBufferLock::VarjoDataStreamBufferLock(std::shared_ptr<varjo_Session> session, varjo_BufferId buffer_id)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
    , buffer_id_(buffer_id)
{
    VTK_SD_LOG("VarjoDataStreamBufferLock shared constructor session=" << session_ << " bufferId=" << buffer_id_);
    lock();
}

VarjoDataStreamBufferLock::VarjoDataStreamBufferLock(const VarjoSession& session, varjo_BufferId buffer_id)
    : VarjoDataStreamBufferLock(session.shared(), buffer_id)
{}

VarjoDataStreamBufferLock::~VarjoDataStreamBufferLock()
{
    VTK_SD_LOG("VarjoDataStreamBufferLock destructor locked=" << (locked_ ? "true" : "false") << " bufferId=" << buffer_id_);
    unlock();
}

VarjoDataStreamBufferLock::VarjoDataStreamBufferLock(VarjoDataStreamBufferLock&& other) noexcept
    : session_owner_(std::move(other.session_owner_))
    , session_(std::exchange(other.session_, nullptr))
    , buffer_id_(std::exchange(other.buffer_id_, varjo_InvalidId))
    , locked_(std::exchange(other.locked_, false))
{
    VTK_SD_LOG("VarjoDataStreamBufferLock move constructor session=" << session_ << " bufferId=" << buffer_id_ << " locked=" << (locked_ ? "true" : "false"));
}

VarjoDataStreamBufferLock& VarjoDataStreamBufferLock::operator=(VarjoDataStreamBufferLock&& other) noexcept
{
    if (this != &other) {
        VTK_SD_LOG("VarjoDataStreamBufferLock move assignment releasing current locked=" << (locked_ ? "true" : "false"));
        unlock();
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        buffer_id_ = std::exchange(other.buffer_id_, varjo_InvalidId);
        locked_ = std::exchange(other.locked_, false);
        VTK_SD_LOG("VarjoDataStreamBufferLock move assignment new session=" << session_ << " bufferId=" << buffer_id_ << " locked=" << (locked_ ? "true" : "false"));
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
        VTK_SD_WARN("metadata requested while buffer is not locked");
        return varjo_BufferMetadata{};
    }
    const auto metadata = varjo_GetBufferMetadata(session_, buffer_id_);
    VTK_SD_TRACE("buffer metadata bufferId=" << buffer_id_ << " size=" << metadata.byteSize << " timestamp=" << metadata.timestamp);
    return metadata;
}

const void* VarjoDataStreamBufferLock::cpuData() const
{
    if (!locked_) {
        VTK_SD_WARN("const cpuData requested while buffer is not locked");
        return nullptr;
    }
    const void* data = varjo_GetBufferCPUData(session_, buffer_id_);
    VTK_SD_TRACE("const cpuData bufferId=" << buffer_id_ << " data=" << data);
    return data;
}

void* VarjoDataStreamBufferLock::cpuData()
{
    if (!locked_) {
        VTK_SD_WARN("cpuData requested while buffer is not locked");
        return nullptr;
    }
    void* data = varjo_GetBufferCPUData(session_, buffer_id_);
    VTK_SD_TRACE("cpuData bufferId=" << buffer_id_ << " data=" << data);
    return data;
}

void VarjoDataStreamBufferLock::unlock()
{
    if (locked_) {
        VTK_SD_LOG("varjo_UnlockDataStreamBuffer bufferId=" << buffer_id_);
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
        VTK_SD_WARN("lock skipped session=" << session_ << " bufferId=" << buffer_id_);
        return;
    }

    VTK_SD_LOG("varjo_LockDataStreamBuffer bufferId=" << buffer_id_);
    varjo_LockDataStreamBuffer(session_, buffer_id_);
    locked_ = true;
}
