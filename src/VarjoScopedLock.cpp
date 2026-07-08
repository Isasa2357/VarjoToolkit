#include <VarjoToolkit/Core/VarjoScopedLock.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>

#include <utility>

VarjoScopedLock::VarjoScopedLock(varjo_Session* session, varjo_LockType lockType)
    : session_(session)
    , lock_type_(lockType)
{
    VTK_SD_LOG("VarjoScopedLock raw constructor session=" << session_ << " lockType=" << lockTypeToString(lock_type_));
    lock();
}

VarjoScopedLock::VarjoScopedLock(std::shared_ptr<varjo_Session> session, varjo_LockType lockType)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
    , lock_type_(lockType)
{
    VTK_SD_LOG("VarjoScopedLock shared constructor session=" << session_ << " lockType=" << lockTypeToString(lock_type_));
    lock();
}

VarjoScopedLock::VarjoScopedLock(const VarjoSession& session, varjo_LockType lockType)
    : VarjoScopedLock(session.shared(), lockType)
{}

VarjoScopedLock::~VarjoScopedLock()
{
    VTK_SD_LOG("VarjoScopedLock destructor locked=" << (locked_ ? "true" : "false") << " lockType=" << lockTypeToString(lock_type_));
    unlock();
}

VarjoScopedLock::VarjoScopedLock(VarjoScopedLock&& other) noexcept
    : session_owner_(std::move(other.session_owner_))
    , session_(std::exchange(other.session_, nullptr))
    , lock_type_(std::exchange(other.lock_type_, 0))
    , locked_(std::exchange(other.locked_, false))
    , last_error_(std::move(other.last_error_))
{
    VTK_SD_LOG("VarjoScopedLock move constructor session=" << session_ << " lockType=" << lockTypeToString(lock_type_) << " locked=" << (locked_ ? "true" : "false"));
}

VarjoScopedLock& VarjoScopedLock::operator=(VarjoScopedLock&& other) noexcept
{
    if (this != &other) {
        VTK_SD_LOG("VarjoScopedLock move assignment releasing current locked=" << (locked_ ? "true" : "false"));
        unlock();
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        lock_type_ = std::exchange(other.lock_type_, 0);
        locked_ = std::exchange(other.locked_, false);
        last_error_ = std::move(other.last_error_);
        VTK_SD_LOG("VarjoScopedLock move assignment new session=" << session_ << " lockType=" << lockTypeToString(lock_type_) << " locked=" << (locked_ ? "true" : "false"));
    }
    return *this;
}

bool VarjoScopedLock::locked() const
{
    return locked_;
}

varjo_Session* VarjoScopedLock::session() const
{
    return session_;
}

std::shared_ptr<varjo_Session> VarjoScopedLock::sharedSession() const
{
    return session_owner_;
}

bool VarjoScopedLock::ownsSession() const
{
    return static_cast<bool>(session_owner_);
}

varjo_LockType VarjoScopedLock::lockType() const
{
    return lock_type_;
}

std::string VarjoScopedLock::lastError() const
{
    return last_error_;
}

bool VarjoScopedLock::lock()
{
    VTK_SD_SCOPE("VarjoScopedLock::lock");
    if (locked_) {
        VTK_SD_LOG("lock already held lockType=" << lockTypeToString(lock_type_));
        return true;
    }
    if (!session_) {
        setLastError("session is null");
        return false;
    }
    if (lock_type_ == 0) {
        setLastError("lock type is invalid");
        return false;
    }

    VTK_SD_LOG("varjo_Lock lockType=" << lockTypeToString(lock_type_));
    locked_ = (varjo_Lock(session_, lock_type_) == varjo_True);
    if (!locked_) {
        setLastError("failed to acquire Varjo lock: " + lockTypeToString(lock_type_));
        return false;
    }

    last_error_.clear();
    VTK_SD_LOG("lock acquired lockType=" << lockTypeToString(lock_type_));
    return true;
}

void VarjoScopedLock::unlock()
{
    if (locked_ && session_) {
        VTK_SD_LOG("varjo_Unlock lockType=" << lockTypeToString(lock_type_));
        varjo_Unlock(session_, lock_type_);
    }
    locked_ = false;
}

std::string VarjoScopedLock::lockTypeToString(varjo_LockType lockType)
{
    switch (lockType) {
    case varjo_LockType_Camera:
        return "Camera";
    case varjo_LockType_ChromaKey:
        return "ChromaKey";
    case varjo_LockType_EnvironmentCubemap:
        return "EnvironmentCubemap";
    default:
        return "Unknown(" + std::to_string(lockType) + ")";
    }
}

void VarjoScopedLock::setLastError(std::string message)
{
    last_error_ = std::move(message);
    VTK_SD_ERROR(last_error_);
}
