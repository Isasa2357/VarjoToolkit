#include <VarjoToolkit/Utilities/VarjoTimestampMapping.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

std::tm makeUtcTm(std::time_t time)
{
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    return tm;
}

std::tm makeLocalTm(std::time_t time)
{
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    return tm;
}

std::string formatTimePointIso8601(
    std::chrono::system_clock::time_point timePoint,
    const char* suffix,
    bool useUtc)
{
    const auto usSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(
        timePoint.time_since_epoch());
    const auto secSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(usSinceEpoch);
    const int micros = static_cast<int>((usSinceEpoch - secSinceEpoch).count());

    const std::time_t tt = std::chrono::system_clock::to_time_t(timePoint);
    const std::tm tm = useUtc ? makeUtcTm(tt) : makeLocalTm(tt);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
        << "." << std::setw(6) << std::setfill('0') << micros
        << suffix;
    return oss.str();
}

} // namespace

VarjoTimestampMapping::VarjoTimestampMapping(std::shared_ptr<varjo_Session> session)
    : session_(std::move(session))
{}

bool VarjoTimestampMapping::convertVarjoTimestampToUnixNs(
    varjo_Nanoseconds varjoTimestamp,
    varjo_Nanoseconds& outUnixNs) const
{
    if (!session_ || varjoTimestamp <= 0) {
        outUnixNs = 0;
        return false;
    }

    outUnixNs = varjo_ConvertToUnixTime(session_.get(), varjoTimestamp);
    return outUnixNs > 0;
}

bool VarjoTimestampMapping::convertVarjoTimestampToUnixUs(
    varjo_Nanoseconds varjoTimestamp,
    int64_t& outUnixUs) const
{
    varjo_Nanoseconds unixNs = 0;
    if (!convertVarjoTimestampToUnixNs(varjoTimestamp, unixNs)) {
        outUnixUs = 0;
        return false;
    }

    outUnixUs = static_cast<int64_t>(unixNs / 1000);
    return outUnixUs > 0;
}

VarjoTimestampMapping::Sample VarjoTimestampMapping::sampleCurrentMapping() const
{
    Sample sample{};
    if (!session_) {
        return sample;
    }

    sample.systemTimestamp = std::chrono::system_clock::now();
    sample.systemTimestampUnixUs = systemTimeToUnixUs(sample.systemTimestamp);
    sample.varjoTimestamp = varjo_GetCurrentTime(session_.get());

    if (!convertVarjoTimestampToUnixNs(sample.varjoTimestamp, sample.varjoTimestampUnixNs)) {
        return sample;
    }

    sample.varjoTimestampUnixUs = static_cast<int64_t>(sample.varjoTimestampUnixNs / 1000);
    sample.deltaVarjoUnixMinusSystemUs = sample.varjoTimestampUnixUs - sample.systemTimestampUnixUs;
    sample.valid = true;
    return sample;
}

int64_t VarjoTimestampMapping::systemTimeToUnixUs(std::chrono::system_clock::time_point timePoint)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        timePoint.time_since_epoch()).count();
}

std::string VarjoTimestampMapping::formatUtcIso8601(std::chrono::system_clock::time_point timePoint)
{
    return formatTimePointIso8601(timePoint, "Z", true);
}

std::string VarjoTimestampMapping::formatLocalIso8601(std::chrono::system_clock::time_point timePoint)
{
    return formatTimePointIso8601(timePoint, "", false);
}
