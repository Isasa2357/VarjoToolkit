#include "hmd_service_test_common.hpp"

#include <VarjoToolkit/Services/EyeTracking/VarjoEyeTrackingService.hpp>

namespace {

const char* gazeStatusName(VarjoEyeTrackingProvider::Status status)
{
    switch (status) {
    case VarjoEyeTrackingProvider::Status::NOT_AVAILABLE:
        return "NOT_AVAILABLE";
    case VarjoEyeTrackingProvider::Status::NOT_CONNECTED:
        return "NOT_CONNECTED";
    case VarjoEyeTrackingProvider::Status::NOT_CALIBRATED:
        return "NOT_CALIBRATED";
    case VarjoEyeTrackingProvider::Status::CALIBRATING:
        return "CALIBRATING";
    case VarjoEyeTrackingProvider::Status::CALIBRATED:
        return "CALIBRATED";
    default:
        return "UNKNOWN";
    }
}

void runEyeTrackingServiceSmokeTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    {
        VarjoEyeTrackingProvider statusProvider(session.shared());
        const auto status = statusProvider.getStatus();
        std::cout << "Gaze status before service start: " << gazeStatusName(status) << '\n';
        if (status != VarjoEyeTrackingProvider::Status::CALIBRATED) {
            vtk_hmd_service_test::skipOrFail(
                std::string("Eye tracking is not ready. Current status: ") + gazeStatusName(status));
        }
    }

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("eye_tracking");
    const auto csvPath = outputDirectory / "eye_tracking_smoke.csv";

    VarjoEyeTrackingService service(
        session.shared(),
        VarjoEyeTrackingProvider::OutputFilterType::STANDARD,
        VarjoEyeTrackingProvider::OutputFrequency::MAXIMUM,
        csvPath.string(),
        2000,
        5);

    if (!service.start()) {
        throw std::runtime_error("VarjoEyeTrackingService::start failed");
    }

    vtk_hmd_service_test::StopGuard<VarjoEyeTrackingService> stopGuard(service);

    std::this_thread::sleep_for(std::chrono::milliseconds(1250));
    const auto warmupSamples = service.requestData();
    std::cout << "Eye tracking warmup samples=" << warmupSamples.size() << '\n';

    const auto measurementStart = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1250));
    const auto measuredSamples = service.requestData();
    const double elapsedSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - measurementStart).count();
    const double samplesPerSecond = static_cast<double>(measuredSamples.size()) / elapsedSeconds;

    std::cout
        << "Eye tracking measured samples=" << measuredSamples.size()
        << " elapsedSeconds=" << elapsedSeconds
        << " samplesPerSecond=" << samplesPerSecond
        << '\n';

    VTK_HMD_TEST_REQUIRE(!warmupSamples.empty() || !measuredSamples.empty());
    VTK_HMD_TEST_REQUIRE(!measuredSamples.empty());
    VTK_HMD_TEST_REQUIRE(samplesPerSecond > 0.0);

    const auto& sample = measuredSamples.back();
    VTK_HMD_TEST_REQUIRE(sample.gaze.captureTime > 0);
    VTK_HMD_TEST_REQUIRE(!sample.frameInfo.views.empty());
    VTK_HMD_TEST_REQUIRE(sample.frameInfo.displayTime > 0);

    stopGuard.stop();
    vtk_hmd_service_test::requireNonEmptyFile(csvPath);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest("VarjoToolkitHmdEyeTrackingServiceSmokeTest", runEyeTrackingServiceSmokeTest);
}
