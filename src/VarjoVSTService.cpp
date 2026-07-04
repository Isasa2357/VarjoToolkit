#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <VarjoServices/VST/VarjoVSTService.hpp>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

int64_t systemTimeUnixUsFromTimePoint(std::chrono::system_clock::time_point tp)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        tp.time_since_epoch()).count();
}

std::wstring quoteCommandArgument(const std::filesystem::path& path)
{
    std::wstring value = path.wstring();
    std::wstring out;
    out.reserve(value.size() + 2);
    out.push_back(L'"');
    for (wchar_t ch : value) {
        if (ch == L'"') {
            out.push_back(L'\\');
        }
        out.push_back(ch);
    }
    out.push_back(L'"');
    return out;
}

std::string formatSystemClockUtcIso8601(std::chrono::system_clock::time_point tp)
{
    const auto us_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch());
    const auto sec_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(us_since_epoch);
    const int micros = static_cast<int>((us_since_epoch - sec_since_epoch).count());

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
        if (i != 0) os << ',';
        os << matrix.value[i];
    }
}

} // namespace

VarjoVSTService::VarjoVSTService(
    const std::shared_ptr<varjo_Session>& session,
    const std::wstring& output_directory,
    const std::wstring& base_filename,
    size_t queue_capacity)
    : session_(session)
    , output_directory_(output_directory)
    , base_filename_(base_filename)
    , queue_capacity_((queue_capacity > 0) ? queue_capacity : 1)
{
    left_video_path_ = output_directory_ / (base_filename_ + L"_vst_left.mp4");
    right_video_path_ = output_directory_ / (base_filename_ + L"_vst_right.mp4");
    left_metadata_path_ = output_directory_ / (base_filename_ + L"_vst_left_metadata.csv");
    right_metadata_path_ = output_directory_ / (base_filename_ + L"_vst_right_metadata.csv");
}

VarjoVSTService::~VarjoVSTService()
{
    stop();
}

bool VarjoVSTService::start()
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
    writer_thread_ = std::thread(&VarjoVSTService::writerMain, this);
    SetThreadPriority(static_cast<HANDLE>(writer_thread_.native_handle()), THREAD_PRIORITY_BELOW_NORMAL);

    varjo_StartDataStream(
        session_.get(),
        stream_id_,
        channel_flags_,
        &VarjoVSTService::onFrameReceivedStatic,
        this);

    stream_started_.store(true);
    return true;
}

void VarjoVSTService::stop()
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

bool VarjoVSTService::isRunning() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return running_;
}

std::wstring VarjoVSTService::lastError() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return last_error_;
}

VarjoVSTService::Paths VarjoVSTService::paths() const
{
    Paths p{};
    p.left_video = left_video_path_.wstring();
    p.right_video = right_video_path_.wstring();
    p.left_metadata_csv = left_metadata_path_.wstring();
    p.right_metadata_csv = right_metadata_path_.wstring();
    return p;
}

uint64_t VarjoVSTService::leftFrameCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return left_frame_count_;
}

uint64_t VarjoVSTService::rightFrameCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return right_frame_count_;
}

uint64_t VarjoVSTService::droppedFrameCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return dropped_frame_count_;
}

uint64_t VarjoVSTService::writeFailureCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return write_failure_count_;
}

bool VarjoVSTService::selectStreamConfig()
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
        if (cfg.streamType != varjo_StreamType_DistortedColor) continue;
        if (cfg.format != varjo_TextureFormat_NV12) continue;
        if (cfg.bufferType != varjo_BufferType_CPU) continue;
        if ((cfg.channelFlags & varjo_ChannelFlag_Left) == 0) continue;
        if ((cfg.channelFlags & varjo_ChannelFlag_Right) == 0) continue;

        const int64_t score =
            static_cast<int64_t>(cfg.frameRate) * 1000000000LL +
            static_cast<int64_t>(cfg.width) * static_cast<int64_t>(cfg.height);
        if (!found || score > best_score) {
            found = true;
            best_score = score;
            best = cfg;
        }
    }

    if (!found) {
        setLastError(L"No CPU NV12 DistortedColor VST stream with both left and right channels was found");
        return false;
    }

    stream_config_ = best;
    stream_id_ = best.streamId;
    channel_flags_ = best.channelFlags & (varjo_ChannelFlag_Left | varjo_ChannelFlag_Right);
    return true;
}

bool VarjoVSTService::openOutputs()
{
    std::error_code ec;
    std::filesystem::create_directories(output_directory_, ec);

    if (!openVideoPipe(left_video_pipe_, left_video_path_)) {
        setLastError(L"failed to open VST left video ffmpeg pipe: " + left_video_path_.wstring());
        return false;
    }
    if (!openVideoPipe(right_video_pipe_, right_video_path_)) {
        setLastError(L"failed to open VST right video ffmpeg pipe: " + right_video_path_.wstring());
        return false;
    }

    left_metadata_csv_.open(left_metadata_path_, std::ios::out | std::ios::trunc);
    if (!left_metadata_csv_.is_open()) {
        setLastError(L"failed to open VST left metadata CSV: " + left_metadata_path_.wstring());
        return false;
    }
    right_metadata_csv_.open(right_metadata_path_, std::ios::out | std::ios::trunc);
    if (!right_metadata_csv_.is_open()) {
        setLastError(L"failed to open VST right metadata CSV: " + right_metadata_path_.wstring());
        return false;
    }

    writeMetadataHeader(left_metadata_csv_);
    writeMetadataHeader(right_metadata_csv_);
    return true;
}

void VarjoVSTService::closeOutputs()
{
    closeVideoPipe(left_video_pipe_);
    closeVideoPipe(right_video_pipe_);

    if (left_metadata_csv_.is_open()) {
        left_metadata_csv_.flush();
        left_metadata_csv_.close();
    }
    if (right_metadata_csv_.is_open()) {
        right_metadata_csv_.flush();
        right_metadata_csv_.close();
    }
}

bool VarjoVSTService::openVideoPipe(FILE*& pipe, const std::filesystem::path& path)
{
    closeVideoPipe(pipe);

    std::wostringstream cmd;
    cmd << L"ffmpeg -y -loglevel error "
        << L"-f rawvideo -pix_fmt nv12 "
        << L"-s " << stream_config_.width << L"x" << stream_config_.height << L" "
        << L"-r " << stream_config_.frameRate << L" "
        << L"-i pipe:0 "
        << L"-c:v libx264 -preset veryfast -crf 18 -g 1 -pix_fmt yuv420p -an "
        << quoteCommandArgument(path);

    pipe = _wpopen(cmd.str().c_str(), L"wb");
    return pipe != nullptr;
}

void VarjoVSTService::closeVideoPipe(FILE*& pipe)
{
    if (pipe) {
        fflush(pipe);
        _pclose(pipe);
        pipe = nullptr;
    }
}

void VarjoVSTService::onFrameReceivedStatic(
    const varjo_StreamFrame* frame,
    varjo_Session* session,
    void* user_data)
{
    auto* self = static_cast<VarjoVSTService*>(user_data);
    if (!self) return;
    self->onFrameReceived(frame, session);
}

void VarjoVSTService::onFrameReceived(const varjo_StreamFrame* frame, varjo_Session* callback_session)
{
    if (!frame || !callback_session || stop_requested_.load()) {
        return;
    }
    if (frame->type != varjo_StreamType_DistortedColor) {
        return;
    }

    if ((frame->channels & varjo_ChannelFlag_Left) != 0) {
        captureChannel(*frame, callback_session, varjo_ChannelIndex_Left);
    }
    if ((frame->channels & varjo_ChannelFlag_Right) != 0) {
        captureChannel(*frame, callback_session, varjo_ChannelIndex_Right);
    }
}

void VarjoVSTService::captureChannel(
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

    if ((frame.dataFlags & varjo_DataFlag_Intrinsics) != 0) {
        captured.intrinsics = varjo_GetCameraIntrinsics2(callback_session, frame.id, frame.frameNumber, channel_index);
    }
    if ((frame.dataFlags & varjo_DataFlag_Extrinsics) != 0) {
        captured.extrinsics = varjo_GetCameraExtrinsics(callback_session, frame.id, frame.frameNumber, channel_index);
    }

    varjo_LockDataStreamBuffer(callback_session, buffer_id);
    captured.buffer_metadata = varjo_GetBufferMetadata(callback_session, buffer_id);

    if (captured.buffer_metadata.type == varjo_BufferType_CPU && captured.buffer_metadata.byteSize > 0) {
        const auto* src = static_cast<const uint8_t*>(varjo_GetBufferCPUData(callback_session, buffer_id));
        if (src) {
            captured.nv12_with_padding.resize(static_cast<size_t>(captured.buffer_metadata.byteSize));
            std::memcpy(captured.nv12_with_padding.data(), src, captured.nv12_with_padding.size());
        }
    }

    varjo_UnlockDataStreamBuffer(callback_session, buffer_id);

    if (!captured.nv12_with_padding.empty()) {
        pushCapturedFrame(std::move(captured));
    }
}

void VarjoVSTService::pushCapturedFrame(CapturedFrame&& frame)
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

void VarjoVSTService::writerMain()
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

void VarjoVSTService::writeFrame(const CapturedFrame& frame)
{
    const bool is_left = (frame.channel_index == varjo_ChannelIndex_Left);
    FILE* pipe = is_left ? left_video_pipe_ : right_video_pipe_;
    std::ofstream& metadata_csv = is_left ? left_metadata_csv_ : right_metadata_csv_;

    uint64_t row_index = 0;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        row_index = is_left ? left_frame_count_ : right_frame_count_;
    }

    if (!writeNv12WithoutPadding(pipe, frame)) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ++write_failure_count_;
    }

    if (metadata_csv.is_open()) {
        writeMetadataRow(metadata_csv, frame, row_index);
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

bool VarjoVSTService::writeNv12WithoutPadding(FILE* pipe, const CapturedFrame& frame)
{
    if (!pipe || frame.nv12_with_padding.empty()) {
        return false;
    }

    const int width = frame.buffer_metadata.width;
    const int height = frame.buffer_metadata.height;
    const int row_stride = frame.buffer_metadata.rowStride;
    if (width <= 0 || height <= 0 || row_stride < width) {
        return false;
    }

    const uint8_t* src = frame.nv12_with_padding.data();
    const size_t src_size = frame.nv12_with_padding.size();
    const size_t y_plane_size = static_cast<size_t>(row_stride) * static_cast<size_t>(height);
    const size_t uv_plane_size = static_cast<size_t>(row_stride) * static_cast<size_t>(height / 2);
    if (src_size < y_plane_size + uv_plane_size) {
        return false;
    }

    for (int y = 0; y < height; ++y) {
        const size_t written = fwrite(src + static_cast<size_t>(y) * row_stride, 1, static_cast<size_t>(width), pipe);
        if (written != static_cast<size_t>(width)) return false;
    }

    const uint8_t* uv = src + y_plane_size;
    for (int y = 0; y < height / 2; ++y) {
        const size_t written = fwrite(uv + static_cast<size_t>(y) * row_stride, 1, static_cast<size_t>(width), pipe);
        if (written != static_cast<size_t>(width)) return false;
    }

    return true;
}

void VarjoVSTService::writeMetadataHeader(std::ofstream& ofs)
{
    ofs
        << "row_index,"
        << "system_timestamp_unix_us,"
        << "system_timestamp_utc_iso8601,"
        << "stream_id,stream_type,frame_number,channels,data_flags,channel_index,"
        << "distorted_color_timestamp,distorted_color_timestamp_unix_us,"
        << "ev,exposure_time,white_balance_temperature,camera_calibration_constant,"
        << "wb_gain_r,wb_gain_g,wb_gain_b";

    for (int i = 0; i < 9; ++i) ofs << ",inv_ccm_m" << i;
    for (int i = 0; i < 9; ++i) ofs << ",ccm_m" << i;
    for (int i = 0; i < 16; ++i) ofs << ",hmd_pose_m" << i;
    for (int i = 0; i < 16; ++i) ofs << ",extrinsics_m" << i;

    ofs
        << ",intrinsics_model,"
        << "intrinsics_principal_point_x,intrinsics_principal_point_y,"
        << "intrinsics_focal_length_x,intrinsics_focal_length_y";
    for (int i = 0; i < 8; ++i) ofs << ",intrinsics_distortion_coeff_" << i;

    ofs
        << ",buffer_format,buffer_type,buffer_byte_size,buffer_row_stride,buffer_width,buffer_height\n";
}

void VarjoVSTService::writeMetadataRow(std::ofstream& ofs, const CapturedFrame& frame, uint64_t row_index)
{
    const auto& sf = frame.stream_frame;
    const auto& dc = sf.metadata.distortedColor;

    ofs
        << row_index << ','
        << frame.system_unix_us << ','
        << formatSystemClockUtcIso8601(frame.system_time) << ','
        << sf.id << ','
        << sf.type << ','
        << sf.frameNumber << ','
        << sf.channels << ','
        << sf.dataFlags << ','
        << frame.channel_index << ','
        << dc.timestamp << ','
        << convertVarjoTimeToUnixUs(dc.timestamp) << ','
        << dc.ev << ','
        << dc.exposureTime << ','
        << dc.whiteBalanceTemperature << ','
        << dc.cameraCalibrationConstant << ','
        << dc.wbNormalizationData.whiteBalanceColorGains[0] << ','
        << dc.wbNormalizationData.whiteBalanceColorGains[1] << ','
        << dc.wbNormalizationData.whiteBalanceColorGains[2];

    for (int i = 0; i < 9; ++i) ofs << ',' << dc.wbNormalizationData.invCCM.value[i];
    for (int i = 0; i < 9; ++i) ofs << ',' << dc.wbNormalizationData.ccm.value[i];
    ofs << ',';
    writeMatrixCsv(ofs, sf.hmdPose);
    ofs << ',';
    writeMatrixCsv(ofs, frame.extrinsics);

    ofs
        << ',' << frame.intrinsics.model
        << ',' << frame.intrinsics.principalPointX
        << ',' << frame.intrinsics.principalPointY
        << ',' << frame.intrinsics.focalLengthX
        << ',' << frame.intrinsics.focalLengthY;
    for (int i = 0; i < 8; ++i) ofs << ',' << frame.intrinsics.distortionCoefficients[i];

    ofs
        << ',' << frame.buffer_metadata.format
        << ',' << frame.buffer_metadata.type
        << ',' << frame.buffer_metadata.byteSize
        << ',' << frame.buffer_metadata.rowStride
        << ',' << frame.buffer_metadata.width
        << ',' << frame.buffer_metadata.height
        << '\n';
}

int64_t VarjoVSTService::convertVarjoTimeToUnixUs(varjo_Nanoseconds timestamp) const
{
    if (!session_ || timestamp <= 0) {
        return 0;
    }
    const varjo_Nanoseconds unix_ns = varjo_ConvertToUnixTime(session_.get(), timestamp);
    return static_cast<int64_t>(unix_ns / 1000);
}

void VarjoVSTService::setLastError(const std::wstring& message)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_ = message;
}
