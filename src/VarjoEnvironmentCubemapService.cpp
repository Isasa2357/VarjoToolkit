#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <VarjoToolkit/Services/Cubemap/VarjoEnvironmentCubemapService.hpp>

#include <VarjoToolkit/DataStream/VarjoDataStreamBufferLock.hpp>
#include <VarjoToolkit/Utilities/VarjoCsv.hpp>
#include <VarjoToolkit/Utilities/VarjoTimestampMapping.hpp>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <iomanip>
#include <sstream>

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
}

VarjoEnvironmentCubemapService::~VarjoEnvironmentCubemapService()
{
    stop();
}

bool VarjoEnvironmentCubemapService::start()
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

    if (!data_stream_.start(stream_config_, channel_flags_, [this](const varjo_StreamFrame* frame, varjo_Session* callback_session) {
            this->onFrameReceived(frame, callback_session);
        })) {
        setLastError(L"failed to start EnvironmentCubemap data stream");
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

    return true;
}

void VarjoEnvironmentCubemapService::stop()
{
    data_stream_.stop();

    stop_requested_.store(true);
    frame_queue_.notifyAll();

    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }

    closeOutputs();

    std::lock_guard<std::mutex> lock(state_mutex_);
    running_ = false;
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
    return true;
}

bool VarjoEnvironmentCubemapService::openOutputs()
{
    std::error_code ec;
    std::filesystem::create_directories(output_directory_, ec);

    raw_.open(raw_path_, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!raw_.is_open()) {
        setLastError(L"failed to open EnvironmentCubemap raw output: " + raw_path_.wstring());
        return false;
    }

    metadata_csv_.open(metadata_path_, std::ios::out | std::ios::trunc);
    if (!metadata_csv_.is_open()) {
        setLastError(L"failed to open EnvironmentCubemap metadata CSV: " + metadata_path_.wstring());
        return false;
    }

    writeMetadataHeader(metadata_csv_);
    return true;
}

void VarjoEnvironmentCubemapService::closeOutputs()
{
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
        return;
    }
    if (frame->type != varjo_StreamType_EnvironmentCubemap) {
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
        return;
    }

    const varjo_BufferId buffer_id = varjo_GetBufferId(callback_session, frame.id, frame.frameNumber, channel_index);
    if (buffer_id == varjo_InvalidId) {
        return;
    }

    CapturedFrame captured{};
    captured.system_time = std::chrono::system_clock::now();
    captured.system_unix_us = VarjoTimestampMapping::systemTimeToUnixUs(captured.system_time);
    captured.stream_frame = frame;
    captured.channel_index = channel_index;

    VarjoDataStreamBufferLock buffer_lock(callback_session, buffer_id);
    if (!buffer_lock) {
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
    }
}

void VarjoEnvironmentCubemapService::pushCapturedFrame(CapturedFrame&& frame)
{
    const size_t dropped = frame_queue_.push(std::move(frame));
    if (dropped > 0) {
        std::lock_guard<std::mutex> state_lock(state_mutex_);
        dropped_frame_count_ += dropped;
    }
}

void VarjoEnvironmentCubemapService::writerMain()
{
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
        std::lock_guard<std::mutex> lock(state_mutex_);
        ++write_failure_count_;
    }

    if (metadata_csv_.is_open()) {
        writeMetadataRow(metadata_csv_, frame, row_index, byte_offset, byte_size);
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
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_ = message;
}
