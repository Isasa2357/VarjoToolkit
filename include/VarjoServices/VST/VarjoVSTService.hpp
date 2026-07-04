#pragma once

#include <Varjo.h>
#include <Varjo_datastream.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Service-style logger for Varjo VST distorted-color camera frames.
//
// This is a standalone version of the old VSTFrameDataLogger idea. It uses the
// native Varjo DataStream API directly, copies each incoming CPU NV12 buffer in
// the Varjo callback, and writes video/metadata from a separate writer thread so
// the callback returns quickly.
class VarjoVSTService {
public:
    struct Paths {
        std::wstring left_video;
        std::wstring right_video;
        std::wstring left_metadata_csv;
        std::wstring right_metadata_csv;
    };

public:
    VarjoVSTService(
        const std::shared_ptr<varjo_Session>& session,
        const std::wstring& output_directory,
        const std::wstring& base_filename,
        size_t queue_capacity = 180);

    ~VarjoVSTService();

    VarjoVSTService(const VarjoVSTService&) = delete;
    VarjoVSTService& operator=(const VarjoVSTService&) = delete;

    bool start();
    void stop();

    bool isRunning() const;
    std::wstring lastError() const;
    Paths paths() const;

    uint64_t leftFrameCount() const;
    uint64_t rightFrameCount() const;
    uint64_t droppedFrameCount() const;
    uint64_t writeFailureCount() const;

private:
    struct CapturedFrame {
        std::chrono::system_clock::time_point system_time{};
        int64_t system_unix_us = 0;

        varjo_StreamFrame stream_frame{};
        varjo_ChannelIndex channel_index = varjo_ChannelIndex_Left;
        varjo_BufferMetadata buffer_metadata{};
        varjo_CameraIntrinsics2 intrinsics{};
        varjo_Matrix extrinsics{};

        std::vector<uint8_t> nv12_with_padding;
    };

private:
    static void onFrameReceivedStatic(
        const varjo_StreamFrame* frame,
        varjo_Session* session,
        void* user_data);

    void onFrameReceived(const varjo_StreamFrame* frame, varjo_Session* callback_session);
    void captureChannel(const varjo_StreamFrame& frame, varjo_Session* callback_session, varjo_ChannelIndex channel_index);

    bool selectStreamConfig();
    bool openOutputs();
    void closeOutputs();

    bool openVideoPipe(FILE*& pipe, const std::filesystem::path& path);
    void closeVideoPipe(FILE*& pipe);

    void writerMain();
    void writeFrame(const CapturedFrame& frame);
    void writeMetadataHeader(std::ofstream& ofs);
    void writeMetadataRow(std::ofstream& ofs, const CapturedFrame& frame, uint64_t row_index);

    bool writeNv12WithoutPadding(FILE* pipe, const CapturedFrame& frame);
    void pushCapturedFrame(CapturedFrame&& frame);

    int64_t convertVarjoTimeToUnixUs(varjo_Nanoseconds timestamp) const;
    void setLastError(const std::wstring& message);

private:
    std::shared_ptr<varjo_Session> session_;
    std::filesystem::path output_directory_;
    std::wstring base_filename_;
    size_t queue_capacity_ = 180;

    varjo_StreamConfig stream_config_{};
    varjo_StreamId stream_id_ = varjo_InvalidId;
    varjo_ChannelFlag channel_flags_ = varjo_ChannelFlag_None;

    std::filesystem::path left_video_path_;
    std::filesystem::path right_video_path_;
    std::filesystem::path left_metadata_path_;
    std::filesystem::path right_metadata_path_;

    FILE* left_video_pipe_ = nullptr;
    FILE* right_video_pipe_ = nullptr;
    std::ofstream left_metadata_csv_;
    std::ofstream right_metadata_csv_;

    std::deque<CapturedFrame> frame_queue_;
    mutable std::mutex frame_queue_mutex_;
    std::condition_variable frame_queue_cv_;

    std::thread writer_thread_;
    std::atomic_bool stop_requested_{ true };
    std::atomic_bool stream_started_{ false };

    mutable std::mutex state_mutex_;
    bool running_ = false;
    std::wstring last_error_;

    uint64_t left_frame_count_ = 0;
    uint64_t right_frame_count_ = 0;
    uint64_t dropped_frame_count_ = 0;
    uint64_t write_failure_count_ = 0;
};
