#include "hmd_service_performance_common.hpp"

#include <VarjoToolkit/Services/IMU/VarjoIMUService.hpp>

namespace {

void runImuPerformanceTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("imu_performance");
    const auto csvPath = outputDirectory / "imu_performance.csv";
    VarjoIMUService service(session.shared(), csvPath.wstring(), 360);

    if (!service.start(false)) {
        throw std::runtime_error(
            "VarjoIMUService::start failed: "
            + vtk_hmd_service_test::wideToUtf8(service.lastError()));
    }

    vtk_hmd_service_test::StopGuard<VarjoIMUService> stopGuard(service);

    const auto summary = vtk_hmd_performance_test::measureRate(
        "IMU/head pose",
        [&]() { return service.getSamplesPerSecond(); },
        [&]() { return service.receivedSampleCount(); });

    const auto latest = service.latestData();
    VTK_HMD_TEST_REQUIRE(latest.valid);
    VTK_HMD_TEST_REQUIRE(latest.frame_info.valid);
    VTK_HMD_TEST_REQUIRE(latest.frame_number >= 0);
    VTK_HMD_TEST_REQUIRE(latest.frame_display_time > 0);

    stopGuard.stop();

    const uint64_t received = service.receivedSampleCount();
    const uint64_t written = service.writtenSampleCount();
    VTK_HMD_TEST_REQUIRE(received > 0);
    VTK_HMD_TEST_REQUIRE(written == received);

    vtk_hmd_performance_test::printSummary(
        "IMU/head pose",
        summary,
        received,
        received,
        written,
        std::nullopt,
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
