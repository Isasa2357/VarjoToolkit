#include "hmd_service_performance_common.hpp"

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

void runEyeTrackingPerformanceTest()
{
    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());

    {
        VarjoEyeTrackingProvider statusProvider(session.shared());
        const auto status = statusProvider.getStatus();
        std::cout << "Gaze status before performance test: " << gazeStatusName(status) << '\n';
        if (status != VarjoEyeTrackingProvider::Status::CALIBRATED) {
            vtk_hmd_service_test::skipOrFail(
                std::string("Eye tracking is not ready. Current status: ") + gazeStatusName(status));
        }
    }

    const int durationSeconds = vtk_hmd_performance_test::measurementSeconds();
    const int warmupDurationSeconds = vtk_hmd_performance_test::warmupSeconds();
    const size_t estimatedSamples = static_cast<size_t>(durationSeconds + warmupDurationSeconds + 10) * 250U;
    const size_t queueCapacity = std::max<size_t>(5000U, estimatedSamples);

    const auto outputDirectory = vtk_hmd_service_test::makeOutputDirectory("eye_tracking_performance");
    const auto csvPath = outputDirectory / "eye_tracking_performance.csv";

    VarjoEyeTrackingService service(
        session.shared(),
        VarjoEyeTrackingProvider::OutputFilterType::STANDARD,
        VarjoEyeTrackingProvider::OutputFrequency::MAXIMUM,
        csvPath.string(),
        queueCapacity,
        5);

    if (!service.start()) {
        throw std::runtime_error("VarjoEyeTrackingService::start failed");
    }

    vtk_hmd_service_test::StopGuard<VarjoEyeTrackingService> stopGuard(service);

    const auto summary = vtk_hmd_performance_test::measureRate(
        "Eye tracking",
        [&]() { return service.getSamplesPerSecond(); },
        [&]() { return service.receivedSampleCount(); },
        durationSeconds,
        warmupDurationSeconds);

    stopGuard.stop();

    const uint64_t received = service.receivedSampleCount();
    const uint64_t processed = service.processedSampleCount();
    const uint64_t written = service.writtenSampleCount();
    const uint64_t dropped = service.droppedSampleCount();

    VTK_HMD_TEST_REQUIRE(received > 0);
    VTK_HMD_TEST_REQUIRE(processed == received);
    VTK_HMD_TEST_REQUIRE(written == received);

    vtk_hmd_performance_test::printSummary(
        "Eye tracking",
        summary,
        received,
        processed,
        written,
        dropped,
        std::nullopt);

    vtk_hmd_service_test::requireNonEmptyFile(csvPath);
}

} // namespace

int main()
{
    return vtk_hmd_service_test::runServiceTest(
        "VarjoToolkitHmdEyeTrackingServicePerformanceTest",
        runEyeTrackingPerformanceTest);
}
