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

    const int cycles = vtk_hmd_service_test::restartCycleCount();
    std::cout << "Eye tracking restart cycles=" << cycles << '\n';

    for (int cycle = 0; cycle < cycles; ++cycle) {
        if (!service.start()) {
            throw std::runtime_error("VarjoEyeTrackingService::start failed at cycle " + std::to_string(cycle));
        }

        vtk_hmd_service_test::StopGuard<VarjoEyeTrackingService> stopGuard(service);
        VTK_HMD_TEST_REQUIRE(service.getSamplesPerSecond() == 0.0);

        const double sampleRate = vtk_hmd_service_test::waitForPositiveRate(
            "Eye tracking",
            [&]() { return service.getSamplesPerSecond(); },
            [&]() { return service.receivedSampleCount(); });

        const uint64_t receivedBeforeRequest = service.receivedSampleCount();
        auto samples = service.requestData();
        const uint64_t receivedAfterRequest = service.receivedSampleCount();
        const double rateAfterRequest = service.getSamplesPerSecond();

        if (samples.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            samples = service.requestData();
        }

        stopGuard.stop();

        std::cout
            << "Eye tracking cycle=" << cycle
            << " received=" << service.receivedSampleCount()
            << " samplesReturned=" << samples.size()
            << " samplesPerSecond=" << sampleRate
            << " rateAfterRequest=" << rateAfterRequest
            << '\n';

        VTK_HMD_TEST_REQUIRE(receivedBeforeRequest > 0);
        VTK_HMD_TEST_REQUIRE(receivedAfterRequest >= receivedBeforeRequest);
        VTK_HMD_TEST_REQUIRE(service.receivedSampleCount() >= receivedAfterRequest);
        VTK_HMD_TEST_REQUIRE(sampleRate > 0.0);
        VTK_HMD_TEST_REQUIRE(rateAfterRequest > 0.0);
        VTK_HMD_TEST_REQUIRE(!samples.empty());

        const auto& sample = samples.back();
        VTK_HMD_TEST_REQUIRE(sample.gaze.captureTime > 0);
        VTK_HMD_TEST_REQUIRE(!sample.frameInfo.views.empty());
        VTK_HMD_TEST_REQUIRE(sample.frameInfo.displayTime > 0);
    }

    vtk_hmd_service_test::requireNonEmptyFile(csvPath);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest("VarjoToolkitHmdEyeTrackingServiceSmokeTest", runEyeTrackingServiceSmokeTest);
}
