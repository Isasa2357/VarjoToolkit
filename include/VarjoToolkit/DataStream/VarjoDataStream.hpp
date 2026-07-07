#pragma once

#include <Varjo.h>
#include <Varjo_datastream.h>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <VarjoToolkit/Core/VarjoSession.hpp>

// RAII wrapper for Varjo DataStream start/stop and config enumeration.
//
// This class keeps ownership of std::shared_ptr<varjo_Session> when constructed
// from shared ownership or VarjoSession, but also supports raw varjo_Session* for
// direct Varjo C API interop.
//
// Copy and move are disabled intentionally. When a stream is started, the Varjo
// runtime stores this object's address as callback user_data. Moving the object
// would leave the runtime pointing at the old address.
class VarjoDataStream {
public:
    using Callback = std::function<void(const varjo_StreamFrame* frame, varjo_Session* session)>;

    struct ConfigRequest {
        varjo_StreamType streamType = static_cast<varjo_StreamType>(0);
        std::optional<varjo_TextureFormat> format;
        std::optional<varjo_BufferType> bufferType;
        varjo_ChannelFlag requiredChannels = varjo_ChannelFlag_None;
        bool requireAllRequiredChannels = true;
        bool preferHigherFrameRate = true;
        bool preferLargerResolution = true;
    };

public:
    explicit VarjoDataStream(varjo_Session* session);
    explicit VarjoDataStream(std::shared_ptr<varjo_Session> session);
    explicit VarjoDataStream(const VarjoSession& session);
    ~VarjoDataStream();

    VarjoDataStream(const VarjoDataStream&) = delete;
    VarjoDataStream& operator=(const VarjoDataStream&) = delete;
    VarjoDataStream(VarjoDataStream&&) = delete;
    VarjoDataStream& operator=(VarjoDataStream&&) = delete;

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Session* session() const;
    std::shared_ptr<varjo_Session> sharedSession() const;
    bool ownsSession() const;

    std::vector<varjo_StreamConfig> enumerateConfigs() const;
    static bool configMatches(const varjo_StreamConfig& config, const ConfigRequest& request);
    static int64_t configScore(const varjo_StreamConfig& config, const ConfigRequest& request);
    std::optional<varjo_StreamConfig> findBestConfig(const ConfigRequest& request) const;

    bool start(const varjo_StreamConfig& config, varjo_ChannelFlag channels, Callback callback);
    bool start(const varjo_StreamConfig& config, Callback callback);
    bool startBest(const ConfigRequest& request, Callback callback);
    bool startBest(const ConfigRequest& request, varjo_ChannelFlag channels, Callback callback);

    void stop();
    bool running() const;

    varjo_StreamId streamId() const;
    varjo_ChannelFlag channelFlags() const;
    varjo_StreamConfig config() const;
    std::string lastError() const;

private:
    static void onFrameReceivedStatic(
        const varjo_StreamFrame* frame,
        varjo_Session* session,
        void* user_data);

    void dispatchFrame(const varjo_StreamFrame* frame, varjo_Session* callback_session);
    void setLastError(std::string message);
    static bool hasAnyChannels(varjo_ChannelFlag flags, varjo_ChannelFlag required);
    static bool hasAllChannels(varjo_ChannelFlag flags, varjo_ChannelFlag required);

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;

    mutable std::mutex state_mutex_;
    varjo_StreamConfig config_{};
    varjo_StreamId stream_id_ = varjo_InvalidId;
    varjo_ChannelFlag channel_flags_ = varjo_ChannelFlag_None;
    bool running_ = false;
    std::string last_error_;

    mutable std::mutex callback_mutex_;
    Callback callback_;
};
