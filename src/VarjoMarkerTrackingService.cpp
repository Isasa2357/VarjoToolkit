#include <VarjoToolkit/Services/MarkerTracking/VarjoMarkerTrackingService.hpp>

#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <chrono>
#include <utility>
#include <vector>

namespace {

std::string booleanField(bool value)
{
    return VarjoToolkit::Csv::boolean(value);
}

std::string emptyMarkerComponentCsv()
{
    return VarjoToolkit::Csv::emptyFields(6);
}

std::string emptyPoseCsv()
{
    return VarjoToolkit::Csv::emptyFields(28);
}

} // namespace

VarjoMarkerTrackingCsvLogger::VarjoMarkerTrackingCsvLogger(std::filesystem::path filepath)
    : filepath_(std::move(filepath))
{}

VarjoMarkerTrackingCsvLogger::~VarjoMarkerTrackingCsvLogger()
{
    close();
}

bool VarjoMarkerTrackingCsvLogger::open()
{
    if (file_.is_open()) {
        setLastError("marker CSV is already open");
        return false;
    }

    const auto parent = filepath_.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            setLastError("failed to create marker CSV output directory");
            return false;
        }
    }

    file_.open(filepath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        setLastError("failed to open marker CSV");
        return false;
    }

    file_ << header();
    last_error_.clear();
    return true;
}

void VarjoMarkerTrackingCsvLogger::close()
{
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

bool VarjoMarkerTrackingCsvLogger::isOpen() const
{
    return file_.is_open();
}

void VarjoMarkerTrackingCsvLogger::write(const VarjoMarkerTrackingRecord& record)
{
    if (file_.is_open()) {
        file_ << row(record);
    }
}

const std::filesystem::path& VarjoMarkerTrackingCsvLogger::filepath() const
{
    return filepath_;
}

const std::string& VarjoMarkerTrackingCsvLogger::lastError() const
{
    return last_error_;
}

std::string VarjoMarkerTrackingCsvLogger::header()
{
    return VarjoToolkit::Csv::join({
        "row_index",
        "sample_timestamp",
        "has_marker_component",
        "has_pose_component",
        VarjoToolkit::Csv::headerForWorldObject("object"),
        VarjoToolkit::Csv::headerForWorldObjectMarkerComponent("marker"),
        poseHeader("pose")
    }) + "\n";
}

std::string VarjoMarkerTrackingCsvLogger::row(const VarjoMarkerTrackingRecord& record)
{
    return VarjoToolkit::Csv::join({
        std::to_string(record.rowIndex),
        std::to_string(record.sampleTimestamp),
        booleanField(record.marker.hasMarker),
        booleanField(record.marker.hasPose),
        VarjoToolkit::Csv::toCsv(record.marker.object),
        record.marker.hasMarker ? VarjoToolkit::Csv::toCsv(record.marker.marker) : emptyMarkerComponentCsv(),
        record.marker.hasPose ? poseToCsv(record.marker.pose) : emptyPoseCsv()
    }) + "\n";
}

std::string VarjoMarkerTrackingCsvLogger::poseHeader(const std::string& name)
{
    return VarjoToolkit::Csv::join({
        VarjoToolkit::Csv::headerForMatrix(name + ".matrix"),
        VarjoToolkit::Csv::headerForVector3D(name + ".velocity"),
        VarjoToolkit::Csv::headerForVector3D(name + ".angularVelocity"),
        VarjoToolkit::Csv::headerForVector3D(name + ".acceleration"),
        VarjoToolkit::Csv::makeHeader(name, {"poseFlags", "timeStamp", "confidence"})
    });
}

std::string VarjoMarkerTrackingCsvLogger::poseToCsv(const varjo_WorldPoseComponent& pose)
{
    return VarjoToolkit::Csv::join({
        VarjoToolkit::Csv::toCsv(pose.pose),
        VarjoToolkit::Csv::toCsv(pose.velocity),
        VarjoToolkit::Csv::toCsv(pose.angularVelocity),
        VarjoToolkit::Csv::toCsv(pose.acceleration),
        std::to_string(pose.poseFlags),
        std::to_string(pose.timeStamp),
        VarjoToolkit::Csv::number(pose.confidence)
    });
}

void VarjoMarkerTrackingCsvLogger::setLastError(std::string message) const
{
    last_error_ = std::move(message);
}

VarjoMarkerTrackingService::VarjoMarkerTrackingService(
    std::shared_ptr<varjo_Session> session,
    std::filesystem::path filepath,
    size_t queueSize,
    int sampleIntervalMs)
    : session_(std::move(session))
    , marker_tracker_(session_)
    , logger_(std::move(filepath))
    , queue_size_(queueSize)
    , sample_interval_ms_(sampleIntervalMs < 0 ? 0 : sampleIntervalMs)
{}

VarjoMarkerTrackingService::~VarjoMarkerTrackingService()
{
    stop();
}

bool VarjoMarkerTrackingService::start()
{
    if (running_.load()) {
        return true;
    }
    if (!session_) {
        setLastError("session is null");
        return false;
    }
    if (!marker_tracker_.valid()) {
        setLastError(marker_tracker_.world().lastError().empty() ? "marker tracker is invalid" : marker_tracker_.world().lastError());
        return false;
    }
    if (!logger_.open()) {
        setLastError(logger_.lastError());
        return false;
    }

    stop_signal_.store(false);
    running_.store(true);
    thread_ = std::thread(&VarjoMarkerTrackingService::worker, this);
    last_error_.clear();
    return true;
}

void VarjoMarkerTrackingService::stop()
{
    stop_signal_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
    running_.store(false);
    logger_.close();
}

bool VarjoMarkerTrackingService::running() const
{
    return running_.load();
}

std::deque<VarjoMarkerTrackingRecord> VarjoMarkerTrackingService::requestMarkers()
{
    std::deque<VarjoMarkerTrackingRecord> out;
    std::lock_guard<std::mutex> lock(mutex_);
    out.swap(queue_);
    return out;
}

uint64_t VarjoMarkerTrackingService::rowCount() const
{
    return row_count_.load();
}

const std::string& VarjoMarkerTrackingService::lastError() const
{
    return last_error_;
}

void VarjoMarkerTrackingService::worker()
{
    while (!stop_signal_.load()) {
        const varjo_Nanoseconds sampleTimestamp = varjo_GetCurrentTime(session_.get());
        const auto markers = marker_tracker_.markers(sampleTimestamp, true);
        if (!markers.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& marker : markers) {
                VarjoMarkerTrackingRecord record{};
                record.rowIndex = row_count_.fetch_add(1);
                record.sampleTimestamp = sampleTimestamp;
                record.marker = marker;

                logger_.write(record);
                queue_.push_back(record);
                while (queue_.size() > queue_size_) {
                    queue_.pop_front();
                }
            }
        }

        if (sample_interval_ms_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sample_interval_ms_));
        } else {
            std::this_thread::yield();
        }
    }
}

void VarjoMarkerTrackingService::setLastError(std::string message) const
{
    last_error_ = std::move(message);
}
