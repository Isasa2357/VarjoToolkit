#include <VarjoToolkit/World/VarjoWorld.hpp>

#include <utility>

VarjoWorld::VarjoWorld(varjo_Session* session, varjo_WorldFlags flags)
    : session_(session)
    , flags_(flags)
{
    if (session_) {
        world_ = varjo_WorldInit(session_, flags_);
    }
    if (!world_) {
        setLastError("failed to initialize Varjo world");
    }
}

VarjoWorld::VarjoWorld(std::shared_ptr<varjo_Session> session, varjo_WorldFlags flags)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
    , flags_(flags)
{
    if (session_) {
        world_ = varjo_WorldInit(session_, flags_);
    }
    if (!world_) {
        setLastError("failed to initialize Varjo world");
    }
}

VarjoWorld::VarjoWorld(const VarjoSession& session, varjo_WorldFlags flags)
    : VarjoWorld(session.shared(), flags)
{}

VarjoWorld::~VarjoWorld()
{
    if (world_) {
        varjo_WorldDestroy(world_);
        world_ = nullptr;
    }
}

VarjoWorld::VarjoWorld(VarjoWorld&& other) noexcept
    : session_owner_(std::move(other.session_owner_))
    , session_(std::exchange(other.session_, nullptr))
    , world_(std::exchange(other.world_, nullptr))
    , flags_(std::exchange(other.flags_, 0))
    , last_error_(std::move(other.last_error_))
{}

VarjoWorld& VarjoWorld::operator=(VarjoWorld&& other) noexcept
{
    if (this != &other) {
        if (world_) {
            varjo_WorldDestroy(world_);
        }
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        world_ = std::exchange(other.world_, nullptr);
        flags_ = std::exchange(other.flags_, 0);
        last_error_ = std::move(other.last_error_);
    }
    return *this;
}

bool VarjoWorld::valid() const
{
    return world_ != nullptr;
}

varjo_Session* VarjoWorld::session() const
{
    return session_;
}

varjo_World* VarjoWorld::get() const
{
    return world_;
}

varjo_WorldFlags VarjoWorld::flags() const
{
    return flags_;
}

void VarjoWorld::sync()
{
    if (world_) {
        varjo_WorldSync(world_);
        last_error_.clear();
    } else {
        setLastError("world is null");
    }
}

int64_t VarjoWorld::objectCount(varjo_WorldComponentTypeMask typeMask) const
{
    if (!world_) {
        setLastError("world is null");
        return 0;
    }
    return varjo_WorldGetObjectCount(world_, typeMask);
}

std::vector<varjo_WorldObject> VarjoWorld::objects(varjo_WorldComponentTypeMask typeMask) const
{
    std::vector<varjo_WorldObject> out;
    const int64_t count = objectCount(typeMask);
    if (count <= 0) {
        return out;
    }
    out.resize(static_cast<size_t>(count));
    const int64_t written = varjo_WorldGetObjects(world_, out.data(), count, typeMask);
    if (written < count && written >= 0) {
        out.resize(static_cast<size_t>(written));
    }
    return out;
}

bool VarjoWorld::getPoseComponent(varjo_WorldObjectId id, varjo_WorldPoseComponent& out, varjo_Nanoseconds displayTime) const
{
    if (!world_) {
        setLastError("world is null");
        return false;
    }
    const bool ok = (varjo_WorldGetPoseComponent(world_, id, &out, displayTime) == varjo_True);
    if (!ok) {
        setLastError("failed to get world pose component");
    } else {
        last_error_.clear();
    }
    return ok;
}

bool VarjoWorld::getObjectMarkerComponent(varjo_WorldObjectId id, varjo_WorldObjectMarkerComponent& out) const
{
    if (!world_) {
        setLastError("world is null");
        return false;
    }
    const bool ok = (varjo_WorldGetObjectMarkerComponent(world_, id, &out) == varjo_True);
    if (!ok) {
        setLastError("failed to get world marker component");
    } else {
        last_error_.clear();
    }
    return ok;
}

void VarjoWorld::setObjectMarkerTimeouts(const std::vector<varjo_WorldMarkerId>& ids, varjo_Nanoseconds duration)
{
    if (!world_ || ids.empty()) {
        return;
    }
    auto mutableIds = ids;
    varjo_WorldSetObjectMarkerTimeouts(world_, mutableIds.data(), static_cast<int64_t>(mutableIds.size()), duration);
}

void VarjoWorld::setObjectMarkerFlags(const std::vector<varjo_WorldMarkerId>& ids, varjo_WorldObjectMarkerFlags flags)
{
    if (!world_ || ids.empty()) {
        return;
    }
    auto mutableIds = ids;
    varjo_WorldSetObjectMarkerFlags(world_, mutableIds.data(), static_cast<int64_t>(mutableIds.size()), flags);
}

const std::string& VarjoWorld::lastError() const
{
    return last_error_;
}

bool VarjoWorld::hasComponent(const varjo_WorldObject& object, varjo_WorldComponentTypeMask mask)
{
    return (object.typeMask & mask) != 0;
}

std::string VarjoWorld::markerErrorToString(varjo_WorldObjectMarkerError error)
{
    switch (error) {
    case varjo_WorldObjectMarkerError_None: return "None";
    case varjo_WorldObjectMarkerError_DuplicateID: return "DuplicateID";
    default: return "Unknown(" + std::to_string(error) + ")";
    }
}

void VarjoWorld::setLastError(std::string message) const
{
    last_error_ = std::move(message);
}
