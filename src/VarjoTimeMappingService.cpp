#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <VarjoServices/TimeMapping/VarjoTimeMappingService.hpp>

#include <Windows.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

int64_t systemTimeUnixUsFromTimePoint(std::chrono::system_clock::time_point tp)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        tp.time_since_epoch()).count();
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

std::string formatSystemClockLocalIso8601(std::chrono::system_clock::time_point tp)
{
    const auto us_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch());
    const auto sec_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(us_since_epoch);
    const int micros = static_cast<int>((us_since_epoch - sec_since_epoch).count());

    const std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    localtime_s(&tm, &tt);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
        << "." << std::setw(6) << std::setfill('0') << micros;
    return oss.str();
}

} // namespace

VarjoTimeMappingService::VarjoTimeMappingService(const std::shared_ptr<varjo_Session>& session, int interval_ms)
    : session_(session)
    , interval_ms_((interval_ms > 0) ? interval_ms : 5)
{}

VarjoTimeMappingService::~VarjoTimeMappingService()
{
    stop();
}

bool VarjoTimeMappingService::start(const std::wstring& csv_output_path)
{
    stop();

    if (!session_) {
        return false;
    }

    output_path_ = std::filesystem::path(csv_output_path);
    if (output_path_.empty()) {
        return false;
    }

    if (!openFile()) {
        return false;
    }

    stop_requested_.store(false);
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        running_ = true;
        row_count_ = 0;
    }

    worker_ = std::thread(&VarjoTimeMappingService::workerMain, this);
    return true;
}

void VarjoTimeMappingService::stop()
{
    stop_requested_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }

    closeFile();

    std::lock_guard<std::mutex> lock(state_mutex_);
    running_ = false;
}

bool VarjoTimeMappingService::convertVarjoTimestampToUnixUs(varjo_Nanoseconds varjo_timestamp, int64_t& out_unix_us) const
{
    if (!session_ || varjo_timestamp <= 0) {
        out_unix_us = 0;
        return false;
    }

    const varjo_Nanoseconds unix_ns = varjo_ConvertToUnixTime(session_.get(), varjo_timestamp);
    out_unix_us = static_cast<int64_t>(unix_ns / 1000);
    return out_unix_us > 0;
}

bool VarjoTimeMappingService::isRunning() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return running_;
}

uint64_t VarjoTimeMappingService::rowCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return row_count_;
}

std::wstring VarjoTimeMappingService::outputPath() const
{
    return output_path_.wstring();
}

void VarjoTimeMappingService::workerMain()
{
    // This is only a timestamp sampling/logger thread. Keep it below the camera
    // and render path priorities.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    while (!stop_requested_.load()) {
        const auto start = std::chrono::steady_clock::now();

        const VarjoTimeMappingSample sample = sampleOnce();
        writeRow(sample);

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ++row_count_;
        }

        const auto elapsed = std::chrono::steady_clock::now() - start;
        const auto interval = std::chrono::milliseconds(interval_ms_);
        if (elapsed < interval) {
            std::this_thread::sleep_for(interval - elapsed);
        }
    }
}

bool VarjoTimeMappingService::openFile()
{
    closeFile();

    const auto parent = output_path_.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    csv_file_.open(output_path_, std::ios::out | std::ios::trunc);
    if (!csv_file_.is_open()) {
        return false;
    }

    writeHeader();
    return true;
}

void VarjoTimeMappingService::closeFile()
{
    if (csv_file_.is_open()) {
        csv_file_.flush();
        csv_file_.close();
    }
}

void VarjoTimeMappingService::writeHeader()
{
    csv_file_
        << "row_index,"
        << "varjo_timestamp,"
        << "varjo_timestamp_unix_ns,"
        << "varjo_timestamp_unix_us,"
        << "system_timestamp_unix_us,"
        << "system_timestamp_utc_iso8601,"
        << "system_timestamp_local_iso8601,"
        << "delta_varjo_unix_minus_system_us\n";
}

void VarjoTimeMappingService::writeRow(const VarjoTimeMappingSample& sample)
{
    if (!csv_file_.is_open()) {
        return;
    }

    uint64_t row_index = 0;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        row_index = row_count_;
    }

    const int64_t delta_us = sample.varjo_timestamp_unix_us - sample.system_timestamp_unix_us;

    csv_file_
        << row_index << ","
        << sample.varjo_timestamp << ","
        << sample.varjo_timestamp_unix_ns << ","
        << sample.varjo_timestamp_unix_us << ","
        << sample.system_timestamp_unix_us << ","
        << formatSystemClockUtcIso8601(sample.system_timestamp) << ","
        << formatSystemClockLocalIso8601(sample.system_timestamp) << ","
        << delta_us << "\n";
}

VarjoTimeMappingSample VarjoTimeMappingService::sampleOnce() const
{
    VarjoTimeMappingSample sample{};
    if (!session_) {
        return sample;
    }

    sample.system_timestamp = std::chrono::system_clock::now();
    sample.system_timestamp_unix_us = systemTimeUnixUsFromTimePoint(sample.system_timestamp);
    sample.varjo_timestamp = varjo_GetCurrentTime(session_.get());
    sample.varjo_timestamp_unix_ns = varjo_ConvertToUnixTime(session_.get(), sample.varjo_timestamp);
    sample.varjo_timestamp_unix_us = static_cast<int64_t>(sample.varjo_timestamp_unix_ns / 1000);
    return sample;
}
