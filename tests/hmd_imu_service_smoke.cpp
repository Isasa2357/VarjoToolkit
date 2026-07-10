#include "hmd_service_test_common.hpp"

#include <VarjoToolkit/Services/IMU/VarjoIMUService.hpp>

namespace {

void runImuServiceSmokeTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("imu");
    const auto csvPath = outputDirectory / "imu_smoke.csv";
    VarjoIMUService service(session.shared(), csvPath.wstring(), 180);

    if (!service.start(false)) {
        throw std::runtime_error(
            "VarjoIMUService::start failed: "
            + vtk_hmd_service_test::wideToUtf8(service.lastError()));
    }

    vtk_hmd_service_test::StopGuard<VarjoIMUService> stopGuard(service);

    const double sampleRate = vtk_hmd_service_test::waitForPositiveRate(
        "IMU/head pose",
        [&]() { return service.getSamplesPerSecond(); },
        [&]() { return service.rowCount(); });

    const auto latest = service.latestData();
    const auto buffered = service.requestBufferedData();

    std::cout
        << "IMU final live metrics: rowCount=" << service.rowCount()
        << " samplesPerSecond=" << sampleRate
        << " bufferSize=" << service.bufferSize()
        << " frameNumber=" << latest.frame_number
        << '\n';

    VTK_HMD_TEST_REQUIRE(service.rowCount() > 0);
    VTK_HMD_TEST_REQUIRE(sampleRate > 0.0);
    VTK_HMD_TEST_REQUIRE(service.bufferSize() > 0);
    VTK_HMD_TEST_REQUIRE(!buffered.empty());
    VTK_HMD_TEST_REQUIRE(latest.valid);
    VTK_HMD_TEST_REQUIRE(latest.frame_info.valid);
    VTK_HMD_TEST_REQUIRE(latest.frame_number >= 0);
    VTK_HMD_TEST_REQUIRE(latest.frame_display_time > 0);

    stopGuard.stop();
    vtk_hmd_service_test::requireNonEmptyFile(csvPath);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest("VarjoToolkitHmdImuServiceSmokeTest", runImuServiceSmokeTest);
}
