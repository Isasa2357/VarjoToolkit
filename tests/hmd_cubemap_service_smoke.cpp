#include "hmd_service_test_common.hpp"

#include <VarjoToolkit/Services/Cubemap/VarjoEnvironmentCubemapService.hpp>

namespace {

void runCubemapServiceSmokeTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("cubemap");
    VarjoEnvironmentCubemapService service(session.shared(), outputDirectory.wstring(), L"cubemap_smoke", 120);

    const int cycles = vtk_hmd_service_test::restartCycleCount();
    std::cout << "Cubemap restart cycles=" << cycles << '\n';

    for (int cycle = 0; cycle < cycles; ++cycle) {
        if (!service.start()) {
            const std::string message =
                "VarjoEnvironmentCubemapService::start failed at cycle " + std::to_string(cycle)
                + ". The cubemap stream may be unavailable for this HMD/runtime: "
                + vtk_hmd_service_test::wideToUtf8(service.lastError());
            if (cycle == 0) {
                vtk_hmd_service_test::skipOrFail(message);
            }
            throw std::runtime_error("Cubemap restart failed: " + message);
        }

        vtk_hmd_service_test::StopGuard<VarjoEnvironmentCubemapService> stopGuard(service);
        VTK_HMD_TEST_REQUIRE(service.isRunning());
        VTK_HMD_TEST_REQUIRE(service.getFramesPerSecond() == 0.0);

        const double frameRate = vtk_hmd_service_test::waitForPositiveRate(
            "Environment cubemap",
            [&]() { return service.getFramesPerSecond(); },
            [&]() { return service.receivedFrameCount(); });

        stopGuard.stop();
        VTK_HMD_TEST_REQUIRE(!service.isRunning());

        const uint64_t received = service.receivedFrameCount();
        const uint64_t processed = service.processedFrameCount();

        std::cout
            << "Cubemap cycle=" << cycle
            << " received=" << received
            << " processed=" << processed
            << " successfulWrites=" << service.successfulWriteCount()
            << " fps=" << frameRate
            << " dropped=" << service.droppedFrameCount()
            << " writeFailures=" << service.writeFailureCount()
            << '\n';

        VTK_HMD_TEST_REQUIRE(received > 0);
        VTK_HMD_TEST_REQUIRE(frameRate > 0.0);
        VTK_HMD_TEST_REQUIRE(service.writeFailureCount() == 0);
        VTK_HMD_TEST_REQUIRE(received == processed + service.droppedFrameCount());
        VTK_HMD_TEST_REQUIRE(service.successfulWriteCount() == processed);
    }

    const auto paths = service.paths();
    vtk_hmd_service_test::requireNonEmptyFile(paths.raw);
    vtk_hmd_service_test::requireNonEmptyFile(paths.metadata_csv);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest("VarjoToolkitHmdCubemapServiceSmokeTest", runCubemapServiceSmokeTest);
}
