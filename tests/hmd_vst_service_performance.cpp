#include "hmd_service_performance_common.hpp"

#include <VarjoToolkit/Services/VST/VarjoVSTService.hpp>

namespace {

void runVstPerformanceTest()
{
    if (!vtk_hmd_service_test::executableOnPath(L"ffmpeg.exe")) {
        vtk_hmd_service_test::skipOrFail("ffmpeg.exe was not found on PATH; VST performance output cannot be tested");
    }

    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("vst_performance");
    VarjoVSTService service(session.shared(), outputDirectory.wstring(), L"vst_performance", 480);

    if (!service.start()) {
        throw std::runtime_error(
            "VarjoVSTService::start failed: "
            + vtk_hmd_service_test::wideToUtf8(service.lastError()));
    }

    vtk_hmd_service_test::StopGuard<VarjoVSTService> stopGuard(service);
    VTK_HMD_TEST_REQUIRE(service.isRunning());

    const auto summaries = vtk_hmd_performance_test::measureDualRates(
        "VST left",
        [&]() { return service.getLeftFramesPerSecond(); },
        [&]() { return service.leftReceivedFrameCount(); },
        "VST right",
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
        "VST left",
        summaries.first,
        service.leftReceivedFrameCount(),
        service.leftProcessedFrameCount(),
        service.leftProcessedFrameCount(),
        std::nullopt,
        std::nullopt);
    vtk_hmd_performance_test::printSummary(
        "VST right",
        summaries.second,
        service.rightReceivedFrameCount(),
        service.rightProcessedFrameCount(),
        service.rightProcessedFrameCount(),
        std::nullopt,
        std::nullopt);

    std::cout
        << "[ AGGREGATE   ] VST"
        << " received=" << received
        << " processed=" << processed
        << " written=" << written
        << " dropped=" << dropped
        << " writeFailures=" << writeFailures
        << '\n';

    const auto paths = service.paths();
    vtk_hmd_service_test::requireNonEmptyFile(paths.left_video);
    vtk_hmd_service_test::requireNonEmptyFile(paths.right_video);
    vtk_hmd_service_test::requireNonEmptyFile(paths.left_metadata_csv);
    vtk_hmd_service_test::requireNonEmptyFile(paths.right_metadata_csv);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest(
        "VarjoToolkitHmdVstServicePerformanceTest",
        runVstPerformanceTest);
}
