#include <VarjoToolkit/DataStream/VarjoDataStream.hpp>
#include <VarjoToolkit/DataStream/VarjoDataStreamBufferLock.hpp>

#include <iostream>
#include <string>

namespace {

int fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

varjo_ChannelFlag channels(varjo_ChannelFlag a, varjo_ChannelFlag b)
{
    return static_cast<varjo_ChannelFlag>(static_cast<int64_t>(a) | static_cast<int64_t>(b));
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit DataStream wrapper test\n";

    VarjoDataStream null_stream(nullptr);
    if (null_stream.valid()) {
        return fail("VarjoDataStream constructed from nullptr should be invalid");
    }
    if (!null_stream.enumerateConfigs().empty()) {
        return fail("null VarjoDataStream should enumerate no configs");
    }

    varjo_StreamConfig cfg{};
    cfg.streamId = static_cast<varjo_StreamId>(123);
    cfg.streamType = varjo_StreamType_DistortedColor;
    cfg.format = varjo_TextureFormat_NV12;
    cfg.bufferType = varjo_BufferType_CPU;
    cfg.channelFlags = channels(varjo_ChannelFlag_Left, varjo_ChannelFlag_Right);
    cfg.width = 2880;
    cfg.height = 1404;
    cfg.frameRate = 90;

    VarjoDataStream::ConfigRequest exact{};
    exact.streamType = varjo_StreamType_DistortedColor;
    exact.format = varjo_TextureFormat_NV12;
    exact.bufferType = varjo_BufferType_CPU;
    exact.requiredChannels = channels(varjo_ChannelFlag_Left, varjo_ChannelFlag_Right);

    if (!VarjoDataStream::configMatches(cfg, exact)) {
        return fail("exact DataStream config request should match");
    }

    VarjoDataStream::ConfigRequest wrong_format = exact;
    wrong_format.format = varjo_TextureFormat_RGBA8;
    if (VarjoDataStream::configMatches(cfg, wrong_format)) {
        return fail("wrong format request should not match");
    }

    VarjoDataStream::ConfigRequest left_only = exact;
    left_only.requiredChannels = varjo_ChannelFlag_Left;
    if (!VarjoDataStream::configMatches(cfg, left_only)) {
        return fail("left-only required channel should match config with left+right");
    }

    VarjoDataStream::ConfigRequest any_eye = exact;
    any_eye.requiredChannels = channels(varjo_ChannelFlag_Left, varjo_ChannelFlag_Right);
    any_eye.requireAllRequiredChannels = false;
    cfg.channelFlags = varjo_ChannelFlag_Left;
    if (!VarjoDataStream::configMatches(cfg, any_eye)) {
        return fail("any-channel request should match config with left only");
    }

    cfg.channelFlags = varjo_ChannelFlag_Left;
    if (VarjoDataStream::configMatches(cfg, exact)) {
        return fail("all-channel request should not match config with left only");
    }

    cfg.channelFlags = channels(varjo_ChannelFlag_Left, varjo_ChannelFlag_Right);
    const int64_t score = VarjoDataStream::configScore(cfg, exact);
    if (score <= 0) {
        return fail("DataStream config score should be positive");
    }

    const bool start_result = null_stream.start(cfg, varjo_ChannelFlag_Left, [] (const varjo_StreamFrame*, varjo_Session*) {});
    if (start_result) {
        return fail("starting a null VarjoDataStream should fail");
    }
    if (null_stream.lastError().empty()) {
        return fail("failed null VarjoDataStream start should set lastError");
    }

    VarjoDataStreamBufferLock invalid_lock(nullptr, varjo_InvalidId);
    if (invalid_lock.locked()) {
        return fail("DataStreamBufferLock constructed with null session should not lock");
    }
    if (invalid_lock.cpuData() != nullptr) {
        return fail("invalid DataStreamBufferLock should not expose CPU data");
    }
    if (invalid_lock.bufferId() != varjo_InvalidId) {
        return fail("invalid DataStreamBufferLock should keep invalid buffer id before unlock/reset");
    }

    VarjoDataStreamBufferLock moved_invalid_lock(std::move(invalid_lock));
    if (moved_invalid_lock.locked()) {
        return fail("moved invalid DataStreamBufferLock should not lock");
    }

    std::cout << "[PASS] VarjoToolkit DataStream wrapper test passed\n";
    return 0;
}
