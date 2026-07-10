#include "hmd_service_test_common.hpp"

#include <VarjoToolkit/Services/EyeCamera/VarjoEyeCameraService.hpp>

namespace {

void runEyeCameraServiceSmokeTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("eye_camera");
    VarjoEyeCameraService service(session.shared(), outputDirectory.wstring(), L"eye_camera_smoke", 240);

    if (!service.start()) {
        vtk_hmd_service_test::skipOrFail(
            "VarjoEyeCameraService::start failed. The eye camera stream may be unavailable or access may be denied: "
            + vtk_hmd_service_test::wideToUtf8(service.lastError()));
    }

    vtk_hmd_service_test::StopGuard<VarjoEyeCameraService> stopGuard(service);
    VTK_HMD_TEST_REQUIRE(service.isRunning());

    const double leftRate = vtk_hmd_service_test::waitForPositiveRate(
        "Eye camera left",
        [&]() { return service.getLeftFramesPerSecond(); },
        [&]() { return service.leftFrameCount(); });

    const double rightRate = vtk_hmd_service_test::waitForPositiveRate(
        "Eye camera right",
        [&]() { return service.getRightFramesPerSecond(); },
        [&]() { return service.rightFrameCount(); });

    std::cout
        << "Eye camera final live metrics: leftCount=" << service.leftFrameCount()
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
    vtk_hmd_service_test::requireNonEmptyFile(paths.left_raw);
    vtk_hmd_service_test::requireNonEmptyFile(paths.right_raw);
    vtk_hmd_service_test::requireNonEmptyFile(paths.left_metadata_csv);
    vtk_hmd_service_test::requireNonEmptyFile(paths.right_metadata_csv);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest("VarjoToolkitHmdEyeCameraServiceSmokeTest", runEyeCameraServiceSmokeTest);
}
