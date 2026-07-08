#include <VarjoToolkit/DataStream/VarjoDataStream.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>

#include <cstdint>
#include <utility>

namespace {

int64_t channelBits(varjo_ChannelFlag flags)
{
    return static_cast<int64_t>(flags);
}

varjo_ChannelFlag channelIntersection(varjo_ChannelFlag a, varjo_ChannelFlag b)
{
    return static_cast<varjo_ChannelFlag>(channelBits(a) & channelBits(b));
}

void logStreamConfig(const char* label, const varjo_StreamConfig& config)
{
    VTK_SD_LOG(label
        << " streamId=" << config.streamId
        << " streamType=" << static_cast<int64_t>(config.streamType)
        << " channelFlags=" << static_cast<int64_t>(config.channelFlags)
        << " bufferType=" << static_cast<int64_t>(config.bufferType)
        << " format=" << static_cast<int64_t>(config.format)
        << " width=" << config.width
        << " height=" << config.height
        << " frameRate=" << config.frameRate);
}

} // namespace

VarjoDataStream::VarjoDataStream(varjo_Session* session)
    : session_(session)
{
    VTK_SD_LOG("VarjoDataStream raw constructor session=" << session_);
}

VarjoDataStream::VarjoDataStream(std::shared_ptr<varjo_Session> session)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
{
    VTK_SD_LOG("VarjoDataStream shared constructor session=" << session_);
}

VarjoDataStream::VarjoDataStream(const VarjoSession& session)
    : VarjoDataStream(session.shared())
{}

VarjoDataStream::~VarjoDataStream()
{
    VTK_SD_LOG("VarjoDataStream destructor running=" << (running() ? "true" : "false"));
    stop();
}

bool VarjoDataStream::valid() const
{
    return session_ != nullptr;
}

varjo_Session* VarjoDataStream::session() const
{
    return session_;
}

std::shared_ptr<varjo_Session> VarjoDataStream::sharedSession() const
{
    return session_owner_;
}

bool VarjoDataStream::ownsSession() const
{
    return static_cast<bool>(session_owner_);
}

std::vector<varjo_StreamConfig> VarjoDataStream::enumerateConfigs() const
{
    VTK_SD_SCOPE("VarjoDataStream::enumerateConfigs");
    if (!session_) {
        VTK_SD_ERROR("enumerateConfigs called with null session");
        return {};
    }

    const int32_t config_count = varjo_GetDataStreamConfigCount(session_);
    VTK_SD_LOG("data stream config count=" << config_count);
    if (config_count <= 0) {
        return {};
    }

    std::vector<varjo_StreamConfig> configs(static_cast<size_t>(config_count));
    varjo_GetDataStreamConfigs(session_, configs.data(), config_count);
    for (size_t i = 0; i < configs.size(); ++i) {
        logStreamConfig((std::string("config[") + std::to_string(i) + "]").c_str(), configs[i]);
    }
    return configs;
}

bool VarjoDataStream::configMatches(const varjo_StreamConfig& config, const ConfigRequest& request)
{
    if (config.streamType != request.streamType) {
        return false;
    }
    if (request.format.has_value() && config.format != request.format.value()) {
        return false;
    }
    if (request.bufferType.has_value() && config.bufferType != request.bufferType.value()) {
        return false;
    }
    if (request.requiredChannels != varjo_ChannelFlag_None) {
        if (request.requireAllRequiredChannels) {
            return hasAllChannels(config.channelFlags, request.requiredChannels);
        }
        return hasAnyChannels(config.channelFlags, request.requiredChannels);
    }
    return true;
}

int64_t VarjoDataStream::configScore(const varjo_StreamConfig& config, const ConfigRequest& request)
{
    int64_t score = 0;
    if (request.preferHigherFrameRate) {
        score += static_cast<int64_t>(config.frameRate) * 1000000000000LL;
    }
    if (request.preferLargerResolution) {
        score += static_cast<int64_t>(config.width) * static_cast<int64_t>(config.height);
    }
    VTK_SD_TRACE("configScore streamId=" << config.streamId << " score=" << score);
    return score;
}

std::optional<varjo_StreamConfig> VarjoDataStream::findBestConfig(const ConfigRequest& request) const
{
    VTK_SD_SCOPE("VarjoDataStream::findBestConfig");
    VTK_SD_LOG("request streamType=" << static_cast<int64_t>(request.streamType)
        << " formatSet=" << (request.format.has_value() ? "true" : "false")
        << " bufferTypeSet=" << (request.bufferType.has_value() ? "true" : "false")
        << " requiredChannels=" << static_cast<int64_t>(request.requiredChannels));
    const auto configs = enumerateConfigs();
    std::optional<varjo_StreamConfig> best;
    int64_t best_score = 0;

    for (const auto& config : configs) {
        if (!configMatches(config, request)) {
            continue;
        }

        const int64_t score = configScore(config, request);
        if (!best.has_value() || score > best_score) {
            best = config;
            best_score = score;
        }
    }

    if (best.has_value()) {
        logStreamConfig("bestConfig", best.value());
    } else {
        VTK_SD_WARN("no matching data stream config found");
    }
    return best;
}

bool VarjoDataStream::start(const varjo_StreamConfig& config, varjo_ChannelFlag channels, Callback callback)
{
    VTK_SD_SCOPE("VarjoDataStream::start");
    logStreamConfig("start requested", config);
    VTK_SD_LOG("requestedChannels=" << static_cast<int64_t>(channels));
    stop();

    if (!session_) {
        setLastError("session is null");
        return false;
    }
    if (config.streamId == varjo_InvalidId) {
        setLastError("stream config has invalid streamId");
        return false;
    }
    if (!callback) {
        setLastError("callback is empty");
        return false;
    }

    varjo_ChannelFlag start_channels = channels;
    if (start_channels == varjo_ChannelFlag_None) {
        start_channels = config.channelFlags;
    }
    if (start_channels == varjo_ChannelFlag_None) {
        setLastError("channel flags are empty");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_ = std::move(callback);
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        config_ = config;
        stream_id_ = config.streamId;
        channel_flags_ = start_channels;
        running_ = true;
        last_error_.clear();
    }

    VTK_SD_LOG("varjo_StartDataStream streamId=" << stream_id_ << " channelFlags=" << static_cast<int64_t>(channel_flags_));
    varjo_StartDataStream(
        session_,
        stream_id_,
        channel_flags_,
        &VarjoDataStream::onFrameReceivedStatic,
        this);

    return true;
}

bool VarjoDataStream::start(const varjo_StreamConfig& config, Callback callback)
{
    return start(config, config.channelFlags, std::move(callback));
}

bool VarjoDataStream::startBest(const ConfigRequest& request, Callback callback)
{
    return startBest(request, varjo_ChannelFlag_None, std::move(callback));
}

bool VarjoDataStream::startBest(const ConfigRequest& request, varjo_ChannelFlag channels, Callback callback)
{
    VTK_SD_SCOPE("VarjoDataStream::startBest");
    auto config = findBestConfig(request);
    if (!config.has_value()) {
        setLastError("no matching data stream config was found");
        return false;
    }

    varjo_ChannelFlag start_channels = channels;
    if (start_channels == varjo_ChannelFlag_None) {
        if (request.requiredChannels != varjo_ChannelFlag_None) {
            if (request.requireAllRequiredChannels) {
                start_channels = request.requiredChannels;
            } else {
                start_channels = channelIntersection(config.value().channelFlags, request.requiredChannels);
            }
        } else {
            start_channels = config.value().channelFlags;
        }
    }

    VTK_SD_LOG("startBest resolvedChannels=" << static_cast<int64_t>(start_channels));
    return start(config.value(), start_channels, std::move(callback));
}

void VarjoDataStream::stop()
{
    varjo_Session* session_to_stop = nullptr;
    varjo_StreamId stream_to_stop = varjo_InvalidId;

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (running_ && session_ && stream_id_ != varjo_InvalidId) {
            session_to_stop = session_;
            stream_to_stop = stream_id_;
        }
        running_ = false;
        stream_id_ = varjo_InvalidId;
        channel_flags_ = varjo_ChannelFlag_None;
        config_ = varjo_StreamConfig{};
    }

    if (session_to_stop && stream_to_stop != varjo_InvalidId) {
        VTK_SD_LOG("varjo_StopDataStream streamId=" << stream_to_stop);
        varjo_StopDataStream(session_to_stop, stream_to_stop);
    }

    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_ = nullptr;
    }
}

bool VarjoDataStream::running() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return running_;
}

varjo_StreamId VarjoDataStream::streamId() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return stream_id_;
}

varjo_ChannelFlag VarjoDataStream::channelFlags() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return channel_flags_;
}

varjo_StreamConfig VarjoDataStream::config() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return config_;
}

std::string VarjoDataStream::lastError() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return last_error_;
}

void VarjoDataStream::onFrameReceivedStatic(
    const varjo_StreamFrame* frame,
    varjo_Session* session,
    void* user_data)
{
    auto* self = static_cast<VarjoDataStream*>(user_data);
    if (!self) {
        VTK_SD_ERROR("onFrameReceivedStatic called with null user_data");
        return;
    }
    self->dispatchFrame(frame, session);
}

void VarjoDataStream::dispatchFrame(const varjo_StreamFrame* frame, varjo_Session* callback_session)
{
    Callback callback;
    {
        std::lock_guard<std::mutex> state_lock(state_mutex_);
        if (!running_) {
            VTK_SD_TRACE("dispatchFrame ignored because stream is not running");
            return;
        }
    }
    {
        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        callback = callback_;
    }

    if (callback) {
        VTK_SD_TRACE("dispatchFrame frame=" << frame << " callbackSession=" << callback_session);
        callback(frame, callback_session);
    } else {
        VTK_SD_WARN("dispatchFrame has no callback");
    }
}

void VarjoDataStream::setLastError(std::string message)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_ = std::move(message);
    VTK_SD_ERROR(last_error_);
}

bool VarjoDataStream::hasAnyChannels(varjo_ChannelFlag flags, varjo_ChannelFlag required)
{
    return (channelBits(flags) & channelBits(required)) != 0;
}

bool VarjoDataStream::hasAllChannels(varjo_ChannelFlag flags, varjo_ChannelFlag required)
{
    const int64_t required_bits = channelBits(required);
    return (channelBits(flags) & required_bits) == required_bits;
}
