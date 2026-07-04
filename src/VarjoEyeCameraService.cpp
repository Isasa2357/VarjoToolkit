#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <VarjoToolkit/Services/EyeCamera/VarjoEyeCameraService.hpp>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

int64_t systemTimeUnixUsFromTimePoint(std::chrono::system_clock::time_point tp)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
}

std::string formatSystemClockUtcIso8601(std::chrono::system_clock::time_point tp)
{
    const auto usSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch());
    const auto secSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(usSinceEpoch);
    const int micros = static_cast<int>((usSinceEpoch - secSinceEpoch).count());

    const std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_s(&tm, &tt);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
        << "." << std::setw(6) << std::setfill('0') << micros << "Z";
    return oss.str();
}

void writeMatrixCsv(std::ostream& os, const varjo_Matrix& matrix)
{
    for (int i = 0; i < 16; ++i) {
        if (i != 0) {
            os << ',';
        }
        os << matrix.value[i];
    }
}

int64_t streamConfigScore(const varjo_StreamConfig& cfg)
{
    int64_t score = static_cast<int64_t>(cfg.frameRate) * 1000000000LL +
        static_cast<int64_t>(cfg.width) * static_cast<int64_t>(cfg.height);
    if (cfg.format == varjo_TextureFormat_Y8_UNORM) {
        score += 1000000000000000LL;
    }
    return score;
}

} // namespace

VarjoEyeCameraService::VarjoEyeCameraService(
    const std::shared_ptr<varjo_Session>& session,
    const std::wstring& output_directory,
    const std::wstring& base_filename,
    size_t queue_capacity)
    : session_(session)
    , output_directory_(output_directory)
    , base_filename_(base_filename)
    , queue_capacity_((queue_capacity > 0) ? queue_capacity : 1)
{
    left_raw_path_ = output_directory_ / (base_filename_ + L"_eye_camera_left.raw");
    right_raw_path_ = output_directory_ / (base_filename_ + L"_eye_camera_right.raw");
    left_metadata_path_ = output_directory_ / (base_filename_ + L"_eye_camera_left_metadata.csv");
    right_metadata_path_ = output_directory_ / (base_filename_ + L"_eye_camera_right_metadata.csv");
}

VarjoEyeCameraService::~VarjoEyeCameraService()
{
    stop();
}

bool VarjoEyeCameraService::start()
{
    stop();

    if (!session_) {
        setLastError(L"session is null");
        return false;
    }

    if (!selectStreamConfig()) {
        return false;
    }

    if (!openOutputs()) {
        closeOutputs();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(frame_queue_mutex_);
        frame_queue_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        left_frame_count_ = 0;
        right_frame_count_ = 0;
        dropped_frame_count_ = 0;
        write_failure_count_ = 0;
        last_error_.clear();
        running_ = true;
    }

    stop_requested_.store(false);
    writer_thread_ = std::thread(&VarjoEyeCameraService::writerMain, this);
    SetThreadPriority(static_cast<HANDLE>(writer_thread_.native_handle()), THREAD_PRIORITY_BELOW_NORMAL);

    varjo_StartDataStream(
        session_.get(),
        stream_id_,
        channel_flags_,
        &VarjoEyeCameraService::onFrameReceivedStatic,
        this);

    stream_started_.store(true);
    return true;
}

void VarjoEyeCameraService::stop()
{
    if (stream_started_.load() && session_ && stream_id_ != varjo_InvalidId) {
        varjo_StopDataStream(session_.get(), stream_id_);
        stream_started_.store(false);
    }

    stop_requested_.store(true);
    frame_queue_cv_.notify_all();

    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }

    closeOutputs();

    std::lock_guard<std::mutex> lock(state_mutex_);
    running_ = false;
}

bool VarjoEyeCameraService::isRunning() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return running_;
}

std::wstring VarjoEyeCameraService::lastError() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return last_error_;
}

VarjoEyeCameraService::Paths VarjoEyeCameraService::paths() const
{
    Paths p{};
    p.left_raw = left_raw_path_.wstring();
    p.right_raw = right_raw_path_.wstring();
    p.left_metadata_csv = left_metadata_path_.wstring();
    p.right_metadata_csv = right_metadata_path_.wstring();
    return p;
}

uint64_t VarjoEyeCameraService::leftFrameCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return left_frame_count_;
}

uint64_t VarjoEyeCameraService::rightFrameCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return right_frame_count_;
}

uint64_t VarjoEyeCameraService::droppedFrameCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return dropped_frame_count_;
}

uint64_t VarjoEyeCameraService::writeFailureCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return write_failure_count_;
}

bool VarjoEyeCameraService::selectStreamConfig()
{
    const int32_t config_count = varjo_GetDataStreamConfigCount(session_.get());
    if (config_count <= 0) {
        setLastError(L"varjo_GetDataStreamConfigCount returned no configs");
        return false;
    }

    std::vector<varjo_StreamConfig> configs(static_cast<size_t>(config_count));
    varjo_GetDataStreamConfigs(session_.get(), configs.data(), config_count);

    bool found = false;
    int64_t best_score = -1;
    varjo_StreamConfig best{};

    for (const auto& cfg : configs) {
        if (cfg.streamType != varjo_StreamType_EyeCamera) {
            continue;
        }
        if (cfg.bufferType != varjo_BufferType_CPU) {
            continue;
        }
        if ((cfg.channelFlags & (varjo_ChannelFlag_Left | varjo_ChannelFlag_Right)) == 0) {
            continue;
        }

        const int64_t score = streamConfigScore(cfg);
        if (!found || score > best_score) {
            found = true;
            best_score = score;
            best = cfg;
        }
    }

    if (!found) {
        setLastError(L"No CPU EyeCamera data stream with left/right channel buffer was found");
        return false;
    }

    stream_config_ = best;
    stream_id_ = best.streamId;
    channel_flags_ = best.channelFlags & (varjo_ChannelFlag_Left | varjo_ChannelFlag_Right);
    return true;
}

bool VarjoEyeCameraService::openOutputs()
{
    std::error_code ec;
    std::filesystem::create_directories(output_directory_, ec);

    left_raw_.open(left_raw_path_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!left_raw_.is_open()) {
        setLastError(L"failed to open EyeCamera left raw output: " + left_raw_path_.wstring());
        return false;
    }

    right_raw_.open(right_raw_path_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!right_raw_.is_open()) {
        setLastError(L"failed to open EyeCamera right raw output: " + right_raw_path_.wstring());
        return false;
    }

    left_metadata_csv_.open(left_metadata_path_, std::ios::out | std::ios::trunc);
    if (!left_metadata_csv_.is_open()) {
        setLastError(L"failed to open EyeCamera left metadata CSV: " + left_metadata_path_.wstring());
        return false;
    }

    right_metadata_csv_.open(right_metadata_path_, std::ios::out | std::ios::trunc);
    if (!right_metadata_csv_.is_open()) {
        setLastError(L"failed to open EyeCamera right metadata CSV: " + right_metadata_path_.wstring());
        return false;
    }

    writeMetadataHeader(left_metadata_csv_);
    writeMetadataHeader(right_metadata_csv_);
    return true;
}

void VarjoEyeCameraService::closeOutputs()
{
    if (left_raw_.is_open()) {
        left_raw_.flush();
        left_raw_.close();
    }
    if (right_raw_.is_open()) {
        right_raw_.flush();
        right_raw_.close();
    }
    if (left_metadata_csv_.is_open()) {
        left_metadata_csv_.flush();
        left_metadata_csv_.close();
    }
    if (right_metadata_csv_.is_open()) {
        right_metadata_csv_.flush();
        right_metadata_csv_.close();
    }
}

void VarjoEyeCameraService::onFrameReceivedStatic(
    const varjo_StreamFrame* frame,
    varjo_Session* session,
    void* user_data)
{
    auto* self = static_cast<VarjoEyeCameraService*>(user_data);
    if (!self) {
        return;
    }
    self->onFrameReceived(frame, session);
}

void VarjoEyeCameraService::onFrameReceived(const varjo_StreamFrame* frame, varjo_Session* callback_session)
{
    if (!frame || !callback_session || stop_requested_.load()) {
        return;
    }
    if (frame->type != varjo_StreamType_EyeCamera) {
        return;
    }

    if ((frame->channels & varjo_ChannelFlag_Left) != 0) {
        captureChannel(*frame, callback_session, varjo_ChannelIndex_Left);
    }
    if ((frame->channels & varjo_ChannelFlag_Right) != 0) {
        captureChannel(*frame, callback_session, varjo_ChannelIndex_Right);
    }
}

void VarjoEyeCameraService::captureChannel(
    const varjo_StreamFrame& frame,
    varjo_Session* callback_session,
    varjo_ChannelIndex channel_index)
{
    if ((frame.dataFlags & varjo_DataFlag_Buffer) == 0) {
        return;
    }

    const varjo_BufferId buffer_id = varjo_GetBufferId(callback_session, frame.id, frame.frameNumber, channel_index);
    if (buffer_id == varjo_InvalidId) {
        return;
    }

    CapturedFrame captured{};
    captured.system_time = std::chrono::system_clock::now();
    captured.system_unix_us = systemTimeUnixUsFromTimePoint(captured.system_time);
    captured.stream_frame = frame;
    captured.channel_index = channel_index;

    varjo_LockDataStreamBuffer(callback_session, buffer_id);
    captured.buffer_metadata = varjo_GetBufferMetadata(callback_session, buffer_id);

    if (captured.buffer_metadata.type == varjo_BufferType_CPU && captured.buffer_metadata.byteSize > 0) {
        const auto* src = static_cast<const uint8_t*>(varjo_GetBufferCPUData(callback_session, buffer_id));
        if (src) {
            captured.raw_with_padding.resize(static_cast<size_t>(captured.buffer_metadata.byteSize));
            std::memcpy(captured.raw_with_padding.data(), src, captured.raw_with_padding.size());
        }
    }

    varjo_UnlockDataStreamBuffer(callback_session, buffer_id);

    if (!captured.raw_with_padding.empty()) {
        pushCapturedFrame(std::move(captured));
    }
}

void VarjoEyeCameraService::pushCapturedFrame(CapturedFrame&& frame)
{
    {
        std::lock_guard<std::mutex> lock(frame_queue_mutex_);
        frame_queue_.push_back(std::move(frame));
        while (frame_queue_.size() > queue_capacity_) {
            frame_queue_.pop_front();
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            ++dropped_frame_count_;
        }
    }
    frame_queue_cv_.notify_one();
}

void VarjoEyeCameraService::writerMain()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    std::deque<CapturedFrame> local_queue;
    while (!stop_requested_.load()) {
        {
            std::unique_lock<std::mutex> lock(frame_queue_mutex_);
            frame_queue_cv_.wait(lock, [&]() {
                return stop_requested_.load() || !frame_queue_.empty();
            });
            local_queue.swap(frame_queue_);
        }

        while (!local_queue.empty()) {
            writeFrame(local_queue.front());
            local_queue.pop_front();
        }
    }

    {
        std::lock_guard<std::mutex> lock(frame_queue_mutex_);
        local_queue.swap(frame_queue_);
    }
    while (!local_queue.empty()) {
        writeFrame(local_queue.front());
        local_queue.pop_front();
    }
}

void VarjoEyeCameraService::writeFrame(const CapturedFrame& frame)
{
    const bool is_left = (frame.channel_index == varjo_ChannelIndex_Left);
    std::ofstream& raw = is_left ? left_raw_ : right_raw_;
    std::ofstream& metadata_csv = is_left ? left_metadata_csv_ : right_metadata_csv_;

    uint64_t row_index = 0;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        row_index = is_left ? left_frame_count_ : right_frame_count_;
    }

    uint64_t byte_offset = 0;
    uint64_t byte_size = 0;
    if (!writeRawBuffer(raw, frame, byte_offset, byte_size)) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ++write_failure_count_;
    }

    if (metadata_csv.is_open()) {
        writeMetadataRow(metadata_csv, frame, row_index, byte_offset, byte_size);
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (is_left) {
            ++left_frame_count_;
        } else {
            ++right_frame_count_;
        }
    }
}

bool VarjoEyeCameraService::writeRawBuffer(std::ofstream& ofs, const CapturedFrame& frame, uint64_t& byte_offset, uint64_t& byte_size)
{
    byte_offset = 0;
    byte_size = static_cast<uint64_t>(frame.raw_with_padding.size());
    if (!ofs.is_open() || frame.raw_with_padding.empty()) {
        return false;
    }

    const auto pos = ofs.tellp();
    if (pos != std::streampos(-1)) {
        byte_offset = static_cast<uint64_t>(static_cast<std::streamoff>(pos));
    }
    ofs.write(reinterpret_cast<const char*>(frame.raw_with_padding.data()), static_cast<std::streamsize>(frame.raw_with_padding.size()));
    return ofs.good();
}

void VarjoEyeCameraService::writeMetadataHeader(std::ofstream& ofs)
{
    ofs
        << "row_index,"
        << "raw_byte_offset,raw_byte_size,"
        << "system_timestamp_unix_us,"
        << "system_timestamp_utc_iso8601,"
        << "stream_id,stream_type,frame_number,channels,data_flags,channel_index,"
        << "eye_camera_timestamp,eye_camera_timestamp_unix_us,"
        << "glint_mask_left,glint_mask_right,"
        << "buffer_format,buffer_type,buffer_byte_size,buffer_row_stride,buffer_width,buffer_height";

    for (int i = 0; i < 16; ++i) {
        ofs << ",hmd_pose_m" << i;
    }
    ofs << "\n";
}

void VarjoEyeCameraService::writeMetadataRow(
    std::ofstream& ofs,
    const CapturedFrame& frame,
    uint64_t row_index,
    uint64_t byte_offset,
    uint64_t byte_size)
{
    const auto& sf = frame.stream_frame;
    const auto& ec = sf.metadata.eyeCamera;

    ofs
        << row_index << ','
        << byte_offset << ','
        << byte_size << ','
        << frame.system_unix_us << ','
        << formatSystemClockUtcIso8601(frame.system_time) << ','
        << sf.id << ','
        << sf.type << ','
        << sf.frameNumber << ','
        << sf.channels << ','
        << sf.dataFlags << ','
        << frame.channel_index << ','
        << ec.timestamp << ','
        << convertVarjoTimeToUnixUs(ec.timestamp) << ','
        << ec.glintMaskLeft << ','
        << ec.glintMaskRight << ','
        << frame.buffer_metadata.format << ','
        << frame.buffer_metadata.type << ','
        << frame.buffer_metadata.byteSize << ','
        << frame.buffer_metadata.rowStride << ','
        << frame.buffer_metadata.width << ','
        << frame.buffer_metadata.height << ',';

    writeMatrixCsv(ofs, sf.hmdPose);
    ofs << '\n';
}

int64_t VarjoEyeCameraService::convertVarjoTimeToUnixUs(varjo_Nanoseconds timestamp) const
{
    if (!session_ || timestamp <= 0) {
        return 0;
    }
    const varjo_Nanoseconds unix_ns = varjo_ConvertToUnixTime(session_.get(), timestamp);
    return static_cast<int64_t>(unix_ns / 1000);
}

void VarjoEyeCameraService::setLastError(const std::wstring& message)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_ = message;
}
