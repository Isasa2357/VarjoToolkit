#pragma once

#include <Varjo.h>
#include <Varjo_mr.h>

#include <memory>
#include <string>

#include <VarjoToolkit/Core/VarjoSession.hpp>

// RAII wrapper for Varjo mixed reality feature locks.
//
// This class wraps varjo_Lock / varjo_Unlock and supports raw varjo_Session*,
// std::shared_ptr<varjo_Session>, and VarjoSession construction. The lock is
// acquired in the constructor and released in the destructor unless unlock() is
// called earlier.
class VarjoScopedLock {
public:
    VarjoScopedLock(varjo_Session* session, varjo_LockType lockType);
    VarjoScopedLock(std::shared_ptr<varjo_Session> session, varjo_LockType lockType);
    VarjoScopedLock(const VarjoSession& session, varjo_LockType lockType);
    ~VarjoScopedLock();

    VarjoScopedLock(const VarjoScopedLock&) = delete;
    VarjoScopedLock& operator=(const VarjoScopedLock&) = delete;

    VarjoScopedLock(VarjoScopedLock&& other) noexcept;
    VarjoScopedLock& operator=(VarjoScopedLock&& other) noexcept;

    bool locked() const;
    explicit operator bool() const { return locked(); }

    varjo_Session* session() const;
    std::shared_ptr<varjo_Session> sharedSession() const;
    bool ownsSession() const;

    varjo_LockType lockType() const;
    std::string lastError() const;

    bool lock();
    void unlock();
    void release() { unlock(); }

    static std::string lockTypeToString(varjo_LockType lockType);

private:
    void setLastError(std::string message);

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    varjo_LockType lock_type_ = 0;
    bool locked_ = false;
    std::string last_error_;
};
