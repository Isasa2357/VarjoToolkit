#include <VarjoToolkit/Services/IMU/VarjoIMUService.hpp>

#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>
#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

int64_t systemTimeUnixUsFromTimePoint(
    std::chrono::system_clock::time_point timePoint)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        timePoint.time_since_epoch()).count();
}

std::string formatSystemClockUtcIso8601(
    std::chrono::system_clock::time_point timePoint)
{
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        timePoint.time_since_epoch());
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        microseconds);
    const int fraction = static_cast<int>((microseconds - seconds).count());

    const std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm utc{};
    gmtime_s(&utc, &time);

    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
           << '.' << std::setw(6) << std::setfill('0') << fraction << 'Z';
    return output.str();
}

std::string formatSystemClockLocalIso8601(
    std::chrono::system_clock::time_point timePoint)
{
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        timePoint.time_since_epoch());
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        microseconds);
    const int fraction = static_cast<int>((microseconds - seconds).count());

    const std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm local{};
    localtime_s(&local, &time);

    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S")
           << '.' << std::setw(6) << std::setfill('0') << fraction;
    return output.str();
}

int64_t convertVarjoTimeToUnixUs(
    varjo_Session* session,
    varjo_Nanoseconds timestamp)
{
    if (!session || timestamp <= 0) return 0;
    return static_cast<int64_t>(
        varjo_ConvertToUnixTime(session, timestamp) / 1000);
}

} // namespace

VarjoIMUService::VarjoIMUService(
    const std::shared_ptr<varjo_Session>& session,
    const std::wstring& csvOutputPath,
    size_t bufferCapacity)
    : session_(session)
    , csv_output_path_(csvOutputPath)
    , buffer_capacity_(std::max(bufferCapacity, min_buffer_capacity_))
    , pending_capacity_(std::max<size_t>(buffer_capacity_ * 2, 8))
{
    VTK_SD_LOG("VarjoIMUService constructor session=" << session_.get()
        << " csv=" << csv_output_path_.string()
        << " bufferCapacity=" << buffer_capacity_
        << " pendingCapacity=" << pending_capacity_);
}

VarjoIMUService::~VarjoIMUService()
{
    stop();
}

bool VarjoIMUService::start()
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
    if (!openLogFile()) return false;

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_frames_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(imu_buffer_mutex_);
        imu_buffer_.clear();
        previous_data_ = VarjoIMUData{};
    }

    received_count_.store(0);
    processed_count_.store(0);
    written_count_.store(0);
    dropped_count_.store(0);
    sample_rate_counter_.reset();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_error_.clear();
        running_ = true;
    }

    stop_requested_.store(false);
    worker_ = std::thread(&VarjoIMUService::workerMain, this);
    VTK_SD_LOG("VarjoIMUService external-frame worker started");
    return true;
}

void VarjoIMUService::stop()
{
    stop_requested_.store(true);
    pending_cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    closeLogFile();

    std::lock_guard<std::mutex> lock(state_mutex_);
    running_ = false;
}

bool VarjoIMUService::submitFrameInfo(
    const VarjoFrameInfoSnapshot& snapshot)
{
    if (!snapshot.valid ||
        !snapshot.centerPoseValid ||
        snapshot.displayTime <= 0) {
        VTK_SD_WARN("VarjoIMUService rejected invalid external frame info");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!running_) return false;
    }

    PendingFrame pending{};
    pending.snapshot = snapshot;
    pending.system_time = std::chrono::system_clock::now();
    pending.system_unix_us = systemTimeUnixUsFromTimePoint(
        pending.system_time);
    pending.varjo_now = varjo_GetCurrentTime(session_.get());
    pending.varjo_now_unix_us = convertVarjoTimeToUnixUs(
        session_.get(),
        pending.varjo_now);

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        while (pending_frames_.size() >= pending_capacity_) {
            pending_frames_.pop_front();
            dropped_count_.fetch_add(1);
        }
        pending_frames_.push_back(std::move(pending));
        received_count_.fetch_add(1);
    }
    pending_cv_.notify_one();
    return true;
}

size_t VarjoIMUService::bufferSize() const
{
    std::lock_guard<std::mutex> lock(imu_buffer_mutex_);
    return imu_buffer_.size();
}

uint64_t VarjoIMUService::rowCount() const
{
    return written_count_.load();
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
    return imu_buffer_.empty() ? VarjoIMUData{} : imu_buffer_.back();
}

std::deque<VarjoIMUService::VarjoIMUData>
VarjoIMUService::requestBufferedData() const
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
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            last_error_ = L"failed to create IMU output directory";
            return false;
        }
    }

    std::lock_guard<std::mutex> lock(log_mutex_);
    logfile_.open(csv_output_path_, std::ios::out | std::ios::trunc);
    if (!logfile_.is_open()) {
        std::lock_guard<std::mutex> stateLock(state_mutex_);
        last_error_ = L"failed to open IMU CSV: " + csv_output_path_.wstring();
        return false;
    }
    writeHeader();
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
    for (int index = 0; index < 16; ++index) {
        logfile_ << ",pose_m" << index;
    }
    logfile_ << '\n';
}

bool VarjoIMUService::writeRow(
    const VarjoIMUData& data,
    uint64_t rowIndex)
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (!logfile_.is_open()) return false;

    logfile_
        << rowIndex << ','
        << data.system_unix_us << ','
        << formatSystemClockUtcIso8601(data.system_time) << ','
        << formatSystemClockLocalIso8601(data.system_time) << ','
        << data.varjo_now << ','
        << data.varjo_now_unix_us << ','
        << data.frame_number << ','
        << data.frame_display_time << ','
        << data.frame_display_time_unix_us << ','
        << VarjoToolkit::Csv::toCsv(data.position) << ','
        << VarjoToolkit::Csv::toCsv(data.euler_deg) << ','
        << VarjoToolkit::Csv::toCsv(data.angular_velocity) << ','
        << VarjoToolkit::Csv::toCsv(data.pose)
        << '\n';
    return static_cast<bool>(logfile_);
}

VarjoIMUService::VarjoIMUData VarjoIMUService::makeData(
    const PendingFrame& pending)
{
    VarjoIMUData data{};
    if (!pending.snapshot.valid || !pending.snapshot.centerPoseValid) {
        return data;
    }

    data.system_time = pending.system_time;
    data.system_unix_us = pending.system_unix_us;
    data.varjo_now = pending.varjo_now;
    data.varjo_now_unix_us = pending.varjo_now_unix_us;
    data.frame_number = pending.snapshot.frameNumber;
    data.frame_display_time = pending.snapshot.displayTime;
    data.frame_display_time_unix_us = convertVarjoTimeToUnixUs(
        session_.get(),
        data.frame_display_time);
    data.frame_info = pending.snapshot;
    data.pose = pending.snapshot.centerPose;
    data.position = varjo_GetPosition(&data.pose);

    const auto eulerRadians = varjo_GetEulerAngles(
        &data.pose,
        varjo_EulerOrder_XYZ,
        varjo_RotationDirection_CounterClockwise,
        varjo_Handedness_RightHanded);
    data.euler_deg = varjo_Vector3D{
        eulerRadians.x * 180.0 / kPi,
        eulerRadians.y * 180.0 / kPi,
        eulerRadians.z * 180.0 / kPi};
    data.valid = true;
    return data;
}

void VarjoIMUService::workerMain()
{
    VTK_SD_LOG("VarjoIMUService worker started without WaitSync");

    for (;;) {
        PendingFrame pending{};
        {
            std::unique_lock<std::mutex> lock(pending_mutex_);
            pending_cv_.wait(lock, [this]() {
                return stop_requested_.load() || !pending_frames_.empty();
            });
            if (stop_requested_.load() && pending_frames_.empty()) break;
            pending = std::move(pending_frames_.front());
            pending_frames_.pop_front();
        }

        VarjoIMUData data = makeData(pending);
        if (!data.valid) continue;

        {
            std::lock_guard<std::mutex> lock(imu_buffer_mutex_);
            if (previous_data_.valid) {
                const double deltaSeconds =
                    std::chrono::duration<double>(
                        data.system_time - previous_data_.system_time).count();
                if (deltaSeconds > 1.0e-9) {
                    data.angular_velocity = varjo_Vector3D{
                        (data.euler_deg.x - previous_data_.euler_deg.x) / deltaSeconds,
                        (data.euler_deg.y - previous_data_.euler_deg.y) / deltaSeconds,
                        (data.euler_deg.z - previous_data_.euler_deg.z) / deltaSeconds};
                }
            }
            previous_data_ = data;
            imu_buffer_.push_back(data);
            while (imu_buffer_.size() > buffer_capacity_) {
                imu_buffer_.pop_front();
            }
        }

        processed_count_.fetch_add(1);
        const uint64_t rowIndex = written_count_.load();
        if (writeRow(data, rowIndex)) {
            written_count_.fetch_add(1);
        } else {
            std::lock_guard<std::mutex> lock(state_mutex_);
            last_error_ = L"failed to write IMU CSV row";
        }
    }

    VTK_SD_LOG("VarjoIMUService worker stopped received="
        << received_count_.load()
        << " processed=" << processed_count_.load()
        << " written=" << written_count_.load()
        << " dropped=" << dropped_count_.load());
}
