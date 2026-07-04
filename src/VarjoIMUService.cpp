#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <VarjoToolkit/Services/IMU/VarjoIMUService.hpp>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

void setCurrentThreadLowPriorityForWaitSync()
{
    // This service calls varjo_WaitSync only to sample IMU/head pose and write a
    // CSV. Keep it below the rendering/camera path priority.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
}

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

int64_t convertVarjoTimeToUnixUs(varjo_Session* session, varjo_Nanoseconds timestamp)
{
    if (!session || timestamp <= 0) {
        return 0;
    }
    const varjo_Nanoseconds unix_ns = varjo_ConvertToUnixTime(session, timestamp);
    return static_cast<int64_t>(unix_ns / 1000);
}

void writeMatrixCsv(std::ostream& os, const varjo_Matrix& matrix)
{
    for (int i = 0; i < 16; ++i) {
        if (i != 0) {
            os << ',';
        }
        os << matrix.value[i];
    }
}

} // namespace

VarjoIMUService::VarjoIMUService(
    const std::shared_ptr<varjo_Session>& session,
    const std::wstring& csv_output_path,
    size_t buffer_capacity)
    : session_(session)
    , csv_output_path_(csv_output_path)
    , buffer_capacity_((buffer_capacity > min_buffer_capacity_) ? buffer_capacity : min_buffer_capacity_)
{}

VarjoIMUService::~VarjoIMUService()
{
    stop();
}

bool VarjoIMUService::start(bool waitFillBuffer)
{
    stop();

    if (varjo_IsAvailable() == varjo_False) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_error_ = L"varjo_IsAvailable returned false";
        return false;
    }

    if (!session_) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_error_ = L"session is null";
        return false;
    }

    if (!openLogFile()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> buffer_lock(imu_buffer_mutex_);
        imu_buffer_.clear();
        previous_data_ = VarjoIMUData{};
    }

    {
        std::lock_guard<std::mutex> state_lock(state_mutex_);
        row_count_ = 0;
        running_ = true;
        last_error_.clear();
    }

    stop_requested_.store(false);
    worker_ = std::thread(&VarjoIMUService::workerMain, this);
    SetThreadPriority(static_cast<HANDLE>(worker_.native_handle()), THREAD_PRIORITY_LOWEST);

    if (waitFillBuffer) {
        while (!stop_requested_.load() && bufferSize() < bufferCapacity()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    return true;
}

void VarjoIMUService::stop()
{
    stop_requested_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }

    closeLogFile();

    std::lock_guard<std::mutex> lock(state_mutex_);
    running_ = false;
}

size_t VarjoIMUService::bufferSize() const
{
    std::lock_guard<std::mutex> lock(imu_buffer_mutex_);
    return imu_buffer_.size();
}

uint64_t VarjoIMUService::rowCount() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return row_count_;
}

std::wstring VarjoIMUService::outputPath() const
{
    return csv_output_path_.wstring();
}

std::wstring VarjoIMUService::lastError() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return last_error_;
}

VarjoIMUService::VarjoIMUData VarjoIMUService::latestData() const
{
    std::lock_guard<std::mutex> lock(imu_buffer_mutex_);
    if (imu_buffer_.empty()) {
        return VarjoIMUData{};
    }
    return imu_buffer_.back();
}

std::deque<VarjoIMUService::VarjoIMUData> VarjoIMUService::requestBufferedData() const
{
    std::lock_guard<std::mutex> lock(imu_buffer_mutex_);
    return imu_buffer_;
}

bool VarjoIMUService::openLogFile()
{
    closeLogFile();

    if (csv_output_path_.empty()) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_error_ = L"CSV output path is empty";
        return false;
    }

    const auto parent = csv_output_path_.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        logfile_.open(csv_output_path_, std::ios::out | std::ios::trunc);
        if (!logfile_.is_open()) {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            last_error_ = L"failed to open IMU CSV: " + csv_output_path_.wstring();
            return false;
        }
        writeHeader();
    }

    return true;
}

void VarjoIMUService::closeLogFile()
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (logfile_.is_open()) {
        logfile_.flush();
        logfile_.close();
    }
}

void VarjoIMUService::writeHeader()
{
    logfile_
        << "row_index,"
        << "system_timestamp_unix_us,"
        << "system_timestamp_utc_iso8601,"
        << "system_timestamp_local_iso8601,"
        << "varjo_now,"
        << "varjo_now_unix_us,"
        << "frame_number,"
        << "frame_display_time,"
        << "frame_display_time_unix_us,"
        << "position_x,position_y,position_z,"
        << "euler_x_deg,euler_y_deg,euler_z_deg,"
        << "angular_velocity_x_deg_s,angular_velocity_y_deg_s,angular_velocity_z_deg_s";

    for (int i = 0; i < 16; ++i) {
        logfile_ << ",pose_m" << i;
    }
    logfile_ << "\n";
}

void VarjoIMUService::writeRow(const VarjoIMUData& data)
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (!logfile_.is_open()) {
        return;
    }

    uint64_t row_index = 0;
    {
        std::lock_guard<std::mutex> state_lock(state_mutex_);
        row_index = row_count_;
    }

    logfile_
        << row_index << ","
        << data.system_unix_us << ","
        << formatSystemClockUtcIso8601(data.system_time) << ","
        << formatSystemClockLocalIso8601(data.system_time) << ","
        << data.varjo_now << ","
        << data.varjo_now_unix_us << ","
        << data.frame_number << ","
        << data.frame_display_time << ","
        << data.frame_display_time_unix_us << ","
        << data.position.x << "," << data.position.y << "," << data.position.z << ","
        << data.euler_deg.x << "," << data.euler_deg.y << "," << data.euler_deg.z << ","
        << data.angular_velocity.x << "," << data.angular_velocity.y << "," << data.angular_velocity.z << ",";

    writeMatrixCsv(logfile_, data.pose);
    logfile_ << "\n";
}

VarjoIMUService::VarjoIMUData VarjoIMUService::sampleOnce()
{
    VarjoIMUData data{};
    if (!session_) {
        return data;
    }

    varjo_FrameInfo* frame_info = varjo_CreateFrameInfo(session_.get());
    if (!frame_info) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_error_ = L"varjo_CreateFrameInfo returned null";
        return data;
    }

    data.system_time = std::chrono::system_clock::now();
    data.system_unix_us = systemTimeUnixUsFromTimePoint(data.system_time);
    data.varjo_now = varjo_GetCurrentTime(session_.get());
    data.varjo_now_unix_us = convertVarjoTimeToUnixUs(session_.get(), data.varjo_now);

    varjo_WaitSync(session_.get(), frame_info);

    data.frame_number = frame_info->frameNumber;
    data.frame_display_time = frame_info->displayTime;
    data.frame_display_time_unix_us = convertVarjoTimeToUnixUs(session_.get(), data.frame_display_time);
    data.frame_info = *frame_info;

    varjo_FreeFrameInfo(frame_info);

    data.pose = varjo_FrameGetPose(session_.get(), varjo_PoseType_Center);
    data.position = varjo_GetPosition(&data.pose);

    const auto euler_rad = varjo_GetEulerAngles(
        &data.pose,
        varjo_EulerOrder_XYZ,
        varjo_RotationDirection_CounterClockwise,
        varjo_Handedness_RightHanded);

    data.euler_deg = varjo_Vector3D{
        euler_rad.x * 180.0 / kPi,
        euler_rad.y * 180.0 / kPi,
        euler_rad.z * 180.0 / kPi
    };

    {
        std::lock_guard<std::mutex> lock(imu_buffer_mutex_);
        if (previous_data_.valid) {
            const double delta_time_sec = std::chrono::duration_cast<std::chrono::duration<double>>(
                data.system_time - previous_data_.system_time).count();
            if (delta_time_sec > 1.0e-9) {
                data.angular_velocity = varjo_Vector3D{
                    (data.euler_deg.x - previous_data_.euler_deg.x) / delta_time_sec,
                    (data.euler_deg.y - previous_data_.euler_deg.y) / delta_time_sec,
                    (data.euler_deg.z - previous_data_.euler_deg.z) / delta_time_sec
                };
            }
        }
        previous_data_ = data;
    }

    data.valid = true;
    return data;
}

void VarjoIMUService::workerMain()
{
    setCurrentThreadLowPriorityForWaitSync();

    while (!stop_requested_.load()) {
        const VarjoIMUData data = sampleOnce();
        if (!data.valid) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(imu_buffer_mutex_);
            previous_data_ = data;
            imu_buffer_.push_back(data);
            while (imu_buffer_.size() > buffer_capacity_) {
                imu_buffer_.pop_front();
            }
        }

        writeRow(data);

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            ++row_count_;
        }
    }
}
