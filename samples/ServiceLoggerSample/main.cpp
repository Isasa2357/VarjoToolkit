#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <VarjoServices/EyeTracking/VarjoEyeTrackingService.hpp>
#include <VarjoServices/IMU/VarjoIMUService.hpp>
#include <VarjoServices/VST/VarjoVSTService.hpp>

#include <Varjo.h>

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

std::atomic_bool g_stopRequested{ false };

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
    bool enableEyeTracking = true;
    bool enableIMU = true;
    bool enableVST = true;
    bool help = false;
};

void printUsage()
{
    std::cout
        << "VarjoServiceLoggerSample\n"
        << "\n"
        << "Usage:\n"
        << "  VarjoServiceLoggerSample.exe [options]\n"
        << "\n"
        << "Options:\n"
        << "  --out <dir>          Output directory. Default: varjo_service_logs\n"
        << "  --seconds <n>        Logging duration in seconds. 0 means run until Ctrl+C. Default: 30\n"
        << "  --no-eye            Disable VarjoEyeTrackingService\n"
        << "  --no-imu            Disable VarjoIMUService\n"
        << "  --no-vst            Disable VarjoVSTService\n"
        << "  --help              Show this message\n"
        << "\n"
        << "Notes:\n"
        << "  VST logging writes MP4 through ffmpeg. Put ffmpeg.exe in PATH, or use --no-vst.\n"
        << "  Eye tracking must be allowed and calibrated in Varjo Base.\n";
}

bool parseInt(const char* text, int& value)
{
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(text, &consumed, 10);
        if (consumed == std::string(text).size()) {
            value = parsed;
            return true;
        }
    } catch (...) {
    }
    return false;
}

bool parseArguments(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            options.help = true;
            return true;
        }
        if (arg == "--out") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --out\n";
                return false;
            }
            options.outputDirectory = argv[++i];
            continue;
        }
        if (arg == "--seconds") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --seconds\n";
                return false;
            }
            int seconds = 0;
            if (!parseInt(argv[++i], seconds) || seconds < 0) {
                std::cerr << "Invalid --seconds value\n";
                return false;
            }
            options.seconds = seconds;
            continue;
        }
        if (arg == "--no-eye") {
            options.enableEyeTracking = false;
            continue;
        }
        if (arg == "--no-imu") {
            options.enableIMU = false;
            continue;
        }
        if (arg == "--no-vst") {
            options.enableVST = false;
            continue;
        }

        std::cerr << "Unknown option: " << arg << "\n";
        return false;
    }
    return true;
}

int64_t elapsedSeconds(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start).count();
}

void printPath(const char* label, const std::filesystem::path& path)
{
    std::wcout << L"  " << label << L": " << path.wstring() << L"\n";
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

    std::error_code ec;
    std::filesystem::create_directories(options.outputDirectory, ec);
    if (ec) {
        std::wcerr << L"Failed to create output directory: " << options.outputDirectory.wstring() << L"\n";
        return 1;
    }

    std::cout << "Initializing Varjo session...\n";
    if (varjo_IsAvailable() == varjo_False) {
        std::cerr << "Varjo runtime is not available. Start Varjo Base and connect the HMD.\n";
        return 1;
    }

    std::shared_ptr<varjo_Session> session(
        varjo_SessionInit(),
        [] (varjo_Session* s) {
            if (s) {
                varjo_SessionShutDown(s);
            }
        });

    if (!session) {
        std::cerr << "varjo_SessionInit failed.\n";
        return 1;
    }

    const auto eyeCsv = options.outputDirectory / "eye_tracking.csv";
    const auto imuCsv = options.outputDirectory / "imu.csv";

    std::unique_ptr<VarjoEyeTrackingService> eyeTrackingService;
    std::unique_ptr<VarjoIMUService> imuService;
    std::unique_ptr<VarjoVSTService> vstService;

    bool eyeTrackingStarted = false;
    bool imuStarted = false;
    bool vstStarted = false;

    if (options.enableEyeTracking) {
        eyeTrackingService = std::make_unique<VarjoEyeTrackingService>(
            session,
            VarjoEyeTrackingProvider::OutputFilterType::STANDARD,
            VarjoEyeTrackingProvider::OutputFrequency::MAXIMUM,
            eyeCsv.string(),
            1000,
            5);
        eyeTrackingStarted = eyeTrackingService->start();
        if (!eyeTrackingStarted) {
            std::wcerr << L"VarjoEyeTrackingService failed to start: " << eyeCsv.wstring() << L"\n";
        }
    }

    if (options.enableIMU) {
        imuService = std::make_unique<VarjoIMUService>(session, imuCsv.wstring(), 90);
        imuStarted = imuService->start(false);
        if (!imuStarted) {
            std::wcerr << L"VarjoIMUService failed to start: " << imuService->lastError() << L"\n";
        }
    }

    if (options.enableVST) {
        vstService = std::make_unique<VarjoVSTService>(session, options.outputDirectory.wstring(), L"sample", 180);
        vstStarted = vstService->start();
        if (!vstStarted) {
            std::wcerr << L"VarjoVSTService failed to start: " << vstService->lastError() << L"\n";
        }
    }

    if (!eyeTrackingStarted && !imuStarted && !vstStarted) {
        std::cerr << "No service could be started.\n";
        return 2;
    }

    std::cout << "Logging started. Press Ctrl+C to stop.\n";
    if (eyeTrackingStarted) {
        printPath("eye tracking CSV", eyeCsv);
    }
    if (imuStarted) {
        printPath("IMU CSV", imuCsv);
    }

    const auto loopStart = std::chrono::steady_clock::now();
    int64_t lastPrintedSecond = -1;
    uint64_t totalEyeSamplesRead = 0;

    while (!g_stopRequested.load()) {
        if (options.seconds > 0 && elapsedSeconds(loopStart) >= options.seconds) {
            break;
        }

        if (eyeTrackingStarted && eyeTrackingService) {
            auto eyeSamples = eyeTrackingService->requestData();
            totalEyeSamplesRead += static_cast<uint64_t>(eyeSamples.size());
        }

        const int64_t sec = elapsedSeconds(loopStart);
        if (sec != lastPrintedSecond) {
            lastPrintedSecond = sec;
            std::cout << "elapsed=" << sec << "s";
            if (eyeTrackingStarted) {
                std::cout << " eyeSamplesRead=" << totalEyeSamplesRead;
            }
            if (imuStarted && imuService) {
                const auto imu = imuService->latestData();
                std::cout << " imuRows=" << imuService->rowCount();
                if (imu.valid) {
                    std::cout << " imuPos=(" << imu.position.x << "," << imu.position.y << "," << imu.position.z << ")";
                }
            }
            if (vstStarted && vstService) {
                std::cout << " vstL=" << vstService->leftFrameCount()
                          << " vstR=" << vstService->rightFrameCount()
                          << " vstDrop=" << vstService->droppedFrameCount();
            }
            std::cout << "\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Stopping services...\n";
    if (vstService) {
        vstService->stop();
    }
    if (imuService) {
        imuService->stop();
    }
    if (eyeTrackingService) {
        eyeTrackingService->stop();
    }

    std::cout << "Done.\n";
    if (vstService) {
        const auto paths = vstService->paths();
        std::wcout << L"VST outputs:\n"
                   << L"  left video: " << paths.left_video << L"\n"
                   << L"  right video: " << paths.right_video << L"\n"
                   << L"  left metadata: " << paths.left_metadata_csv << L"\n"
                   << L"  right metadata: " << paths.right_metadata_csv << L"\n";
    }

    return 0;
}
