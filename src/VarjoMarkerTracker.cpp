#include <VarjoToolkit/World/VarjoMarkerTracker.hpp>

VarjoMarkerTracker::VarjoMarkerTracker(varjo_Session* session)
    : world_(session, varjo_WorldFlag_UseObjectMarkers)
{}

VarjoMarkerTracker::VarjoMarkerTracker(std::shared_ptr<varjo_Session> session)
    : world_(std::move(session), varjo_WorldFlag_UseObjectMarkers)
{}

VarjoMarkerTracker::VarjoMarkerTracker(const VarjoSession& session)
    : world_(session, varjo_WorldFlag_UseObjectMarkers)
{}

bool VarjoMarkerTracker::valid() const
{
    return world_.valid();
}

VarjoWorld& VarjoMarkerTracker::world()
{
    return world_;
}

const VarjoWorld& VarjoMarkerTracker::world() const
{
    return world_;
}

std::vector<VarjoMarkerTracker::Marker> VarjoMarkerTracker::markers(varjo_Nanoseconds displayTime, bool syncBeforeQuery)
{
    std::vector<Marker> out;
    if (!world_) {
        return out;
    }

    if (syncBeforeQuery) {
        world_.sync();
    }

    const auto objects = world_.objects(varjo_WorldComponentTypeMask_ObjectMarker);
    out.reserve(objects.size());
    for (const auto& object : objects) {
        Marker marker{};
        marker.object = object;
        marker.hasMarker = world_.getObjectMarkerComponent(object.id, marker.marker);
        if (VarjoWorld::hasComponent(object, varjo_WorldComponentTypeMask_Pose)) {
            marker.hasPose = world_.getPoseComponent(object.id, marker.pose, displayTime);
        }
        out.push_back(marker);
    }
    return out;
}

void VarjoMarkerTracker::setTimeouts(const std::vector<varjo_WorldMarkerId>& ids, varjo_Nanoseconds duration)
{
    world_.setObjectMarkerTimeouts(ids, duration);
}

void VarjoMarkerTracker::setPredictionEnabled(const std::vector<varjo_WorldMarkerId>& ids, bool enabled)
{
    world_.setObjectMarkerFlags(ids, enabled ? varjo_WorldObjectMarkerFlags_DoPrediction : 0);
}
