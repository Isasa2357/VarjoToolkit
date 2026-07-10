#pragma once

#include <Varjo.h>
#include <Varjo_math.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Utilities/VarjoSampleRateCounter.hpp>

// External-frame-driven head-pose logger.
//
// VarjoIMUService never calls varjo_WaitSync. The rendering owner submits one
// VarjoFrameInfoSnapshot per synchronized frame. Pose processing and CSV output
// are performed on the service worker thread.
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
        varjo_Vector3D angular_velocity{};
        VarjoFrameInfoSnapshot frame_info{};

        bool valid = false;
    };

    VarjoIMUService(
        const std::shared_ptr<varjo_Session>& session,
        const std::wstring& csv_output_path,
        size_t buffer_capacity = 90);
    ~VarjoIMUService();

    VarjoIMUService(const VarjoIMUService&) = delete;
    VarjoIMUService& operator=(const VarjoIMUService&) = delete;

    bool start();
    void stop();

    // Call once for each frame synchronized by the renderer. Returns false when
    // the service is stopped or the snapshot is invalid.
    bool submitFrameInfo(const VarjoFrameInfoSnapshot& snapshot);

    size_t bufferCapacity() const { return buffer_capacity_; }
    size_t bufferSize() const;
    uint64_t rowCount() const;
    std::wstring outputPath() const;
    std::wstring lastError() const;

    uint64_t receivedSampleCount() const noexcept
    {
        return received_count_.load();
    }

    uint64_t processedSampleCount() const noexcept
    {
        return processed_count_.load();
    }

    uint64_t writtenSampleCount() const noexcept
    {
        return written_count_.load();
    }

    uint64_t droppedSampleCount() const noexcept
    {
        return dropped_count_.load();
    }

    double getSamplesPerSecond() const
    {
        return sample_rate_counter_.update(receivedSampleCount());
    }

    VarjoIMUData latestData() const;
    std::deque<VarjoIMUData> requestBufferedData() const;

private:
    struct PendingFrame {
        VarjoFrameInfoSnapshot snapshot;
        std::chrono::system_clock::time_point system_time{};
        int64_t system_unix_us = 0;
        varjo_Nanoseconds varjo_now = 0;
        int64_t varjo_now_unix_us = 0;
    };

    void workerMain();
    VarjoIMUData makeData(const PendingFrame& pending);

    bool openLogFile();
    void closeLogFile();
    void writeHeader();
    bool writeRow(const VarjoIMUData& data, uint64_t row_index);

private:
    static constexpr size_t min_buffer_capacity_ = 1;

    std::shared_ptr<varjo_Session> session_;
    std::filesystem::path csv_output_path_;

    const size_t buffer_capacity_;
    const size_t pending_capacity_;

    std::deque<PendingFrame> pending_frames_;
    mutable std::mutex pending_mutex_;
    std::condition_variable pending_cv_;

    std::deque<VarjoIMUData> imu_buffer_;
    VarjoIMUData previous_data_{};
    mutable std::mutex imu_buffer_mutex_;

    std::ofstream logfile_;
    mutable std::mutex log_mutex_;

    std::thread worker_;
    std::atomic_bool stop_requested_{true};
    mutable VarjoToolkit::SampleRateCounter sample_rate_counter_;

    mutable std::mutex state_mutex_;
    bool running_ = false;
    std::wstring last_error_;

    std::atomic<uint64_t> received_count_{0};
    std::atomic<uint64_t> processed_count_{0};
    std::atomic<uint64_t> written_count_{0};
    std::atomic<uint64_t> dropped_count_{0};
};
