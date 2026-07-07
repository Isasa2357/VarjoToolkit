#include <VarjoToolkit/Services/Event/VarjoEventService.hpp>

#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <chrono>
#include <utility>

VarjoEventCsvLogger::VarjoEventCsvLogger(std::filesystem::path filepath)
    : filepath_(std::move(filepath))
{}

VarjoEventCsvLogger::~VarjoEventCsvLogger()
{
    close();
}

bool VarjoEventCsvLogger::open()
{
    if (file_.is_open()) {
        setLastError("event CSV is already open");
        return false;
    }

    const auto parent = filepath_.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            setLastError("failed to create event CSV output directory");
            return false;
        }
    }

    file_.open(filepath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        setLastError("failed to open event CSV");
        return false;
    }

    file_ << header();
    last_error_.clear();
    return true;
}

void VarjoEventCsvLogger::close()
{
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

bool VarjoEventCsvLogger::isOpen() const
{
    return file_.is_open();
}

void VarjoEventCsvLogger::write(const VarjoEventRecord& record)
{
    if (file_.is_open()) {
        file_ << row(record);
    }
}

const std::filesystem::path& VarjoEventCsvLogger::filepath() const
{
    return filepath_;
}

const std::string& VarjoEventCsvLogger::lastError() const
{
    return last_error_;
}

std::string VarjoEventCsvLogger::header()
{
    return VarjoToolkit::Csv::join({
        "row_index",
        "event_type_name",
        VarjoToolkit::Csv::headerForEvent("event")
    }) + "\n";
}

std::string VarjoEventCsvLogger::row(const VarjoEventRecord& record)
{
    return VarjoToolkit::Csv::join({
        std::to_string(record.rowIndex),
        record.eventTypeName,
        VarjoToolkit::Csv::toCsv(record.event)
    }) + "\n";
}

void VarjoEventCsvLogger::setLastError(std::string message) const
{
    last_error_ = std::move(message);
}

VarjoEventService::VarjoEventService(
    std::shared_ptr<varjo_Session> session,
    std::filesystem::path filepath,
    size_t queueSize,
    int pollIntervalMs,
    size_t maxEventsPerPoll)
    : session_(std::move(session))
    , event_queue_(session_)
    , logger_(std::move(filepath))
    , queue_size_(queueSize)
    , poll_interval_ms_(pollIntervalMs < 0 ? 0 : pollIntervalMs)
    , max_events_per_poll_(maxEventsPerPoll == 0 ? 1 : maxEventsPerPoll)
{}

VarjoEventService::~VarjoEventService()
{
    stop();
}

bool VarjoEventService::start()
{
    if (running_.load()) {
        return true;
    }
    if (!session_) {
        setLastError("session is null");
        return false;
    }
    if (!event_queue_.valid()) {
        setLastError(event_queue_.lastError().empty() ? "event queue is invalid" : event_queue_.lastError());
        return false;
    }
    if (!logger_.open()) {
        setLastError(logger_.lastError());
        return false;
    }

    stop_signal_.store(false);
    running_.store(true);
    thread_ = std::thread(&VarjoEventService::worker, this);
    last_error_.clear();
    return true;
}

void VarjoEventService::stop()
{
    stop_signal_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
    running_.store(false);
    logger_.close();
}

bool VarjoEventService::running() const
{
    return running_.load();
}

std::deque<VarjoEventRecord> VarjoEventService::requestEvents()
{
    std::deque<VarjoEventRecord> out;
    std::lock_guard<std::mutex> lock(mutex_);
    out.swap(queue_);
    return out;
}

uint64_t VarjoEventService::rowCount() const
{
    return row_count_.load();
}

const std::string& VarjoEventService::lastError() const
{
    return last_error_;
}

void VarjoEventService::worker()
{
    while (!stop_signal_.load()) {
        const auto events = event_queue_.pollAll(max_events_per_poll_);
        if (!events.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& event : events) {
                VarjoEventRecord record{};
                record.rowIndex = row_count_.fetch_add(1);
                record.event = event;
                record.eventTypeName = VarjoEventQueue::eventTypeToString(event.header.type);

                logger_.write(record);
                queue_.push_back(record);
                while (queue_.size() > queue_size_) {
                    queue_.pop_front();
                }
            }
        }

        if (poll_interval_ms_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms_));
        } else {
            std::this_thread::yield();
        }
    }
}

void VarjoEventService::setLastError(std::string message) const
{
    last_error_ = std::move(message);
}
