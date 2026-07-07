#include <VarjoToolkit/Core/VarjoSession.hpp>

#include <utility>

namespace {

void shutdownVarjoSession(varjo_Session* session)
{
    if (session) {
        varjo_SessionShutDown(session);
    }
}

} // namespace

VarjoSession::VarjoSession()
{
    initialize();
}

VarjoSession::VarjoSession(std::shared_ptr<varjo_Session> session)
    : session_(std::move(session))
{
    if (!session_) {
        last_error_ = "session is null";
    }
}

bool VarjoSession::runtimeAvailable()
{
    return varjo_IsAvailable() == varjo_True;
}

bool VarjoSession::initialize()
{
    if (session_) {
        last_error_.clear();
        return true;
    }

    if (!runtimeAvailable()) {
        last_error_ = "Varjo runtime is not available";
        return false;
    }

    varjo_Session* raw_session = varjo_SessionInit();
    if (!raw_session) {
        last_error_ = "varjo_SessionInit returned null";
        return false;
    }

    session_ = std::shared_ptr<varjo_Session>(raw_session, shutdownVarjoSession);
    last_error_.clear();
    return true;
}

void VarjoSession::reset()
{
    session_.reset();
}

bool VarjoSession::valid() const
{
    return session_ != nullptr;
}

varjo_Session* VarjoSession::get() const
{
    return session_.get();
}

std::shared_ptr<varjo_Session> VarjoSession::shared() const
{
    return session_;
}

varjo_Nanoseconds VarjoSession::currentTime() const
{
    if (!session_) {
        return 0;
    }
    return varjo_GetCurrentTime(session_.get());
}

int32_t VarjoSession::viewCount() const
{
    if (!session_) {
        return 0;
    }
    return varjo_GetViewCount(session_.get());
}

const std::string& VarjoSession::lastError() const
{
    return last_error_;
}
