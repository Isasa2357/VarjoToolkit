#pragma once

#include <Varjo.h>
#include <Varjo_world.h>

#include <memory>
#include <string>
#include <vector>

#include <VarjoToolkit/Core/VarjoSession.hpp>

class VarjoWorld {
public:
    VarjoWorld(varjo_Session* session, varjo_WorldFlags flags = varjo_WorldFlag_UseObjectMarkers);
    VarjoWorld(std::shared_ptr<varjo_Session> session, varjo_WorldFlags flags = varjo_WorldFlag_UseObjectMarkers);
    VarjoWorld(const VarjoSession& session, varjo_WorldFlags flags = varjo_WorldFlag_UseObjectMarkers);
    ~VarjoWorld();

    VarjoWorld(const VarjoWorld&) = delete;
    VarjoWorld& operator=(const VarjoWorld&) = delete;
    VarjoWorld(VarjoWorld&& other) noexcept;
    VarjoWorld& operator=(VarjoWorld&& other) noexcept;

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Session* session() const;
    varjo_World* get() const;
    varjo_WorldFlags flags() const;

    void sync();
    int64_t objectCount(varjo_WorldComponentTypeMask typeMask) const;
    std::vector<varjo_WorldObject> objects(varjo_WorldComponentTypeMask typeMask) const;

    bool getPoseComponent(varjo_WorldObjectId id, varjo_WorldPoseComponent& out, varjo_Nanoseconds displayTime) const;
    bool getObjectMarkerComponent(varjo_WorldObjectId id, varjo_WorldObjectMarkerComponent& out) const;

    void setObjectMarkerTimeouts(const std::vector<varjo_WorldMarkerId>& ids, varjo_Nanoseconds duration);
    void setObjectMarkerFlags(const std::vector<varjo_WorldMarkerId>& ids, varjo_WorldObjectMarkerFlags flags);

    const std::string& lastError() const;

    static bool hasComponent(const varjo_WorldObject& object, varjo_WorldComponentTypeMask mask);
    static std::string markerErrorToString(varjo_WorldObjectMarkerError error);

private:
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    varjo_World* world_ = nullptr;
    varjo_WorldFlags flags_ = 0;
    mutable std::string last_error_;
};
