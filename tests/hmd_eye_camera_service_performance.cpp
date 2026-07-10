#include "hmd_service_performance_common.hpp"

#include <VarjoToolkit/Services/EyeCamera/VarjoEyeCameraService.hpp>

namespace {

void runEyeCameraPerformanceTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("eye_camera_performance");
    VarjoEyeCameraService service(session.shared(), outputDirectory.wstring(), L"eye_camera_performance", 480);

    if (!service.start()) {
        vtk_hmd_service_test::skipOrFail(
            "VarjoEyeCameraService::start failed. The eye camera stream may be unavailable or access may be denied: "
            + vtk_hmd_service_test::wideToUtf8(service.lastError()));
    }

    vtk_hmd_service_test::StopGuard<VarjoEyeCameraService> stopGuard(service);
    VTK_HMD_TEST_REQUIRE(service.isRunning());

    const auto summaries = vtk_hmd_performance_test::measureDualRates(
        "Eye camera left",
        [&]() { return service.getLeftFramesPerSecond(); },
        [&]() { return service.leftReceivedFrameCount(); },
        "Eye camera right",
        [&]() { return service.getRightFramesPerSecond(); },
        [&]() { return service.rightReceivedFrameCount(); });

    stopGuard.stop();
    VTK_HMD_TEST_REQUIRE(!service.isRunning());

    const uint64_t received = service.receivedFrameCount();
    const uint64_t processed = service.processedFrameCount();
    const uint64_t written = service.writtenFrameCount();
    const uint64_t dropped = service.droppedFrameCount();
    const uint64_t writeFailures = service.writeFailureCount();

    VTK_HMD_TEST_REQUIRE(received == processed + dropped);
    VTK_HMD_TEST_REQUIRE(written + writeFailures == processed);
    VTK_HMD_TEST_REQUIRE(writeFailures == 0);

    vtk_hmd_performance_test::printSummary(
        "Eye camera left",
        summaries.first,
        service.leftReceivedFrameCount(),
        service.leftProcessedFrameCount(),
        service.leftProcessedFrameCount(),
        std::nullopt,
        std::nullopt);
    vtk_hmd_performance_test::printSummary(
        "Eye camera right",
        summaries.second,
        service.rightReceivedFrameCount(),
        service.rightProcessedFrameCount(),
        service.rightProcessedFrameCount(),
        std::nullopt,
        std::nullopt);

    std::cout
        << "[ AGGREGATE   ] Eye camera"
        << " received=" << received
        << " processed=" << processed
        << " written=" << written
        << " dropped=" << dropped
        << " writeFailures=" << writeFailures
        << '\n';

    const auto paths = service.paths();
    vtk_hmd_service_test::requireNonEmptyFile(paths.left_raw);
    vtk_hmd_service_test::requireNonEmptyFile(paths.right_raw);
    vtk_hmd_service_test::requireNonEmptyFile(paths.left_metadata_csv);
    vtk_hmd_service_test::requireNonEmptyFile(paths.right_metadata_csv);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest(
        "VarjoToolkitHmdEyeCameraServicePerformanceTest",
        runEyeCameraPerformanceTest);
}
