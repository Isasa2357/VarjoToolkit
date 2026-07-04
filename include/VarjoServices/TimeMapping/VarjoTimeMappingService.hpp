#pragma once

#include <Varjo.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

struct VarjoTimeMappingSample {
    varjo_Nanoseconds varjo_timestamp = 0;
    varjo_Nanoseconds varjo_timestamp_unix_ns = 0;
    int64_t varjo_timestamp_unix_us = 0;
    int64_t system_timestamp_unix_us = 0;
    std::chrono::system_clock::time_point system_timestamp{};
};

// Periodically records the relationship between Varjo time and real-world time.
//
// Varjo timestamps such as gaze.captureTime are in the Varjo monotonic time
// domain. varjo_ConvertToUnixTime() maps those timestamps to Unix time. This
// service writes that mapping to CSV so camera/video metadata, rendering metadata,
// and eye tracking logs can be aligned offline.
//
// Unlike the other services, this service is allowed to have no external data
// acquisition target. It records a time mapping rather than sampling a sensor
// stream.
class VarjoTimeMappingService {
public:
    VarjoTimeMappingService(const std::shared_ptr<varjo_Session>& session, int interval_ms = 5);
    ~VarjoTimeMappingService();

    VarjoTimeMappingService(const VarjoTimeMappingService&) = delete;
    VarjoTimeMappingService& operator=(const VarjoTimeMappingService&) = delete;

    bool start(const std::wstring& csv_output_path);
    void stop();

    // Converts an arbitrary Varjo timestamp to Unix microseconds using
    // varjo_ConvertToUnixTime(). Returns false if session/timestamp is invalid.
    bool convertVarjoTimestampToUnixUs(varjo_Nanoseconds varjo_timestamp, int64_t& out_unix_us) const;

    bool isRunning() const;
    uint64_t rowCount() const;
    std::wstring outputPath() const;

private:
    void workerMain();
    bool openFile();
    void closeFile();
    void writeHeader();
    void writeRow(const VarjoTimeMappingSample& sample);
    VarjoTimeMappingSample sampleOnce() const;

private:
    std::shared_ptr<varjo_Session> session_;
    int interval_ms_ = 5;

    std::filesystem::path output_path_;
    std::ofstream csv_file_;

    std::thread worker_;
    std::atomic_bool stop_requested_{ true };

    mutable std::mutex state_mutex_;
    bool running_ = false;
    uint64_t row_count_ = 0;
};
