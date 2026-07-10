#include "hmd_service_test_common.hpp"

#include <VarjoToolkit/Services/VST/VarjoVSTService.hpp>

namespace {

void runVstServiceSmokeTest()
{
    if (!vtk_hmd_service_test::executableOnPath(L"ffmpeg.exe")) {
        vtk_hmd_service_test::skipOrFail("ffmpeg.exe was not found on PATH; VarjoVSTService MP4 output cannot be tested");
    }

    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("vst");
    VarjoVSTService service(session.shared(), outputDirectory.wstring(), L"vst_smoke", 240);

    if (!service.start()) {
        throw std::runtime_error(
            "VarjoVSTService::start failed: "
            + vtk_hmd_service_test::wideToUtf8(service.lastError()));
    }

    vtk_hmd_service_test::StopGuard<VarjoVSTService> stopGuard(service);
    VTK_HMD_TEST_REQUIRE(service.isRunning());

    const double leftRate = vtk_hmd_service_test::waitForPositiveRate(
        "VST left",
        [&]() { return service.getLeftFramesPerSecond(); },
        [&]() { return service.leftFrameCount(); });

    const double rightRate = vtk_hmd_service_test::waitForPositiveRate(
        "VST right",
        [&]() { return service.getRightFramesPerSecond(); },
        [&]() { return service.rightFrameCount(); });

    std::cout
        << "VST final live metrics: leftCount=" << service.leftFrameCount()
        << " rightCount=" << service.rightFrameCount()
        << " leftFps=" << leftRate
        << " rightFps=" << rightRate
        << " dropped=" << service.droppedFrameCount()
        << " writeFailures=" << service.writeFailureCount()
        << '\n';

    VTK_HMD_TEST_REQUIRE(service.leftFrameCount() > 0);
    VTK_HMD_TEST_REQUIRE(service.rightFrameCount() > 0);
    VTK_HMD_TEST_REQUIRE(leftRate > 0.0);
    VTK_HMD_TEST_REQUIRE(rightRate > 0.0);
    VTK_HMD_TEST_REQUIRE(service.writeFailureCount() == 0);

    stopGuard.stop();
    VTK_HMD_TEST_REQUIRE(!service.isRunning());

    const auto paths = service.paths();
    vtk_hmd_service_test::requireNonEmptyFile(paths.left_video);
    vtk_hmd_service_test::requireNonEmptyFile(paths.right_video);
    vtk_hmd_service_test::requireNonEmptyFile(paths.left_metadata_csv);
    vtk_hmd_service_test::requireNonEmptyFile(paths.right_metadata_csv);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest("VarjoToolkitHmdVstServiceSmokeTest", runVstServiceSmokeTest);
}
