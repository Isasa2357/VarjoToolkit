#include <VarjoToolkit/World/VarjoMarkerTracker.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>

#include <utility>

VarjoMarkerTracker::VarjoMarkerTracker(varjo_Session* session)
    : world_(session, varjo_WorldFlag_UseObjectMarkers)
{
    VTK_SD_LOG("VarjoMarkerTracker raw constructor session=" << session << " valid=" << (world_.valid() ? "true" : "false"));
}

VarjoMarkerTracker::VarjoMarkerTracker(std::shared_ptr<varjo_Session> session)
    : world_(std::move(session), varjo_WorldFlag_UseObjectMarkers)
{
    VTK_SD_LOG("VarjoMarkerTracker shared constructor valid=" << (world_.valid() ? "true" : "false"));
}

VarjoMarkerTracker::VarjoMarkerTracker(const VarjoSession& session)
    : world_(session, varjo_WorldFlag_UseObjectMarkers)
{
    VTK_SD_LOG("VarjoMarkerTracker VarjoSession constructor valid=" << (world_.valid() ? "true" : "false"));
}

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
    VTK_SD_SCOPE("VarjoMarkerTracker::markers");
    VTK_SD_LOG("markers displayTime=" << displayTime << " syncBeforeQuery=" << (syncBeforeQuery ? "true" : "false"));
    std::vector<Marker> out;
    if (!world_) {
        VTK_SD_WARN("markers requested with invalid world");
        return out;
    }

    if (syncBeforeQuery) {
        world_.sync();
    }

    const auto objects = world_.objects(varjo_WorldComponentTypeMask_ObjectMarker);
    VTK_SD_LOG("marker world object count=" << objects.size());
    out.reserve(objects.size());
    for (const auto& object : objects) {
        Marker marker{};
        marker.object = object;
        marker.hasMarker = world_.getObjectMarkerComponent(object.id, marker.marker);
        if (VarjoWorld::hasComponent(object, varjo_WorldComponentTypeMask_Pose)) {
            marker.hasPose = world_.getPoseComponent(object.id, marker.pose, displayTime);
        }
        VTK_SD_LOG("marker object id=" << object.id << " typeMask=" << static_cast<int64_t>(object.typeMask) << " hasMarker=" << (marker.hasMarker ? "true" : "false") << " hasPose=" << (marker.hasPose ? "true" : "false"));
        out.push_back(marker);
    }
    return out;
}

void VarjoMarkerTracker::setTimeouts(const std::vector<varjo_WorldMarkerId>& ids, varjo_Nanoseconds duration)
{
    VTK_SD_LOG("VarjoMarkerTracker::setTimeouts count=" << ids.size() << " duration=" << duration);
    world_.setObjectMarkerTimeouts(ids, duration);
}

void VarjoMarkerTracker::setPredictionEnabled(const std::vector<varjo_WorldMarkerId>& ids, bool enabled)
{
    VTK_SD_LOG("VarjoMarkerTracker::setPredictionEnabled count=" << ids.size() << " enabled=" << (enabled ? "true" : "false"));
    world_.setObjectMarkerFlags(ids, enabled ? varjo_WorldObjectMarkerFlags_DoPrediction : 0);
}
