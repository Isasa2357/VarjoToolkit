#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <VarjoToolkit/Services/VST/VarjoVSTService.hpp>

#include <VarjoToolkit/DataStream/VarjoDataStreamBufferLock.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>
#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

std::string wideToUtf8ForLog(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int input_size = static_cast<int>(value.size());
    const int output_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        input_size,
        nullptr,
        0,
        nullptr,
        nullptr);

    if (output_size <= 0) {
        return std::string(value.begin(), value.end());
    }

    std::string output(static_cast<size_t>(output_size), '\0');
    const int converted = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        input_size,
        output.data(),
        output_size,
        nullptr,
        nullptr);

    if (converted <= 0) {
        return std::string(value.begin(), value.end());
    }
    return output;
}

std::string pathForLog(const std::filesystem::path& path)
{
    return wideToUtf8ForLog(path.wstring());
}

int64_t channelBits(varjo_ChannelFlag flags)
{
    return static_cast<int64_t>(flags);
}

int64_t channelIndexValue(varjo_ChannelIndex index)
{
    return static_cast<int64_t>(index);
}

bool shouldLogCounter(uint64_t value)
{
    return value <= 3 || ((value & (value - 1)) == 0);
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

varjo_ChannelFlag cameraBothEyeChannels()
{
    return static_cast<varjo_ChannelFlag>(
        static_cast<int64_t>(varjo_ChannelFlag_Left) |
        static_cast<int64_t>(varjo_ChannelFlag_Right));
}

} // namespace

VarjoVSTService::VarjoVSTService(
    const std::shared_ptr<varjo_Session>& session,
    const std::wstring& output_directory,
    const std::wstring& base_filename,
    size_t queue_capacity)
    : session_(session)
    , data_stream_(session)
    , timestamp_mapping_(session)
    , output_directory_(output_directory)
    , base_filename_(base_filename)
    , frame_queue_(queue_capacity)
{
    left_video_path_ = output_directory_ / (base_filename_ + L"_vst_left.mp4");
    right_video_path_ = output_directory_ / (base_filename_ + L"_vst_right.mp4");
    left_metadata_path_ = output_directory_ / (base_filename_ + L"_vst_left_metadata.csv");
    right_metadata_path_ = output_directory_ / (base_filename_ + L"_vst_right_metadata.csv");

    VTK_SD_LOG("VarjoVSTService constructor session=" << session_.get()
        << " outputDir=" << pathForLog(output_directory_)
        << " queueCapacity=" << queue_capacity
        << " leftVideo=" << pathForLog(left_video_path_)
        << " rightVideo=" << pathForLog(right_video_path_));
}

VarjoVSTService::~VarjoVSTService()
{
    VTK_SD_LOG("VarjoVSTService destructor running=" << (isRunning() ? "true" : "false"));
    stop();
}

bool VarjoVSTService::start()
{
    VTK_SD_LOG("VarjoVSTService::start requested outputDir=" << pathForLog(output_directory_));
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

    frame_queue_.clear();

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
    VTK_SD_LOG("VarjoVSTService writer thread started");

    if (!data_stream_.start(stream_config_, channel_flags_, [this](const varjo_StreamFrame* frame, varjo_Session* callback_session) {
            this->onFrameReceived(frame, callback_session);
        })) {
        const std::string stream_error = data_stream_.lastError();
        setLastError(L"failed to start VST data stream");
        if (!stream_error.empty()) {
            VTK_SD_ERROR("VST data_stream_.start details=" << stream_error);
        }
        stop_requested_.store(true);
        frame_queue_.notifyAll();
        if (writer_thread_.joinable()) {
            writer_thread_.join();
        }
        closeOutputs();
        std::lock_guard<std::mutex> lock(state_mutex_);
        running_ = false;
        return false;
    }

    VTK_SD_LOG("VarjoVSTService started streamId=" << data_stream_.streamId()
        << " size=" << stream_config_.width << "x" << stream_config_.height
        << " fps=" << stream_config_.frameRate
        << " channels=" << channelBits(channel_flags_));
    return true;
}

void VarjoVSTService::stop()
{
    const bool was_running = isRunning();
    VTK_SD_LOG("VarjoVSTService::stop running=" << (was_running ? "true" : "false"));

    data_stream_.stop();

    stop_requested_.store(true);
    frame_queue_.notifyAll();

    if (writer_thread_.joinable()) {
        VTK_SD_LOG("joining VarjoVSTService writer thread");
        writer_thread_.join();
    }

    closeOutputs();

    uint64_t left_count = 0;
    uint64_t right_count = 0;
    uint64_t dropped_count = 0;
    uint64_t write_failure_count = 0;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        running_ = false;
        left_count = left_frame_count_;
        right_count = right_frame_count_;
        dropped_count = dropped_frame_count_;
        write_failure_count = write_failure_count_;
    }

    if (was_running || left_count > 0 || right_count > 0 || dropped_count > 0 || write_failure_count > 0) {
        VTK_SD_LOG("VarjoVSTService stopped leftFrames=" << left_count
            << " rightFrames=" << right_count
            << " dropped=" << dropped_count
            << " writeFailures=" << write_failure_count);
    }
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
    VTK_SD_LOG("selecting VST stream config CPU NV12 DistortedColor left+right");

    VarjoDataStream::ConfigRequest request{};
    request.streamType = varjo_StreamType_DistortedColor;
    request.format = varjo_TextureFormat_NV12;
    request.bufferType = varjo_BufferType_CPU;
    request.requiredChannels = cameraBothEyeChannels();
    request.requireAllRequiredChannels = true;

    auto best = data_stream_.findBestConfig(request);
    if (!best.has_value()) {
        setLastError(L"No CPU NV12 DistortedColor VST stream with both left and right channels was found");
        return false;
    }

    stream_config_ = best.value();
    channel_flags_ = cameraBothEyeChannels();
    VTK_SD_LOG("selected VST stream config size=" << stream_config_.width << "x" << stream_config_.height
        << " fps=" << stream_config_.frameRate
        << " channels=" << channelBits(channel_flags_));
    return true;
}

bool VarjoVSTService::openOutputs()
{
    VTK_SD_LOG("opening VST outputs outputDir=" << pathForLog(output_directory_));

    std::error_code ec;
    std::filesystem::create_directories(output_directory_, ec);
    if (ec) {
        VTK_SD_WARN("create_directories failed path=" << pathForLog(output_directory_) << " message=" << ec.message());
    }

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
    VTK_SD_LOG("opened VST left metadata CSV path=" << pathForLog(left_metadata_path_));

    right_metadata_csv_.open(right_metadata_path_, std::ios::out | std::ios::trunc);
    if (!right_metadata_csv_.is_open()) {
        setLastError(L"failed to open VST right metadata CSV: " + right_metadata_path_.wstring());
        return false;
    }
    VTK_SD_LOG("opened VST right metadata CSV path=" << pathForLog(right_metadata_path_));

    writeMetadataHeader(left_metadata_csv_);
    writeMetadataHeader(right_metadata_csv_);
    VTK_SD_LOG("VST outputs opened");
    return true;
}

void VarjoVSTService::closeOutputs()
{
    const bool had_outputs = left_video_pipe_ || right_video_pipe_ || left_metadata_csv_.is_open() || right_metadata_csv_.is_open();
    if (had_outputs) {
        VTK_SD_LOG("closing VST outputs leftVideo=" << pathForLog(left_video_path_)
            << " rightVideo=" << pathForLog(right_video_path_)
            << " leftCsvOpen=" << (left_metadata_csv_.is_open() ? "true" : "false")
            << " rightCsvOpen=" << (right_metadata_csv_.is_open() ? "true" : "false"));
    }

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

    VTK_SD_LOG("opening VST ffmpeg pipe path=" << pathForLog(path)
        << " size=" << stream_config_.width << "x" << stream_config_.height
        << " fps=" << stream_config_.frameRate);

    pipe = _wpopen(cmd.str().c_str(), L"wb");
    if (pipe) {
        VTK_SD_LOG("opened VST ffmpeg pipe path=" << pathForLog(path));
    }
    return pipe != nullptr;
}

void VarjoVSTService::closeVideoPipe(FILE*& pipe)
{
    if (pipe) {
        VTK_SD_LOG("closing VST ffmpeg pipe");
        fflush(pipe);
        _pclose(pipe);
        pipe = nullptr;
    }
}

void VarjoVSTService::onFrameReceived(const varjo_StreamFrame* frame, varjo_Session* callback_session)
{
    if (!frame || !callback_session || stop_requested_.load()) {
        VTK_SD_TRACE("VST frame callback skipped frame=" << frame << " callbackSession=" << callback_session);
        return;
    }
    if (frame->type != varjo_StreamType_DistortedColor) {
        VTK_SD_TRACE("VST frame callback skipped unexpected stream type=" << static_cast<int64_t>(frame->type));
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
        VTK_SD_TRACE("VST capture skipped because buffer flag is missing frameNumber=" << frame.frameNumber
            << " channel=" << channelIndexValue(channel_index));
        return;
    }

    const varjo_BufferId buffer_id = varjo_GetBufferId(callback_session, frame.id, frame.frameNumber, channel_index);
    if (buffer_id == varjo_InvalidId) {
        VTK_SD_TRACE("VST capture skipped because varjo_GetBufferId returned invalid id frameNumber=" << frame.frameNumber
            << " channel=" << channelIndexValue(channel_index));
        return;
    }

    CapturedFrame captured{};
    captured.system_time = std::chrono::system_clock::now();
    captured.system_unix_us = VarjoTimestampMapping::systemTimeToUnixUs(captured.system_time);
    captured.stream_frame = frame;
    captured.channel_index = channel_index;

    if ((frame.dataFlags & varjo_DataFlag_Intrinsics) != 0) {
        captured.intrinsics = varjo_GetCameraIntrinsics2(callback_session, frame.id, frame.frameNumber, channel_index);
    }
    if ((frame.dataFlags & varjo_DataFlag_Extrinsics) != 0) {
        captured.extrinsics = varjo_GetCameraExtrinsics(callback_session, frame.id, frame.frameNumber, channel_index);
    }

    VarjoDataStreamBufferLock buffer_lock(callback_session, buffer_id);
    if (!buffer_lock) {
        VTK_SD_TRACE("VST capture skipped because buffer lock failed bufferId=" << buffer_id
            << " frameNumber=" << frame.frameNumber
            << " channel=" << channelIndexValue(channel_index));
        return;
    }

    captured.buffer_metadata = buffer_lock.metadata();
    if (captured.buffer_metadata.type == varjo_BufferType_CPU && captured.buffer_metadata.byteSize > 0) {
        const auto* src = static_cast<const uint8_t*>(buffer_lock.cpuData());
        if (src) {
            captured.nv12_with_padding.resize(static_cast<size_t>(captured.buffer_metadata.byteSize));
            std::memcpy(captured.nv12_with_padding.data(), src, captured.nv12_with_padding.size());
        }
    }

    if (!captured.nv12_with_padding.empty()) {
        pushCapturedFrame(std::move(captured));
    } else {
        VTK_SD_TRACE("VST capture skipped because CPU buffer data is empty frameNumber=" << frame.frameNumber
            << " channel=" << channelIndexValue(channel_index)
            << " bufferType=" << static_cast<int64_t>(captured.buffer_metadata.type)
            << " byteSize=" << captured.buffer_metadata.byteSize);
    }
}

void VarjoVSTService::pushCapturedFrame(CapturedFrame&& frame)
{
    const size_t dropped = frame_queue_.push(std::move(frame));
    if (dropped > 0) {
        uint64_t total_dropped = 0;
        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            dropped_frame_count_ += dropped;
            total_dropped = dropped_frame_count_;
        }
        if (shouldLogCounter(total_dropped)) {
            VTK_SD_WARN("VST frame queue dropped frames droppedNow=" << dropped << " totalDropped=" << total_dropped);
        }
    }
}

void VarjoVSTService::writerMain()
{
    VTK_SD_LOG("VarjoVSTService writer started");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    std::deque<CapturedFrame> local_queue;
    while (!stop_requested_.load()) {
        frame_queue_.waitSwap(local_queue, [&]() {
            return stop_requested_.load();
        });

        while (!local_queue.empty()) {
            writeFrame(local_queue.front());
            local_queue.pop_front();
        }
    }

    frame_queue_.drain(local_queue);
    while (!local_queue.empty()) {
        writeFrame(local_queue.front());
        local_queue.pop_front();
    }

    VTK_SD_LOG("VarjoVSTService writer stopped leftFrames=" << leftFrameCount()
        << " rightFrames=" << rightFrameCount()
        << " dropped=" << droppedFrameCount()
        << " writeFailures=" << writeFailureCount());
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
        uint64_t total_write_failures = 0;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ++write_failure_count_;
            total_write_failures = write_failure_count_;
        }
        if (shouldLogCounter(total_write_failures)) {
            VTK_SD_WARN("VST write frame failed side=" << (is_left ? "left" : "right")
                << " frameNumber=" << frame.stream_frame.frameNumber
                << " rowIndex=" << row_index
                << " totalWriteFailures=" << total_write_failures);
        }
    }

    if (metadata_csv.is_open()) {
        writeMetadataRow(metadata_csv, frame, row_index);
    } else {
        VTK_SD_TRACE("VST metadata row skipped because CSV is not open side=" << (is_left ? "left" : "right"));
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
        VTK_SD_TRACE("writeNv12WithoutPadding skipped pipe=" << pipe
            << " bufferBytes=" << frame.nv12_with_padding.size());
        return false;
    }

    const int width = frame.buffer_metadata.width;
    const int height = frame.buffer_metadata.height;
    const int row_stride = frame.buffer_metadata.rowStride;
    if (width <= 0 || height <= 0 || row_stride < width) {
        VTK_SD_TRACE("writeNv12WithoutPadding invalid metadata width=" << width
            << " height=" << height
            << " rowStride=" << row_stride);
        return false;
    }

    const uint8_t* src = frame.nv12_with_padding.data();
    const size_t src_size = frame.nv12_with_padding.size();
    const size_t y_plane_size = static_cast<size_t>(row_stride) * static_cast<size_t>(height);
    const size_t uv_plane_size = static_cast<size_t>(row_stride) * static_cast<size_t>(height / 2);
    if (src_size < y_plane_size + uv_plane_size) {
        VTK_SD_TRACE("writeNv12WithoutPadding source too small srcSize=" << src_size
            << " required=" << (y_plane_size + uv_plane_size));
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
        << VarjoTimestampMapping::formatUtcIso8601(frame.system_time) << ','
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
        << dc.wbNormalizationData.whiteBalanceColorGains[2] << ','
        << VarjoToolkit::Csv::toCsv(dc.wbNormalizationData.invCCM) << ','
        << VarjoToolkit::Csv::toCsv(dc.wbNormalizationData.ccm) << ','
        << VarjoToolkit::Csv::toCsv(sf.hmdPose) << ','
        << VarjoToolkit::Csv::toCsv(frame.extrinsics) << ','
        << VarjoToolkit::Csv::toCsv(frame.intrinsics) << ','
        << VarjoToolkit::Csv::toCsv(frame.buffer_metadata)
        << '\n';
}

int64_t VarjoVSTService::convertVarjoTimeToUnixUs(varjo_Nanoseconds timestamp) const
{
    int64_t unix_us = 0;
    timestamp_mapping_.convertVarjoTimestampToUnixUs(timestamp, unix_us);
    return unix_us;
}

void VarjoVSTService::setLastError(const std::wstring& message)
{
    VTK_SD_ERROR("VarjoVSTService error: " << wideToUtf8ForLog(message));
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_ = message;
}
