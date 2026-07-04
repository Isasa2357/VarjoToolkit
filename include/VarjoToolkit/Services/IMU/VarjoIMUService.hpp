#pragma once

#include <Varjo.h>
#include <Varjo_math.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

// Service-style IMU/head-pose logger.
//
// This service samples Varjo frame/head-pose data, keeps a bounded in-memory
// buffer, and writes each sample to CSV.
class VarjoIMUService {
public:
    struct VarjoIMUData {
        std::chrono::system_clock::time_point system_time{};
        int64_t system_unix_us = 0;

        varjo_Nanoseconds varjo_now = 0;
        int64_t varjo_now_unix_us = 0;

        int64_t frame_number = -1;
        varjo_Nanoseconds frame_display_time = 0;
        int64_t frame_display_time_unix_us = 0;

        varjo_Matrix pose{};
        varjo_Vector3D position{};
        varjo_Vector3D euler_deg{};
        varjo_Vector3D angular_velocity{}; // deg/s, computed from euler_deg delta
        varjo_FrameInfo frame_info{};

        bool valid = false;
    };

public:
    VarjoIMUService(
        const std::shared_ptr<varjo_Session>& session,
        const std::wstring& csv_output_path,
        size_t buffer_capacity = 90);

    ~VarjoIMUService();

    VarjoIMUService(const VarjoIMUService&) = delete;
    VarjoIMUService& operator=(const VarjoIMUService&) = delete;

    bool start(bool waitFillBuffer = false);
    void stop();

    size_t bufferCapacity() const { return buffer_capacity_; }
    size_t bufferSize() const;
    uint64_t rowCount() const;
    std::wstring outputPath() const;
    std::wstring lastError() const;

    VarjoIMUData latestData() const;
    std::deque<VarjoIMUData> requestBufferedData() const;

private:
    void workerMain();

    bool openLogFile();
    void closeLogFile();
    void writeHeader();
    void writeRow(const VarjoIMUData& data);

    VarjoIMUData sampleOnce();

private:
    static constexpr size_t min_buffer_capacity_ = 1;

    std::shared_ptr<varjo_Session> session_;
    std::filesystem::path csv_output_path_;

    const size_t buffer_capacity_;
    std::deque<VarjoIMUData> imu_buffer_;
    VarjoIMUData previous_data_{};
    mutable std::mutex imu_buffer_mutex_;

    std::ofstream logfile_;
    mutable std::mutex log_mutex_;

    std::thread worker_;
    std::atomic_bool stop_requested_{ true };

    mutable std::mutex state_mutex_;
    bool running_ = false;
    uint64_t row_count_ = 0;
    std::wstring last_error_;
};
