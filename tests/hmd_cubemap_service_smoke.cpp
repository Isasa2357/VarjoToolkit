#include "hmd_service_test_common.hpp"

#include <VarjoToolkit/Services/Cubemap/VarjoEnvironmentCubemapService.hpp>

namespace {

void runCubemapServiceSmokeTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("cubemap");
    VarjoEnvironmentCubemapService service(session.shared(), outputDirectory.wstring(), L"cubemap_smoke", 120);

    if (!service.start()) {
        vtk_hmd_service_test::skipOrFail(
            "VarjoEnvironmentCubemapService::start failed. The cubemap stream may be unavailable for this HMD/runtime: "
            + vtk_hmd_service_test::wideToUtf8(service.lastError()));
    }

    vtk_hmd_service_test::StopGuard<VarjoEnvironmentCubemapService> stopGuard(service);
    VTK_HMD_TEST_REQUIRE(service.isRunning());

    const double frameRate = vtk_hmd_service_test::waitForPositiveRate(
        "Environment cubemap",
        [&]() { return service.getFramesPerSecond(); },
        [&]() { return service.frameCount(); });

    std::cout
        << "Cubemap final live metrics: frameCount=" << service.frameCount()
        << " fps=" << frameRate
        << " dropped=" << service.droppedFrameCount()
        << " writeFailures=" << service.writeFailureCount()
        << '\n';

    VTK_HMD_TEST_REQUIRE(service.frameCount() > 0);
    VTK_HMD_TEST_REQUIRE(frameRate > 0.0);
    VTK_HMD_TEST_REQUIRE(service.writeFailureCount() == 0);

    stopGuard.stop();
    VTK_HMD_TEST_REQUIRE(!service.isRunning());

    const auto paths = service.paths();
    vtk_hmd_service_test::requireNonEmptyFile(paths.raw);
    vtk_hmd_service_test::requireNonEmptyFile(paths.metadata_csv);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest("VarjoToolkitHmdCubemapServiceSmokeTest", runCubemapServiceSmokeTest);
}
