#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <VarjoToolkit/Services/EyeCamera/VarjoEyeCameraService.hpp>

#include <VarjoToolkit/DataStream/VarjoDataStreamBufferLock.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>
#include <VarjoToolkit/Utilities/VarjoCsv.hpp>
#include <VarjoToolkit/Utilities/VarjoTimestampMapping.hpp>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <iomanip>
#include <sstream>

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
        return "<wide-to-utf8 failed>";
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
        return "<wide-to-utf8 failed>";
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

varjo_ChannelFlag cameraAnyEyeChannels()
{
    return static_cast<varjo_ChannelFlag>(
        static_cast<int64_t>(varjo_ChannelFlag_Left) |
        static_cast<int64_t>(varjo_ChannelFlag_Right));
}

} // namespace

VarjoEyeCameraService::VarjoEyeCameraService(
    const std::shared_ptr<varjo_Session>& session,
    const std::wstring& output_directory,
    const std::wstring& base_filename,
    size_t queue_capacity)
    : session_(session)
    , data_stream_(session)
    , output_directory_(output_directory)
    , base_filename_(base_filename)
    , frame_queue_(queue_capacity)
{
    left_raw_path_ = output_directory_ / (base_filename_ + L"_eye_camera_left.raw");
    right_raw_path_ = output_directory_ / (base_filename_ + L"_eye_camera_right.raw");
    left_metadata_path_ = output_directory_ / (base_filename_ + L"_eye_camera_left_metadata.csv");
    right_metadata_path_ = output_directory_ / (base_filename_ + L"_eye_camera_right_metadata.csv");

    VTK_SD_LOG("VarjoEyeCameraService constructor session=" << session_.get()
        << " outputDir=" << pathForLog(output_directory_)
        << " queueCapacity=" << queue_capacity
        << " leftRaw=" << pathForLog(left_raw_path_)
        << " rightRaw=" << pathForLog(right_raw_path_));
}

VarjoEyeCameraService::~VarjoEyeCameraService()
{
    VTK_SD_LOG("VarjoEyeCameraService destructor running=" << (isRunning() ? "true" : "false"));
    stop();
}

bool VarjoEyeCameraService::start()
{
    VTK_SD_LOG("VarjoEyeCameraService::start requested outputDir=" << pathForLog(output_directory_));
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
    writer_thread_ = std::thread(&VarjoEyeCameraService::writerMain, this);
    SetThreadPriority(static_cast<HANDLE>(writer_thread_.native_handle()), THREAD_PRIORITY_BELOW_NORMAL);
    VTK_SD_LOG("VarjoEyeCameraService writer thread started");

    if (!data_stream_.start(stream_config_, channel_flags_, [this](const varjo_StreamFrame* frame, varjo_Session* callback_session) {
            this->onFrameReceived(frame, callback_session);
        })) {
        const std::string stream_error = data_stream_.lastError();
        setLastError(L"failed to start EyeCamera data stream");
        if (!stream_error.empty()) {
            VTK_SD_ERROR("EyeCamera data_stream_.start details=" << stream_error);
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

    VTK_SD_LOG("VarjoEyeCameraService started streamId=" << data_stream_.streamId()
        << " size=" << stream_config_.width << "x" << stream_config_.height
        << " fps=" << stream_config_.frameRate
        << " channels=" << channelBits(channel_flags_));
    return true;
}

void VarjoEyeCameraService::stop()
{
    const bool was_running = isRunning();
    VTK_SD_LOG("VarjoEyeCameraService::stop running=" << (was_running ? "true" : "false"));

    data_stream_.stop();

    stop_requested_.store(true);
    frame_queue_.notifyAll();

    if (writer_thread_.joinable()) {
        VTK_SD_LOG("joining VarjoEyeCameraService writer thread");
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
        VTK_SD_LOG("VarjoEyeCameraService stopped leftFrames=" << left_count
            << " rightFrames=" << right_count
            << " dropped=" << dropped_count
            << " writeFailures=" << write_failure_count);
    }
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
    VTK_SD_LOG("selecting EyeCamera stream config CPU left/right");

    VarjoDataStream::ConfigRequest request{};
    request.streamType = varjo_StreamType_EyeCamera;
    request.bufferType = varjo_BufferType_CPU;
    request.requiredChannels = cameraAnyEyeChannels();
    request.requireAllRequiredChannels = false;

    auto best = data_stream_.findBestConfig(request);
    if (!best.has_value()) {
        setLastError(L"No CPU EyeCamera data stream with left/right channel buffer was found");
        return false;
    }

    stream_config_ = best.value();
    channel_flags_ = static_cast<varjo_ChannelFlag>(
        static_cast<int64_t>(best.value().channelFlags) & static_cast<int64_t>(cameraAnyEyeChannels()));
    VTK_SD_LOG("selected EyeCamera stream config size=" << stream_config_.width << "x" << stream_config_.height
        << " fps=" << stream_config_.frameRate
        << " channels=" << channelBits(channel_flags_));
    return true;
}

bool VarjoEyeCameraService::openOutputs()
{
    VTK_SD_LOG("opening EyeCamera outputs outputDir=" << pathForLog(output_directory_));

    std::error_code ec;
    std::filesystem::create_directories(output_directory_, ec);
    if (ec) {
        VTK_SD_WARN("create_directories failed path=" << pathForLog(output_directory_) << " message=" << ec.message());
    }

    left_raw_.open(left_raw_path_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!left_raw_.is_open()) {
        setLastError(L"failed to open EyeCamera left raw output: " + left_raw_path_.wstring());
        return false;
    }
    VTK_SD_LOG("opened EyeCamera left raw path=" << pathForLog(left_raw_path_));

    right_raw_.open(right_raw_path_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!right_raw_.is_open()) {
        setLastError(L"failed to open EyeCamera right raw output: " + right_raw_path_.wstring());
        return false;
    }
    VTK_SD_LOG("opened EyeCamera right raw path=" << pathForLog(right_raw_path_));

    left_metadata_csv_.open(left_metadata_path_, std::ios::out | std::ios::trunc);
    if (!left_metadata_csv_.is_open()) {
        setLastError(L"failed to open EyeCamera left metadata CSV: " + left_metadata_path_.wstring());
        return false;
    }
    VTK_SD_LOG("opened EyeCamera left metadata CSV path=" << pathForLog(left_metadata_path_));

    right_metadata_csv_.open(right_metadata_path_, std::ios::out | std::ios::trunc);
    if (!right_metadata_csv_.is_open()) {
        setLastError(L"failed to open EyeCamera right metadata CSV: " + right_metadata_path_.wstring());
        return false;
    }
    VTK_SD_LOG("opened EyeCamera right metadata CSV path=" << pathForLog(right_metadata_path_));

    writeMetadataHeader(left_metadata_csv_);
    writeMetadataHeader(right_metadata_csv_);
    VTK_SD_LOG("EyeCamera outputs opened");
    return true;
}

void VarjoEyeCameraService::closeOutputs()
{
    const bool had_outputs = left_raw_.is_open() || right_raw_.is_open() || left_metadata_csv_.is_open() || right_metadata_csv_.is_open();
    if (had_outputs) {
        VTK_SD_LOG("closing EyeCamera outputs leftRaw=" << pathForLog(left_raw_path_)
            << " rightRaw=" << pathForLog(right_raw_path_)
            << " leftCsvOpen=" << (left_metadata_csv_.is_open() ? "true" : "false")
            << " rightCsvOpen=" << (right_metadata_csv_.is_open() ? "true" : "false"));
    }

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

void VarjoEyeCameraService::onFrameReceived(const varjo_StreamFrame* frame, varjo_Session* callback_session)
{
    if (!frame || !callback_session || stop_requested_.load()) {
        VTK_SD_TRACE("EyeCamera frame callback skipped frame=" << frame << " callbackSession=" << callback_session);
        return;
    }
    if (frame->type != varjo_StreamType_EyeCamera) {
        VTK_SD_TRACE("EyeCamera frame callback skipped unexpected stream type=" << static_cast<int64_t>(frame->type));
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
        VTK_SD_TRACE("EyeCamera capture skipped because buffer flag is missing frameNumber=" << frame.frameNumber
            << " channel=" << channelIndexValue(channel_index));
        return;
    }

    const varjo_BufferId buffer_id = varjo_GetBufferId(callback_session, frame.id, frame.frameNumber, channel_index);
    if (buffer_id == varjo_InvalidId) {
        VTK_SD_TRACE("EyeCamera capture skipped because varjo_GetBufferId returned invalid id frameNumber=" << frame.frameNumber
            << " channel=" << channelIndexValue(channel_index));
        return;
    }

    CapturedFrame captured{};
    captured.system_time = std::chrono::system_clock::now();
    captured.system_unix_us = VarjoTimestampMapping::systemTimeToUnixUs(captured.system_time);
    captured.stream_frame = frame;
    captured.channel_index = channel_index;

    VarjoDataStreamBufferLock buffer_lock(callback_session, buffer_id);
    if (!buffer_lock) {
        VTK_SD_TRACE("EyeCamera capture skipped because buffer lock failed bufferId=" << buffer_id
            << " frameNumber=" << frame.frameNumber
            << " channel=" << channelIndexValue(channel_index));
        return;
    }

    captured.buffer_metadata = buffer_lock.metadata();
    if (captured.buffer_metadata.type == varjo_BufferType_CPU && captured.buffer_metadata.byteSize > 0) {
        const auto* src = static_cast<const uint8_t*>(buffer_lock.cpuData());
        if (src) {
            captured.raw_with_padding.resize(static_cast<size_t>(captured.buffer_metadata.byteSize));
            std::memcpy(captured.raw_with_padding.data(), src, captured.raw_with_padding.size());
        }
    }

    if (!captured.raw_with_padding.empty()) {
        pushCapturedFrame(std::move(captured));
    } else {
        VTK_SD_TRACE("EyeCamera capture skipped because CPU buffer data is empty frameNumber=" << frame.frameNumber
            << " channel=" << channelIndexValue(channel_index)
            << " bufferType=" << static_cast<int64_t>(captured.buffer_metadata.type)
            << " byteSize=" << captured.buffer_metadata.byteSize);
    }
}

void VarjoEyeCameraService::pushCapturedFrame(CapturedFrame&& frame)
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
            VTK_SD_WARN("EyeCamera frame queue dropped frames droppedNow=" << dropped << " totalDropped=" << total_dropped);
        }
    }
}

void VarjoEyeCameraService::writerMain()
{
    VTK_SD_LOG("VarjoEyeCameraService writer started");
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

    VTK_SD_LOG("VarjoEyeCameraService writer stopped leftFrames=" << leftFrameCount()
        << " rightFrames=" << rightFrameCount()
        << " dropped=" << droppedFrameCount()
        << " writeFailures=" << writeFailureCount());
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
        uint64_t total_write_failures = 0;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ++write_failure_count_;
            total_write_failures = write_failure_count_;
        }
        if (shouldLogCounter(total_write_failures)) {
            VTK_SD_WARN("EyeCamera write frame failed side=" << (is_left ? "left" : "right")
                << " frameNumber=" << frame.stream_frame.frameNumber
                << " rowIndex=" << row_index
                << " totalWriteFailures=" << total_write_failures);
        }
    }

    if (metadata_csv.is_open()) {
        writeMetadataRow(metadata_csv, frame, row_index, byte_offset, byte_size);
    } else {
        VTK_SD_TRACE("EyeCamera metadata row skipped because CSV is not open side=" << (is_left ? "left" : "right"));
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
        VTK_SD_TRACE("writeRawBuffer skipped streamOpen=" << (ofs.is_open() ? "true" : "false")
            << " bufferBytes=" << frame.raw_with_padding.size());
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
        << VarjoTimestampMapping::formatUtcIso8601(frame.system_time) << ','
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
        << VarjoToolkit::Csv::toCsv(frame.buffer_metadata) << ','
        << VarjoToolkit::Csv::toCsv(sf.hmdPose)
        << '\n';
}

int64_t VarjoEyeCameraService::convertVarjoTimeToUnixUs(varjo_Nanoseconds timestamp) const
{
    VarjoTimestampMapping mapping(session_);
    int64_t unix_us = 0;
    mapping.convertVarjoTimestampToUnixUs(timestamp, unix_us);
    return unix_us;
}

void VarjoEyeCameraService::setLastError(const std::wstring& message)
{
    VTK_SD_ERROR("VarjoEyeCameraService error: " << wideToUtf8ForLog(message));
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_ = message;
}
