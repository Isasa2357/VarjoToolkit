#pragma once

#include <Varjo.h>
#include <Varjo_world.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <VarjoToolkit/World/VarjoMarkerTracker.hpp>

struct VarjoMarkerTrackingRecord {
    uint64_t rowIndex = 0;
    varjo_Nanoseconds sampleTimestamp = 0;
    VarjoMarkerTracker::Marker marker{};
};

class VarjoMarkerTrackingCsvLogger {
public:
    explicit VarjoMarkerTrackingCsvLogger(std::filesystem::path filepath);
    ~VarjoMarkerTrackingCsvLogger();

    bool open();
    void close();
    bool isOpen() const;
    void write(const VarjoMarkerTrackingRecord& record);

    const std::filesystem::path& filepath() const;
    const std::string& lastError() const;

    static std::string header();
    static std::string row(const VarjoMarkerTrackingRecord& record);
    static std::string poseHeader(const std::string& name);
    static std::string poseToCsv(const varjo_WorldPoseComponent& pose);

private:
    void setLastError(std::string message) const;

private:
    std::filesystem::path filepath_;
    std::ofstream file_;
    mutable std::string last_error_;
};

class VarjoMarkerTrackingService {
public:
    VarjoMarkerTrackingService(
        std::shared_ptr<varjo_Session> session,
        std::filesystem::path filepath,
        size_t queueSize = 1000,
        int sampleIntervalMs = 33);
    ~VarjoMarkerTrackingService();

    VarjoMarkerTrackingService(const VarjoMarkerTrackingService&) = delete;
    VarjoMarkerTrackingService& operator=(const VarjoMarkerTrackingService&) = delete;

    bool start();
    void stop();
    bool running() const;

    std::deque<VarjoMarkerTrackingRecord> requestMarkers();
    uint64_t rowCount() const;
    const std::string& lastError() const;

private:
    void worker();
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_;
    VarjoMarkerTracker marker_tracker_;
    VarjoMarkerTrackingCsvLogger logger_;

    std::deque<VarjoMarkerTrackingRecord> queue_;
    size_t queue_size_ = 1000;
    int sample_interval_ms_ = 33;
    mutable std::mutex mutex_;

    std::thread thread_;
    std::atomic_bool stop_signal_{false};
    std::atomic_bool running_{false};
    std::atomic_uint64_t row_count_{0};
    mutable std::string last_error_;
};
