#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <VarjoToolkit/Services/Cubemap/VarjoEnvironmentCubemapService.hpp>

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

} // namespace

VarjoEnvironmentCubemapService::VarjoEnvironmentCubemapService(
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
    raw_path_ = output_directory_ / (base_filename_ + L"_environment_cubemap.raw");
    metadata_path_ = output_directory_ / (base_filename_ + L"_environment_cubemap_metadata.csv");

    VTK_SD_LOG("VarjoEnvironmentCubemapService constructor session=" << session_.get()
        << " outputDir=" << pathForLog(output_directory_)
        << " queueCapacity=" << queue_capacity
        << " raw=" << pathForLog(raw_path_)
        << " metadata=" << pathForLog(metadata_path_));
}

VarjoEnvironmentCubemapService::~VarjoEnvironmentCubemapService()
{
    VTK_SD_LOG("VarjoEnvironmentCubemapService destructor running=" << (isRunning() ? "true" : "false"));
    stop();
}

bool VarjoEnvironmentCubemapService::start()
{
    VTK_SD_LOG("VarjoEnvironmentCubemapService::start requested outputDir=" << pathForLog(output_directory_));
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
        frame_count_ = 0;
        dropped_frame_count_ = 0;
        write_failure_count_ = 0;
        last_error_.clear();
        running_ = true;
    }

    stop_requested_.store(false);
    writer_thread_ = std::thread(&VarjoEnvironmentCubemapService::writerMain, this);
    SetThreadPriority(static_cast<HANDLE>(writer_thread_.native_handle()), THREAD_PRIORITY_BELOW_NORMAL);
    VTK_SD_LOG("VarjoEnvironmentCubemapService writer thread started");

    if (!data_stream_.start(stream_config_, channel_flags_, [this](const varjo_StreamFrame* frame, varjo_Session* callback_session) {
            this->onFrameReceived(frame, callback_session);
        })) {
        const std::string stream_error = data_stream_.lastError();
        setLastError(L"failed to start EnvironmentCubemap data stream");
        if (!stream_error.empty()) {
            VTK_SD_ERROR("EnvironmentCubemap data_stream_.start details=" << stream_error);
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

    VTK_SD_LOG("VarjoEnvironmentCubemapService started streamId=" << data_stream_.streamId()
        << " size=" << stream_config_.width << "x" << stream_config_.height
        << " fps=" << stream_config_.frameRate
        << " channels=" << channelBits(channel_flags_));
    return true;
}

void VarjoEnvironmentCubemapService::stop()
{
    const bool was_running = isRunning();
    VTK_SD_LOG("VarjoEnvironmentCubemapService::stop running=" << (was_running ? "true" : "false"));

    data_stream_.stop();

    stop_requested_.store(true);
    frame_queue_.notifyAll();

    if (writer_thread_.joinable()) {
        VTK_SD_LOG("joining VarjoEnvironmentCubemapService writer thread");
        writer_thread_.join();
    }

    closeOutputs();

    uint64_t frame_count = 0;
    uint64_t dropped_count = 0;
    uint64_t write_failure_count = 0;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        running_ = false;
        frame_count = frame_count_;
        dropped_count = dropped_frame_count_;
        write_failure_count = write_failure_count_;
    }

    if (was_running || frame_count > 0 || dropped_count > 0 || write_failure_count > 0) {
        VTK_SD_LOG("VarjoEnvironmentCubemapService stopped frames=" << frame_count
            << " dropped=" << dropped_count
            << " writeFailures=" << write_failure_count);
    }
}

bool VarjoEnvironmentCubemapService::isRunning() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return running_;
}

std::wstring VarjoEnvironmentCubemapService::lastError() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return last_error_;
}

VarjoEnvironmentCubemapService::Paths VarjoEnvironmentCubemapService::paths() const
{
    Paths p{};
    p.raw = raw_path_.wstring();
    p.metadata_csv = metadata_path_.wstring();
    return p;
}

uint64_t VarjoEnvironmentCubemapService::frameCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return frame_count_;
}

uint64_t VarjoEnvironmentCubemapService::droppedFrameCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return dropped_frame_count_;
}

uint64_t VarjoEnvironmentCubemapService::writeFailureCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return write_failure_count_;
}

bool VarjoEnvironmentCubemapService::selectStreamConfig()
{
    VTK_SD_LOG("selecting EnvironmentCubemap stream config CPU first-channel");

    VarjoDataStream::ConfigRequest request{};
    request.streamType = varjo_StreamType_EnvironmentCubemap;
    request.bufferType = varjo_BufferType_CPU;
    request.requiredChannels = varjo_ChannelFlag_First;
    request.requireAllRequiredChannels = false;

    auto best = data_stream_.findBestConfig(request);
    if (!best.has_value()) {
        setLastError(L"No CPU EnvironmentCubemap data stream with buffer was found");
        return false;
    }

    stream_config_ = best.value();
    channel_flags_ = (best.value().channelFlags != varjo_ChannelFlag_None) ? best.value().channelFlags : varjo_ChannelFlag_First;
    VTK_SD_LOG("selected EnvironmentCubemap stream config size=" << stream_config_.width << "x" << stream_config_.height
        << " fps=" << stream_config_.frameRate
        << " channels=" << channelBits(channel_flags_));
    return true;
}

bool VarjoEnvironmentCubemapService::openOutputs()
{
    VTK_SD_LOG("opening EnvironmentCubemap outputs outputDir=" << pathForLog(output_directory_));

    std::error_code ec;
    std::filesystem::create_directories(output_directory_, ec);
    if (ec) {
        VTK_SD_WARN("create_directories failed path=" << pathForLog(output_directory_) << " message=" << ec.message());
    }

    raw_.open(raw_path_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!raw_.is_open()) {
        setLastError(L"failed to open EnvironmentCubemap raw output: " + raw_path_.wstring());
        return false;
    }
    VTK_SD_LOG("opened EnvironmentCubemap raw path=" << pathForLog(raw_path_));

    metadata_csv_.open(metadata_path_, std::ios::out | std::ios::trunc);
    if (!metadata_csv_.is_open()) {
        setLastError(L"failed to open EnvironmentCubemap metadata CSV: " + metadata_path_.wstring());
        return false;
    }
    VTK_SD_LOG("opened EnvironmentCubemap metadata CSV path=" << pathForLog(metadata_path_));

    writeMetadataHeader(metadata_csv_);
    VTK_SD_LOG("EnvironmentCubemap outputs opened");
    return true;
}

void VarjoEnvironmentCubemapService::closeOutputs()
{
    const bool had_outputs = raw_.is_open() || metadata_csv_.is_open();
    if (had_outputs) {
        VTK_SD_LOG("closing EnvironmentCubemap outputs raw=" << pathForLog(raw_path_)
            << " csvOpen=" << (metadata_csv_.is_open() ? "true" : "false"));
    }

    if (raw_.is_open()) {
        raw_.flush();
        raw_.close();
    }
    if (metadata_csv_.is_open()) {
        metadata_csv_.flush();
        metadata_csv_.close();
    }
}

void VarjoEnvironmentCubemapService::onFrameReceived(const varjo_StreamFrame* frame, varjo_Session* callback_session)
{
    if (!frame || !callback_session || stop_requested_.load()) {
        VTK_SD_TRACE("EnvironmentCubemap frame callback skipped frame=" << frame << " callbackSession=" << callback_session);
        return;
    }
    if (frame->type != varjo_StreamType_EnvironmentCubemap) {
        VTK_SD_TRACE("EnvironmentCubemap frame callback skipped unexpected stream type=" << static_cast<int64_t>(frame->type));
        return;
    }

    if ((frame->channels & varjo_ChannelFlag_First) != 0) {
        captureFrame(*frame, callback_session, varjo_ChannelIndex_First);
    }
}

void VarjoEnvironmentCubemapService::captureFrame(
    const varjo_StreamFrame& frame,
    varjo_Session* callback_session,
    varjo_ChannelIndex channel_index)
{
    if ((frame.dataFlags & varjo_DataFlag_Buffer) == 0) {
        VTK_SD_TRACE("EnvironmentCubemap capture skipped because buffer flag is missing frameNumber=" << frame.frameNumber
            << " channel=" << channelIndexValue(channel_index));
        return;
    }

    const varjo_BufferId buffer_id = varjo_GetBufferId(callback_session, frame.id, frame.frameNumber, channel_index);
    if (buffer_id == varjo_InvalidId) {
        VTK_SD_TRACE("EnvironmentCubemap capture skipped because varjo_GetBufferId returned invalid id frameNumber=" << frame.frameNumber
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
        VTK_SD_TRACE("EnvironmentCubemap capture skipped because buffer lock failed bufferId=" << buffer_id
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
        VTK_SD_TRACE("EnvironmentCubemap capture skipped because CPU buffer data is empty frameNumber=" << frame.frameNumber
            << " channel=" << channelIndexValue(channel_index)
            << " bufferType=" << static_cast<int64_t>(captured.buffer_metadata.type)
            << " byteSize=" << captured.buffer_metadata.byteSize);
    }
}

void VarjoEnvironmentCubemapService::pushCapturedFrame(CapturedFrame&& frame)
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
            VTK_SD_WARN("EnvironmentCubemap frame queue dropped frames droppedNow=" << dropped << " totalDropped=" << total_dropped);
        }
    }
}

void VarjoEnvironmentCubemapService::writerMain()
{
    VTK_SD_LOG("VarjoEnvironmentCubemapService writer started");
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

    VTK_SD_LOG("VarjoEnvironmentCubemapService writer stopped frames=" << frameCount()
        << " dropped=" << droppedFrameCount()
        << " writeFailures=" << writeFailureCount());
}

void VarjoEnvironmentCubemapService::writeFrame(const CapturedFrame& frame)
{
    uint64_t row_index = 0;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        row_index = frame_count_;
    }

    uint64_t byte_offset = 0;
    uint64_t byte_size = 0;
    if (!writeRawBuffer(raw_, frame, byte_offset, byte_size)) {
        uint64_t total_write_failures = 0;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ++write_failure_count_;
            total_write_failures = write_failure_count_;
        }
        if (shouldLogCounter(total_write_failures)) {
            VTK_SD_WARN("EnvironmentCubemap write frame failed frameNumber=" << frame.stream_frame.frameNumber
                << " rowIndex=" << row_index
                << " totalWriteFailures=" << total_write_failures);
        }
    }

    if (metadata_csv_.is_open()) {
        writeMetadataRow(metadata_csv_, frame, row_index, byte_offset, byte_size);
    } else {
        VTK_SD_TRACE("EnvironmentCubemap metadata row skipped because CSV is not open");
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ++frame_count_;
    }
}

bool VarjoEnvironmentCubemapService::writeRawBuffer(std::ofstream& ofs, const CapturedFrame& frame, uint64_t& byte_offset, uint64_t& byte_size)
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

void VarjoEnvironmentCubemapService::writeMetadataHeader(std::ofstream& ofs)
{
    ofs
        << "row_index,"
        << "raw_byte_offset,raw_byte_size,"
        << "system_timestamp_unix_us,"
        << "system_timestamp_utc_iso8601,"
        << "stream_id,stream_type,frame_number,channels,data_flags,channel_index,"
        << "cubemap_timestamp,cubemap_timestamp_unix_us,"
        << "cubemap_mode,white_balance_temperature,brightness_normalization_gain,"
        << "wb_gain_r,wb_gain_g,wb_gain_b";

    for (int i = 0; i < 9; ++i) {
        ofs << ",inv_ccm_m" << i;
    }
    for (int i = 0; i < 9; ++i) {
        ofs << ",ccm_m" << i;
    }
    ofs << ",buffer_format,buffer_type,buffer_byte_size,buffer_row_stride,buffer_width,buffer_height";
    for (int i = 0; i < 16; ++i) {
        ofs << ",hmd_pose_m" << i;
    }
    ofs << "\n";
}

void VarjoEnvironmentCubemapService::writeMetadataRow(
    std::ofstream& ofs,
    const CapturedFrame& frame,
    uint64_t row_index,
    uint64_t byte_offset,
    uint64_t byte_size)
{
    const auto& sf = frame.stream_frame;
    const auto& cm = sf.metadata.environmentCubemap;

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
        << cm.timestamp << ','
        << convertVarjoTimeToUnixUs(cm.timestamp) << ','
        << cm.mode << ','
        << cm.whiteBalanceTemperature << ','
        << cm.brightnessNormalizationGain << ','
        << cm.wbNormalizationData.whiteBalanceColorGains[0] << ','
        << cm.wbNormalizationData.whiteBalanceColorGains[1] << ','
        << cm.wbNormalizationData.whiteBalanceColorGains[2] << ','
        << VarjoToolkit::Csv::toCsv(cm.wbNormalizationData.invCCM) << ','
        << VarjoToolkit::Csv::toCsv(cm.wbNormalizationData.ccm) << ','
        << VarjoToolkit::Csv::toCsv(frame.buffer_metadata) << ','
        << VarjoToolkit::Csv::toCsv(sf.hmdPose)
        << '\n';
}

int64_t VarjoEnvironmentCubemapService::convertVarjoTimeToUnixUs(varjo_Nanoseconds timestamp) const
{
    VarjoTimestampMapping mapping(session_);
    int64_t unix_us = 0;
    mapping.convertVarjoTimestampToUnixUs(timestamp, unix_us);
    return unix_us;
}

void VarjoEnvironmentCubemapService::setLastError(const std::wstring& message)
{
    VTK_SD_ERROR("VarjoEnvironmentCubemapService error: " << wideToUtf8ForLog(message));
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_ = message;
}
