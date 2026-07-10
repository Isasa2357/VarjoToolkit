#include "hmd_service_performance_common.hpp"

#include <VarjoToolkit/Services/Cubemap/VarjoEnvironmentCubemapService.hpp>

namespace {

void runCubemapPerformanceTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("cubemap_performance");
    VarjoEnvironmentCubemapService service(session.shared(), outputDirectory.wstring(), L"cubemap_performance", 240);

    if (!service.start()) {
        vtk_hmd_service_test::skipOrFail(
            "VarjoEnvironmentCubemapService::start failed. The cubemap stream may be unavailable for this HMD/runtime: "
            + vtk_hmd_service_test::wideToUtf8(service.lastError()));
    }

    vtk_hmd_service_test::StopGuard<VarjoEnvironmentCubemapService> stopGuard(service);
    VTK_HMD_TEST_REQUIRE(service.isRunning());

    const auto summary = vtk_hmd_performance_test::measureRate(
        "Environment cubemap",
        [&]() { return service.getFramesPerSecond(); },
        [&]() { return service.receivedFrameCount(); });

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
        "Environment cubemap",
        summary,
        received,
        processed,
        written,
        dropped,
        writeFailures);

    const auto paths = service.paths();
    vtk_hmd_service_test::requireNonEmptyFile(paths.raw);
    vtk_hmd_service_test::requireNonEmptyFile(paths.metadata_csv);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest(
        "VarjoToolkitHmdCubemapServicePerformanceTest",
        runCubemapPerformanceTest);
}
