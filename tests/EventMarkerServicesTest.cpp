#include <VarjoToolkit/Services/Event/VarjoEventService.hpp>
#include <VarjoToolkit/Services/MarkerTracking/VarjoMarkerTrackingService.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

int fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

bool expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }
    return true;
}

std::filesystem::path tempFile(const std::string& name)
{
    return std::filesystem::temp_directory_path() / name;
}

bool fileContains(const std::filesystem::path& path, const std::string& needle)
{
    std::ifstream ifs(path);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return content.find(needle) != std::string::npos;
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit event/marker services test\n";

    const auto eventPath = tempFile("varjotoolkit_event_service_test.csv");
    const auto markerPath = tempFile("varjotoolkit_marker_service_test.csv");
    std::filesystem::remove(eventPath);
    std::filesystem::remove(markerPath);

    varjo_Event event{};
    event.header.type = varjo_EventType_Button;
    event.header.timestamp = 1234;
    event.data.button.pressed = varjo_True;
    event.data.button.buttonId = 7;

    VarjoEventRecord eventRecord{};
    eventRecord.rowIndex = 3;
    eventRecord.eventTypeName = VarjoEventQueue::eventTypeToString(event.header.type);
    eventRecord.event = event;

    const auto eventHeader = VarjoEventCsvLogger::header();
    const auto eventRow = VarjoEventCsvLogger::row(eventRecord);
    if (!expect(eventHeader.find("event_type_name") != std::string::npos, "event header should include type name")) return 1;
    if (!expect(eventRow.find("Button") != std::string::npos, "event row should include readable event type")) return 1;

    VarjoEventCsvLogger eventLogger(eventPath);
    if (!eventLogger.open()) return fail("event logger should open temp file");
    eventLogger.write(eventRecord);
    eventLogger.close();
    if (!fileContains(eventPath, "Button")) return fail("event CSV file should contain event row");

    VarjoEventService nullEventService(std::shared_ptr<varjo_Session>{}, eventPath);
    if (nullEventService.start()) return fail("event service should not start with null session");
    if (nullEventService.lastError().empty()) return fail("event service null start should set lastError");
    if (nullEventService.running()) return fail("event service should not be running after failed start");
    if (!nullEventService.requestEvents().empty()) return fail("event service null queue should be empty");
    if (nullEventService.rowCount() != 0) return fail("event service null row count should be zero");

    VarjoMarkerTrackingRecord markerRecord{};
    markerRecord.rowIndex = 5;
    markerRecord.sampleTimestamp = 999;
    markerRecord.marker.object.id = 10;
    markerRecord.marker.object.typeMask = varjo_WorldComponentTypeMask_ObjectMarker | varjo_WorldComponentTypeMask_Pose;
    markerRecord.marker.hasMarker = true;
    markerRecord.marker.marker.id = 42;
    markerRecord.marker.marker.flags = 1;
    markerRecord.marker.marker.error = varjo_WorldObjectMarkerError_None;
    markerRecord.marker.marker.size.width = 0.10;
    markerRecord.marker.marker.size.height = 0.20;
    markerRecord.marker.marker.size.depth = 0.30;
    markerRecord.marker.hasPose = true;
    markerRecord.marker.pose.poseFlags = varjo_WorldPoseFlags_TrackingOk | varjo_WorldPoseFlags_HasPosition;
    markerRecord.marker.pose.timeStamp = 1000;
    markerRecord.marker.pose.confidence = 0.75;
    markerRecord.marker.pose.velocity = varjo_Vector3D{1.0, 2.0, 3.0};
    markerRecord.marker.pose.angularVelocity = varjo_Vector3D{4.0, 5.0, 6.0};
    markerRecord.marker.pose.acceleration = varjo_Vector3D{7.0, 8.0, 9.0};

    const auto markerHeader = VarjoMarkerTrackingCsvLogger::header();
    const auto markerRow = VarjoMarkerTrackingCsvLogger::row(markerRecord);
    if (!expect(markerHeader.find("has_marker_component") != std::string::npos, "marker header should include component flags")) return 1;
    if (!expect(markerHeader.find("pose.confidence") != std::string::npos, "marker header should include pose fields")) return 1;
    if (!expect(markerRow.find("0.75") != std::string::npos, "marker row should include pose confidence")) return 1;

    VarjoMarkerTrackingCsvLogger markerLogger(markerPath);
    if (!markerLogger.open()) return fail("marker logger should open temp file");
    markerLogger.write(markerRecord);
    markerLogger.close();
    if (!fileContains(markerPath, "0.75")) return fail("marker CSV file should contain marker row");

    VarjoMarkerTrackingService nullMarkerService(std::shared_ptr<varjo_Session>{}, markerPath);
    if (nullMarkerService.start()) return fail("marker service should not start with null session");
    if (nullMarkerService.lastError().empty()) return fail("marker service null start should set lastError");
    if (nullMarkerService.running()) return fail("marker service should not be running after failed start");
    if (!nullMarkerService.requestMarkers().empty()) return fail("marker service null queue should be empty");
    if (nullMarkerService.rowCount() != 0) return fail("marker service null row count should be zero");

    std::filesystem::remove(eventPath);
    std::filesystem::remove(markerPath);

    std::cout << "[PASS] VarjoToolkit event/marker services test passed\n";
    return 0;
}
