#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "WrapperExamples.hpp"

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/Services/Cubemap/VarjoEnvironmentCubemapService.hpp>
#include <VarjoToolkit/Services/Event/VarjoEventService.hpp>
#include <VarjoToolkit/Services/EyeCamera/VarjoEyeCameraService.hpp>
#include <VarjoToolkit/Services/EyeTracking/VarjoEyeTrackingService.hpp>
#include <VarjoToolkit/Services/IMU/VarjoIMUService.hpp>
#include <VarjoToolkit/Services/MarkerTracking/VarjoMarkerTrackingService.hpp>
#include <VarjoToolkit/Services/VST/VarjoVSTService.hpp>

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {

std::atomic_bool g_stopRequested{false};

BOOL WINAPI consoleCtrlHandler(DWORD eventType)
{
    switch (eventType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        g_stopRequested.store(true);
        return TRUE;
    default:
        return FALSE;
    }
}

struct Options {
    std::filesystem::path outputDirectory = "varjo_service_logs";
    int seconds = 30;
    bool enableEvent = true;
    bool enableMarker = true;
    bool enableEye = true;
    bool enableImu = true;
    bool enableVst = true;
    bool enableEyeCamera = true;
    bool enableCubemap = true;
    bool help = false;
};

void printUsage()
{
    std::cout
        << "VarjoServiceLoggerSample\n\n"
        << "  --out <dir>          Output directory\n"
        << "  --seconds <n>        0 means until Ctrl+C\n"
        << "  --no-event\n"
        << "  --no-marker\n"
        << "  --no-eye\n"
        << "  --no-imu\n"
        << "  --no-vst\n"
        << "  --no-eye-camera\n"
        << "  --no-cubemap\n"
        << "  --help\n\n"
        << "This application owns varjo_WaitSync. EyeTracking and IMU receive\n"
        << "the resulting VarjoFrameInfoSnapshot and never call WaitSync.\n";
}

bool parseArguments(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            options.help = true;
            return true;
        }
        if (argument == "--out") {
            if (++index >= argc) return false;
            options.outputDirectory = argv[index];
        } else if (argument == "--seconds") {
            if (++index >= argc) return false;
            try {
                options.seconds = std::stoi(argv[index]);
            } catch (...) {
                return false;
            }
            if (options.seconds < 0) return false;
        } else if (argument == "--no-event") {
            options.enableEvent = false;
        } else if (argument == "--no-marker") {
            options.enableMarker = false;
        } else if (argument == "--no-eye") {
            options.enableEye = false;
        } else if (argument == "--no-imu") {
            options.enableImu = false;
        } else if (argument == "--no-vst") {
            options.enableVst = false;
        } else if (argument == "--no-eye-camera") {
            options.enableEyeCamera = false;
        } else if (argument == "--no-cubemap") {
            options.enableCubemap = false;
        } else {
            std::cerr << "Unknown option: " << argument << '\n';
            return false;
        }
    }
    return true;
}

int64_t elapsedSeconds(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start).count();
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseArguments(argc, argv, options)) {
        printUsage();
        return 1;
    }
    if (options.help) {
        printUsage();
        return 0;
    }

    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    std::error_code error;
    std::filesystem::create_directories(options.outputDirectory, error);
    if (error) {
        std::cerr << "Failed to create output directory: "
                  << error.message() << '\n';
        return 1;
    }

    VarjoSession session;
    if (!session.valid()) {
        std::cerr << "Varjo session initialization failed: "
                  << session.lastError() << '\n';
        return 1;
    }
    printWrapperUsageExamples(session.shared());

    VarjoFrameInfo frameInfo(session);
    if (!frameInfo.valid()) {
        std::cerr << "Failed to allocate VarjoFrameInfo.\n";
        return 1;
    }

    const auto eventCsv = options.outputDirectory / "events.csv";
    const auto markerCsv = options.outputDirectory / "markers.csv";
    const auto eyeCsv = options.outputDirectory / "eye_tracking.csv";
    const auto imuCsv = options.outputDirectory / "imu.csv";

    std::unique_ptr<VarjoEventService> eventService;
    std::unique_ptr<VarjoMarkerTrackingService> markerService;
    std::unique_ptr<VarjoEyeTrackingService> eyeService;
    std::unique_ptr<VarjoIMUService> imuService;
    std::unique_ptr<VarjoVSTService> vstService;
    std::unique_ptr<VarjoEyeCameraService> eyeCameraService;
    std::unique_ptr<VarjoEnvironmentCubemapService> cubemapService;

    if (options.enableEvent) {
        eventService = std::make_unique<VarjoEventService>(
            session.shared(), eventCsv, 1000, 10, 64);
        if (!eventService->start()) eventService.reset();
    }
    if (options.enableMarker) {
        markerService = std::make_unique<VarjoMarkerTrackingService>(
            session.shared(), markerCsv, 1000, 33);
        if (!markerService->start()) markerService.reset();
    }
    if (options.enableEye) {
        eyeService = std::make_unique<VarjoEyeTrackingService>(
            session.shared(),
            VarjoEyeTrackingProvider::OutputFilterType::STANDARD,
            VarjoEyeTrackingProvider::OutputFrequency::MAXIMUM,
            eyeCsv.string(),
            2000,
            5);
        if (!eyeService->start()) eyeService.reset();
    }
    if (options.enableImu) {
        imuService = std::make_unique<VarjoIMUService>(
            session.shared(), imuCsv.wstring(), 512);
        if (!imuService->start()) imuService.reset();
    }
    if (options.enableVst) {
        vstService = std::make_unique<VarjoVSTService>(
            session.shared(), options.outputDirectory.wstring(), L"sample", 180);
        if (!vstService->start()) vstService.reset();
    }
    if (options.enableEyeCamera) {
        eyeCameraService = std::make_unique<VarjoEyeCameraService>(
            session.shared(), options.outputDirectory.wstring(), L"sample", 180);
        if (!eyeCameraService->start()) eyeCameraService.reset();
    }
    if (options.enableCubemap) {
        cubemapService = std::make_unique<VarjoEnvironmentCubemapService>(
            session.shared(), options.outputDirectory.wstring(), L"sample", 90);
        if (!cubemapService->start()) cubemapService.reset();
    }

    if (!eventService && !markerService && !eyeService && !imuService &&
        !vstService && !eyeCameraService && !cubemapService) {
        std::cerr << "No service could be started.\n";
        return 2;
    }

    std::cout << "Logging started. Press Ctrl+C to stop.\n";
    const auto start = std::chrono::steady_clock::now();
    int64_t lastPrintedSecond = -1;
    uint64_t synchronizedFrames = 0;

    while (!g_stopRequested.load()) {
        if (options.seconds > 0 &&
            elapsedSeconds(start) >= options.seconds) {
            break;
        }

        // The sample application, not VarjoToolkit services, owns synchronization.
        varjo_WaitSync(session.get(), frameInfo.get());
        const auto snapshot = frameInfo.snapshot();
        if (snapshot.valid) {
            ++synchronizedFrames;
            if (eyeService) {
                static_cast<void>(eyeService->submitFrameInfo(snapshot));
                static_cast<void>(eyeService->requestData());
            }
            if (imuService) {
                static_cast<void>(imuService->submitFrameInfo(snapshot));
            }
        }

        if (eventService) static_cast<void>(eventService->requestEvents());
        if (markerService) static_cast<void>(markerService->requestMarkers());

        const int64_t second = elapsedSeconds(start);
        if (second != lastPrintedSecond) {
            lastPrintedSecond = second;
            std::cout << "elapsed=" << second
                      << " synchronizedFrames=" << synchronizedFrames;
            if (eyeService) {
                std::cout << " gaze=" << eyeService->receivedSampleCount()
                          << " frameInfo=" << eyeService->submittedFrameInfoCount();
            }
            if (imuService) {
                std::cout << " imu=" << imuService->writtenSampleCount()
                          << " imuDropped=" << imuService->droppedSampleCount();
            }
            if (vstService) {
                std::cout << " vstL=" << vstService->leftFrameCount()
                          << " vstR=" << vstService->rightFrameCount();
            }
            std::cout << '\n';
        }
    }

    if (cubemapService) cubemapService->stop();
    if (eyeCameraService) eyeCameraService->stop();
    if (vstService) vstService->stop();
    if (imuService) imuService->stop();
    if (eyeService) eyeService->stop();
    if (markerService) markerService->stop();
    if (eventService) eventService->stop();

    std::cout << "Done. synchronizedFrames=" << synchronizedFrames << '\n';
    return 0;
}
