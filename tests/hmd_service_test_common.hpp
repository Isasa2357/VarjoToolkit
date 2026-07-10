#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "hmd_test_common.hpp"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace vtk_hmd_service_test {

constexpr int kSkipReturnCode = 125;

class SkipTest final : public std::runtime_error {
public:
    explicit SkipTest(const std::string& message)
        : std::runtime_error(message)
    {}
};

inline bool optionalServicesAreRequired()
{
    const char* value = std::getenv("VARJOTOOLKIT_REQUIRE_OPTIONAL_HMD_SERVICES");
    return value != nullptr && std::string(value) == "1";
}

inline int restartCycleCount()
{
    constexpr int defaultCycles = 3;
    constexpr int maximumCycles = 20;

    const char* value = std::getenv("VARJOTOOLKIT_HMD_RESTART_CYCLES");
    if (!value || *value == '\0') {
        return defaultCycles;
    }

    try {
        const int parsed = std::stoi(value);
        return std::clamp(parsed, 1, maximumCycles);
    } catch (...) {
        return defaultCycles;
    }
}

[[noreturn]] inline void skipOrFail(const std::string& message)
{
    if (optionalServicesAreRequired()) {
        throw std::runtime_error(message);
    }
    throw SkipTest(message);
}

inline std::string wideToUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return "<wide string conversion failed>";
    }

    std::string output(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        output.data(),
        required,
        nullptr,
        nullptr);
    return output;
}

inline bool executableOnPath(const wchar_t* executable)
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = SearchPathW(nullptr, executable, nullptr, MAX_PATH, buffer, nullptr);
    return length > 0 && length < MAX_PATH;
}

inline std::filesystem::path makeOutputDirectory(const char* testName)
{
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path()
        / "VarjoToolkitHmdServiceTests"
        / (std::string(testName) + "_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(unique));

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    ec.clear();
    std::filesystem::create_directories(root, ec);
    if (ec) {
        throw std::runtime_error("Failed to create test output directory: " + root.string() + ": " + ec.message());
    }

    std::cout << "Test output directory: " << root.string() << '\n';
    return root;
}

inline void requireNonEmptyFile(const std::filesystem::path& path)
{
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec || !exists) {
        throw std::runtime_error("Expected output file does not exist: " + path.string());
    }

    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size == 0) {
        throw std::runtime_error("Expected non-empty output file: " + path.string());
    }

    std::cout << "Output file: " << path.string() << " size=" << size << " bytes\n";
}

template <typename Service>
class StopGuard {
public:
    explicit StopGuard(Service& service)
        : service_(service)
    {}

    ~StopGuard()
    {
        if (active_) {
            service_.stop();
        }
    }

    void stop()
    {
        if (active_) {
            service_.stop();
            active_ = false;
        }
    }

private:
    Service& service_;
    bool active_ = true;
};

template <typename RateGetter, typename CountGetter>
double waitForPositiveRate(
    const char* label,
    RateGetter&& rateGetter,
    CountGetter&& countGetter,
    std::chrono::seconds timeout = std::chrono::seconds(6))
{
    (void)rateGetter();
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    double rate = 0.0;
    uint64_t count = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        count = static_cast<uint64_t>(countGetter());
        rate = static_cast<double>(rateGetter());
        std::cout << label << " count=" << count << " rate=" << rate << '\n';
        if (count > 0 && rate > 0.0) {
            return rate;
        }
    }

    throw std::runtime_error(std::string(label) + " did not produce a positive sample rate before timeout");
}

template <typename Fn>
int runServiceTest(const char* testName, Fn&& fn)
{
    try {
        std::cout << "[ RUN      ] " << testName << '\n';
        fn();
        std::cout << "[       OK ] " << testName << '\n';
        return 0;
    } catch (const SkipTest& e) {
        std::cout << "[  SKIPPED ] " << testName << ": " << e.what() << '\n';
        return kSkipReturnCode;
    } catch (const std::exception& e) {
        std::cerr << "[  FAILED  ] " << testName << ": " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "[  FAILED  ] " << testName << ": unknown exception\n";
        return 1;
    }
}

} // namespace vtk_hmd_service_test
