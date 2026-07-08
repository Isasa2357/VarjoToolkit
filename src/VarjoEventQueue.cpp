#include <VarjoToolkit/Core/VarjoEventQueue.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>

#include <utility>

VarjoEventQueue::VarjoEventQueue(varjo_Session* session)
    : session_(session)
{
    VTK_SD_LOG("VarjoEventQueue raw constructor session=" << session_);
    resetEventStorage();
}

VarjoEventQueue::VarjoEventQueue(std::shared_ptr<varjo_Session> session)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
{
    VTK_SD_LOG("VarjoEventQueue shared constructor session=" << session_);
    resetEventStorage();
}

VarjoEventQueue::VarjoEventQueue(const VarjoSession& session)
    : VarjoEventQueue(session.shared())
{}

VarjoEventQueue::~VarjoEventQueue()
{
    if (event_) {
        VTK_SD_LOG("varjo_FreeEvent event=" << event_);
        varjo_FreeEvent(event_);
        event_ = nullptr;
    }
}

VarjoEventQueue::VarjoEventQueue(VarjoEventQueue&& other) noexcept
    : session_owner_(std::move(other.session_owner_))
    , session_(std::exchange(other.session_, nullptr))
    , event_(std::exchange(other.event_, nullptr))
    , last_error_(std::move(other.last_error_))
{
    VTK_SD_LOG("VarjoEventQueue move constructor session=" << session_ << " event=" << event_);
}

VarjoEventQueue& VarjoEventQueue::operator=(VarjoEventQueue&& other) noexcept
{
    if (this != &other) {
        if (event_) {
            VTK_SD_LOG("VarjoEventQueue move assignment freeing current event=" << event_);
            varjo_FreeEvent(event_);
        }
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        event_ = std::exchange(other.event_, nullptr);
        last_error_ = std::move(other.last_error_);
        VTK_SD_LOG("VarjoEventQueue move assignment new session=" << session_ << " event=" << event_);
    }
    return *this;
}

bool VarjoEventQueue::valid() const
{
    return session_ != nullptr && event_ != nullptr;
}

varjo_Session* VarjoEventQueue::session() const
{
    return session_;
}

varjo_Event* VarjoEventQueue::eventStorage() const
{
    return event_;
}

bool VarjoEventQueue::poll(varjo_Event& outEvent)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }
    if (!event_) {
        setLastError("event storage is null");
        return false;
    }

    if (varjo_PollEvent(session_, event_) != varjo_True) {
        VTK_SD_TRACE("varjo_PollEvent returned no event");
        return false;
    }

    outEvent = *event_;
    last_error_.clear();
    VTK_SD_LOG("polled event type=" << eventTypeToString(outEvent.header.type) << " raw=" << static_cast<int64_t>(outEvent.header.type));
    return true;
}

std::vector<varjo_Event> VarjoEventQueue::pollAll(size_t maxEvents)
{
    VTK_SD_LOG("pollAll maxEvents=" << maxEvents);
    std::vector<varjo_Event> out;
    out.reserve(maxEvents);
    for (size_t i = 0; i < maxEvents; ++i) {
        varjo_Event event{};
        if (!poll(event)) {
            break;
        }
        out.push_back(event);
    }
    VTK_SD_LOG("pollAll resultCount=" << out.size());
    return out;
}

const std::string& VarjoEventQueue::lastError() const
{
    return last_error_;
}

std::string VarjoEventQueue::eventTypeToString(varjo_EventType type)
{
    switch (type) {
    case varjo_EventType_Visibility: return "Visibility";
    case varjo_EventType_Button: return "Button";
    case varjo_EventType_TrackingStatus: return "TrackingStatus";
    case varjo_EventType_HeadsetStatus: return "HeadsetStatus";
    case varjo_EventType_DisplayStatus: return "DisplayStatus";
    case varjo_EventType_StandbyStatus: return "StandbyStatus";
    case varjo_EventType_Foreground: return "Foreground";
    case varjo_EventType_MRDeviceStatus: return "MRDeviceStatus";
    case varjo_EventType_MRCameraPropertyChange: return "MRCameraPropertyChange";
    case varjo_EventType_DataStreamStart: return "DataStreamStart";
    case varjo_EventType_DataStreamStop: return "DataStreamStop";
    case varjo_EventType_TextureSizeChange: return "TextureSizeChange";
    case varjo_EventType_MRChromaKeyConfigChange: return "MRChromaKeyConfigChange";
    case varjo_EventType_VisibilityMeshChange: return "VisibilityMeshChange";
    default: return "Unknown(" + std::to_string(type) + ")";
    }
}

void VarjoEventQueue::resetEventStorage()
{
    event_ = varjo_AllocateEvent();
    VTK_SD_LOG("varjo_AllocateEvent returned " << event_);
    if (!event_) {
        setLastError("failed to allocate Varjo event storage");
    }
}

void VarjoEventQueue::setLastError(std::string message) const
{
    last_error_ = std::move(message);
    VTK_SD_ERROR(last_error_);
}
