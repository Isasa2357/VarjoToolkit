#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>

#include <stdexcept>
#include <utility>

VarjoFrameInfo::VarjoFrameInfo(varjo_Session* session)
    : session_(session)
{
    if (session_) {
        frame_info_ = varjo_CreateFrameInfo(session_);
    }
}

VarjoFrameInfo::VarjoFrameInfo(std::shared_ptr<varjo_Session> session)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
{
    if (session_) {
        frame_info_ = varjo_CreateFrameInfo(session_);
    }
}

VarjoFrameInfo::VarjoFrameInfo(const VarjoSession& session)
    : VarjoFrameInfo(session.shared())
{}

VarjoFrameInfo::~VarjoFrameInfo()
{
    release();
}

VarjoFrameInfo::VarjoFrameInfo(VarjoFrameInfo&& other) noexcept
    : session_owner_(std::move(other.session_owner_))
    , session_(std::exchange(other.session_, nullptr))
    , frame_info_(std::exchange(other.frame_info_, nullptr))
{}

VarjoFrameInfo& VarjoFrameInfo::operator=(VarjoFrameInfo&& other) noexcept
{
    if (this != &other) {
        release();
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        frame_info_ = std::exchange(other.frame_info_, nullptr);
    }
    return *this;
}

bool VarjoFrameInfo::valid() const
{
    return session_ != nullptr && frame_info_ != nullptr;
}

bool VarjoFrameInfo::waitSync()
{
    if (!valid()) {
        return false;
    }

    varjo_WaitSync(session_, frame_info_);
    return true;
}

varjo_Session* VarjoFrameInfo::session() const
{
    return session_;
}

std::shared_ptr<varjo_Session> VarjoFrameInfo::sharedSession() const
{
    return session_owner_;
}

bool VarjoFrameInfo::ownsSession() const
{
    return static_cast<bool>(session_owner_);
}

varjo_FrameInfo* VarjoFrameInfo::get()
{
    return frame_info_;
}

const varjo_FrameInfo* VarjoFrameInfo::get() const
{
    return frame_info_;
}

int32_t VarjoFrameInfo::viewCount() const
{
    if (!valid()) {
        return 0;
    }
    return varjo_GetViewCount(session_);
}

const varjo_ViewInfo* VarjoFrameInfo::views() const
{
    if (!frame_info_) {
        return nullptr;
    }
    return frame_info_->views;
}

const varjo_ViewInfo& VarjoFrameInfo::view(int32_t index) const
{
    if (!valid() || index < 0 || index >= viewCount()) {
        throw std::out_of_range("VarjoFrameInfo::view index out of range");
    }
    return frame_info_->views[index];
}

varjo_Nanoseconds VarjoFrameInfo::displayTime() const
{
    if (!frame_info_) {
        return 0;
    }
    return frame_info_->displayTime;
}

int64_t VarjoFrameInfo::frameNumber() const
{
    if (!frame_info_) {
        return 0;
    }
    return frame_info_->frameNumber;
}

VarjoFrameInfoSnapshot VarjoFrameInfo::snapshot() const
{
    VarjoFrameInfoSnapshot out{};
    if (!valid()) {
        return out;
    }

    const int32_t count = viewCount();
    if (count > 0 && frame_info_->views) {
        out.views.assign(frame_info_->views, frame_info_->views + count);
    }
    out.displayTime = frame_info_->displayTime;
    out.frameNumber = frame_info_->frameNumber;
    out.valid = true;
    return out;
}

void VarjoFrameInfo::release()
{
    if (frame_info_) {
        varjo_FreeFrameInfo(frame_info_);
        frame_info_ = nullptr;
    }
    session_ = nullptr;
    session_owner_.reset();
}
