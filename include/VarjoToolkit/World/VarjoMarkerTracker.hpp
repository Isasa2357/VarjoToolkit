#pragma once

#include <Varjo_world.h>

#include <memory>
#include <vector>

#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/World/VarjoWorld.hpp>

class VarjoMarkerTracker {
public:
    struct Marker {
        varjo_WorldObject object{};
        varjo_WorldObjectMarkerComponent marker{};
        varjo_WorldPoseComponent pose{};
        bool hasMarker = false;
        bool hasPose = false;
    };

    explicit VarjoMarkerTracker(varjo_Session* session);
    explicit VarjoMarkerTracker(std::shared_ptr<varjo_Session> session);
    explicit VarjoMarkerTracker(const VarjoSession& session);

    bool valid() const;
    explicit operator bool() const { return valid(); }

    VarjoWorld& world();
    const VarjoWorld& world() const;

    std::vector<Marker> markers(varjo_Nanoseconds displayTime, bool syncBeforeQuery = true);
    void setTimeouts(const std::vector<varjo_WorldMarkerId>& ids, varjo_Nanoseconds duration);
    void setPredictionEnabled(const std::vector<varjo_WorldMarkerId>& ids, bool enabled);

private:
    VarjoWorld world_;
};
