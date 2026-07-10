#include "hmd_external_frame_sync.hpp"
#include "hmd_service_performance_common.hpp"

#include <VarjoToolkit/Services/IMU/VarjoIMUService.hpp>

namespace {

void runImuPerformanceTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    const auto outputDirectory =
        vtk_hmd_service_test::makeOutputDirectory("imu_performance");
    const auto csvPath = outputDirectory / "imu_performance.csv";
    VarjoIMUService service(session.shared(), csvPath.wstring(), 360);

    if (!service.start()) {
        throw std::runtime_error(
            "VarjoIMUService::start failed: " +
            vtk_hmd_service_test::wideToUtf8(service.lastError()));
    }

    vtk_hmd_service_test::StopGuard<VarjoIMUService> stopGuard(service);
    vtk_hmd_test::ExternalFrameInfoPump framePump(
        session,
        [&service](const VarjoFrameInfoSnapshot& snapshot) {
            static_cast<void>(service.submitFrameInfo(snapshot));
        });
    VTK_HMD_TEST_REQUIRE(framePump.start());

    const auto summary = vtk_hmd_performance_test::measureRate(
        "IMU/head pose external frames",
        [&]() { return service.getSamplesPerSecond(); },
        [&]() { return service.receivedSampleCount(); });

    framePump.stop();
    stopGuard.stop();

    const auto latest = service.latestData();
    VTK_HMD_TEST_REQUIRE(latest.valid);
    VTK_HMD_TEST_REQUIRE(latest.frame_info.valid);
    VTK_HMD_TEST_REQUIRE(latest.frame_info.centerPoseValid);
    VTK_HMD_TEST_REQUIRE(latest.frame_number >= 0);
    VTK_HMD_TEST_REQUIRE(latest.frame_display_time > 0);

    const uint64_t received = service.receivedSampleCount();
    const uint64_t processed = service.processedSampleCount();
    const uint64_t written = service.writtenSampleCount();
    const uint64_t dropped = service.droppedSampleCount();
    VTK_HMD_TEST_REQUIRE(received > 0);
    VTK_HMD_TEST_REQUIRE(processed + dropped == received);
    VTK_HMD_TEST_REQUIRE(written == processed);

    vtk_hmd_performance_test::printSummary(
        "IMU/head pose external frames",
        summary,
        received,
        processed,
        written,
        dropped,
        std::nullopt);

    vtk_hmd_service_test::requireNonEmptyFile(csvPath);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest(
        "VarjoToolkitHmdImuServicePerformanceTest",
        runImuPerformanceTest);
}
