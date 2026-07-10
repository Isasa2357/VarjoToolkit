#pragma once

#include <Varjo.h>
#include <Varjo_datastream.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <VarjoToolkit/DataStream/VarjoDataStream.hpp>
#include <VarjoToolkit/DataStream/VarjoDataStreamFrameQueue.hpp>

class VarjoEnvironmentCubemapService {
public:
    struct Paths {
        std::wstring raw;
        std::wstring metadata_csv;
    };

public:
    VarjoEnvironmentCubemapService(
        const std::shared_ptr<varjo_Session>& session,
        const std::wstring& output_directory,
        const std::wstring& base_filename,
        size_t queue_capacity = 90);

    ~VarjoEnvironmentCubemapService();

    VarjoEnvironmentCubemapService(const VarjoEnvironmentCubemapService&) = delete;
    VarjoEnvironmentCubemapService& operator=(const VarjoEnvironmentCubemapService&) = delete;

    bool start();
    void stop();

    bool isRunning() const;
    std::wstring lastError() const;
    Paths paths() const;

    // Legacy processed-frame counter. A processed frame reached the writer
    // thread; writeFailureCount() reports failed raw writes separately.
    uint64_t frameCount() const;
    uint64_t droppedFrameCount() const;
    uint64_t writeFailureCount() const;

    uint64_t receivedFrameCount() const
    {
        return frame_queue_.pushedCount();
    }

    uint64_t processedFrameCount() const { return frameCount(); }

    uint64_t successfulWriteCount() const
    {
        const uint64_t processed = frameCount();
        const uint64_t failures = writeFailureCount();
        return processed >= failures ? processed - failures : 0;
    }

    double getFramesPerSecond() const
    {
        return frame_queue_.pushedRatePerSecond();
    }

private:
    struct CapturedFrame {
        std::chrono::system_clock::time_point system_time{};
        int64_t system_unix_us = 0;

        varjo_StreamFrame stream_frame{};
        varjo_ChannelIndex channel_index = varjo_ChannelIndex_First;
        varjo_BufferMetadata buffer_metadata{};

        std::vector<uint8_t> raw_with_padding;
    };

private:
    void onFrameReceived(const varjo_StreamFrame* frame, varjo_Session* callback_session);
    void captureFrame(const varjo_StreamFrame& frame, varjo_Session* callback_session, varjo_ChannelIndex channel_index);

    bool selectStreamConfig();
    bool openOutputs();
    void closeOutputs();

    void writerMain();
    void writeFrame(const CapturedFrame& frame);
    void writeMetadataHeader(std::ofstream& ofs);
    void writeMetadataRow(std::ofstream& ofs, const CapturedFrame& frame, uint64_t row_index, uint64_t byte_offset, uint64_t byte_size);

    bool writeRawBuffer(std::ofstream& ofs, const CapturedFrame& frame, uint64_t& byte_offset, uint64_t& byte_size);
    void pushCapturedFrame(CapturedFrame&& frame);

    int64_t convertVarjoTimeToUnixUs(varjo_Nanoseconds timestamp) const;
    void setLastError(const std::wstring& message);

private:
    std::shared_ptr<varjo_Session> session_;
    VarjoDataStream data_stream_;
    std::filesystem::path output_directory_;
    std::wstring base_filename_;

    varjo_StreamConfig stream_config_{};
    varjo_ChannelFlag channel_flags_ = varjo_ChannelFlag_None;

    std::filesystem::path raw_path_;
    std::filesystem::path metadata_path_;

    std::ofstream raw_;
    std::ofstream metadata_csv_;

    VarjoDataStreamFrameQueue<CapturedFrame> frame_queue_;

    std::thread writer_thread_;
    std::atomic_bool stop_requested_{true};

    mutable std::mutex state_mutex_;
    bool running_ = false;
    std::wstring last_error_;

    uint64_t frame_count_ = 0;
    uint64_t dropped_frame_count_ = 0;
    uint64_t write_failure_count_ = 0;
};
