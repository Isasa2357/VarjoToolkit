#pragma once

#include <Varjo.h>

#include <cstdint>
#include <memory>
#include <string>

// RAII wrapper for varjo_Session.
//
// The default constructor opens a Varjo session. The session is stored as a
// shared_ptr so existing service classes that already accept
// std::shared_ptr<varjo_Session> can use VarjoSession::shared() directly.
class VarjoSession {
public:
    VarjoSession();
    explicit VarjoSession(std::shared_ptr<varjo_Session> session);
    ~VarjoSession() = default;

    VarjoSession(const VarjoSession&) = default;
    VarjoSession& operator=(const VarjoSession&) = default;
    VarjoSession(VarjoSession&&) noexcept = default;
    VarjoSession& operator=(VarjoSession&&) noexcept = default;

    static bool runtimeAvailable();

    bool initialize();
    void reset();

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Session* get() const;
    std::shared_ptr<varjo_Session> shared() const;

    varjo_Nanoseconds currentTime() const;
    int32_t viewCount() const;

    const std::string& lastError() const;

private:
    std::shared_ptr<varjo_Session> session_;
    std::string last_error_;
};
