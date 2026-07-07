#include <VarjoToolkit/DataStream/VarjoDataStream.hpp>

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

} // namespace

VarjoDataStream::VarjoDataStream(varjo_Session* session)
    : session_(session)
{}

VarjoDataStream::VarjoDataStream(std::shared_ptr<varjo_Session> session)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
{}

VarjoDataStream::VarjoDataStream(const VarjoSession& session)
    : VarjoDataStream(session.shared())
{}

VarjoDataStream::~VarjoDataStream()
{
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
    if (!session_) {
        return {};
    }

    const int32_t config_count = varjo_GetDataStreamConfigCount(session_);
    if (config_count <= 0) {
        return {};
    }

    std::vector<varjo_StreamConfig> configs(static_cast<size_t>(config_count));
    varjo_GetDataStreamConfigs(session_, configs.data(), config_count);
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
    return score;
}

std::optional<varjo_StreamConfig> VarjoDataStream::findBestConfig(const ConfigRequest& request) const
{
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

    return best;
}

bool VarjoDataStream::start(const varjo_StreamConfig& config, varjo_ChannelFlag channels, Callback callback)
{
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
            return;
        }
    }
    {
        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        callback = callback_;
    }

    if (callback) {
        callback(frame, callback_session);
    }
}

void VarjoDataStream::setLastError(std::string message)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_ = std::move(message);
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
