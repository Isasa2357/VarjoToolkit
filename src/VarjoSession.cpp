#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>

#include <utility>

namespace {

void shutdownVarjoSession(varjo_Session* session)
{
    if (session) {
        VTK_SD_LOG("varjo_SessionShutDown session=" << session);
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
        VTK_SD_WARN(last_error_);
    }
}

bool VarjoSession::runtimeAvailable()
{
    const bool available = varjo_IsAvailable() == varjo_True;
    VTK_SD_LOG("varjo_IsAvailable=" << (available ? "true" : "false"));
    return available;
}

bool VarjoSession::initialize()
{
    if (session_) {
        last_error_.clear();
        return true;
    }

    if (!runtimeAvailable()) {
        last_error_ = "Varjo runtime is not available";
        VTK_SD_ERROR(last_error_);
        return false;
    }

    VTK_SD_LOG("calling varjo_SessionInit");
    varjo_Session* raw_session = varjo_SessionInit();
    if (!raw_session) {
        last_error_ = "varjo_SessionInit returned null";
        VTK_SD_ERROR(last_error_);
        return false;
    }

    session_ = std::shared_ptr<varjo_Session>(raw_session, shutdownVarjoSession);
    last_error_.clear();
    VTK_SD_LOG("session initialized session=" << session_.get());
    return true;
}

void VarjoSession::reset()
{
    VTK_SD_LOG("VarjoSession::reset session=" << session_.get());
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
        VTK_SD_WARN("currentTime requested with null session");
        return 0;
    }
    return varjo_GetCurrentTime(session_.get());
}

int32_t VarjoSession::viewCount() const
{
    if (!session_) {
        VTK_SD_WARN("viewCount requested with null session");
        return 0;
    }
    return varjo_GetViewCount(session_.get());
}

const std::string& VarjoSession::lastError() const
{
    return last_error_;
}
