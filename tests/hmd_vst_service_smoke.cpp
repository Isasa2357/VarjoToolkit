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

    const int cycles = vtk_hmd_service_test::restartCycleCount();
    std::cout << "VST restart cycles=" << cycles << '\n';

    for (int cycle = 0; cycle < cycles; ++cycle) {
        if (!service.start()) {
            const std::string message =
                "VarjoVSTService::start failed at cycle " + std::to_string(cycle) + ": "
                + vtk_hmd_service_test::wideToUtf8(service.lastError());
            if (cycle == 0) {
                throw std::runtime_error(message);
            }
            throw std::runtime_error("VST restart failed: " + message);
        }

        vtk_hmd_service_test::StopGuard<VarjoVSTService> stopGuard(service);
        VTK_HMD_TEST_REQUIRE(service.isRunning());
        VTK_HMD_TEST_REQUIRE(service.getLeftFramesPerSecond() == 0.0);
        VTK_HMD_TEST_REQUIRE(service.getRightFramesPerSecond() == 0.0);

        const double leftRate = vtk_hmd_service_test::waitForPositiveRate(
            "VST left",
            [&]() { return service.getLeftFramesPerSecond(); },
            [&]() { return service.leftReceivedFrameCount(); });

        const double rightRate = vtk_hmd_service_test::waitForPositiveRate(
            "VST right",
            [&]() { return service.getRightFramesPerSecond(); },
            [&]() { return service.rightReceivedFrameCount(); });

        stopGuard.stop();
        VTK_HMD_TEST_REQUIRE(!service.isRunning());

        const uint64_t received = service.leftReceivedFrameCount() + service.rightReceivedFrameCount();
        const uint64_t processed = service.leftProcessedFrameCount() + service.rightProcessedFrameCount();

        std::cout
            << "VST cycle=" << cycle
            << " receivedLeft=" << service.leftReceivedFrameCount()
            << " receivedRight=" << service.rightReceivedFrameCount()
            << " processedLeft=" << service.leftProcessedFrameCount()
            << " processedRight=" << service.rightProcessedFrameCount()
            << " successfulWrites=" << service.successfulWriteCount()
            << " leftFps=" << leftRate
            << " rightFps=" << rightRate
            << " dropped=" << service.droppedFrameCount()
            << " writeFailures=" << service.writeFailureCount()
            << '\n';

        VTK_HMD_TEST_REQUIRE(service.leftReceivedFrameCount() > 0);
        VTK_HMD_TEST_REQUIRE(service.rightReceivedFrameCount() > 0);
        VTK_HMD_TEST_REQUIRE(leftRate > 0.0);
        VTK_HMD_TEST_REQUIRE(rightRate > 0.0);
        VTK_HMD_TEST_REQUIRE(service.writeFailureCount() == 0);
        VTK_HMD_TEST_REQUIRE(received == processed + service.droppedFrameCount());
        VTK_HMD_TEST_REQUIRE(service.successfulWriteCount() == processed);
    }

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
