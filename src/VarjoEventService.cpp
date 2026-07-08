#include <VarjoToolkit/Services/Event/VarjoEventService.hpp>

#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>
#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <chrono>
#include <utility>

VarjoEventCsvLogger::VarjoEventCsvLogger(std::filesystem::path filepath)
    : filepath_(std::move(filepath))
{
    VTK_SD_LOG("VarjoEventCsvLogger filepath=" << filepath_.string());
}

VarjoEventCsvLogger::~VarjoEventCsvLogger()
{
    close();
}

bool VarjoEventCsvLogger::open()
{
    VTK_SD_SCOPE("VarjoEventCsvLogger::open");
    VTK_SD_LOG("opening event CSV filepath=" << filepath_.string());
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
            VTK_SD_ERROR("create_directories failed path=" << parent.string() << " message=" << ec.message());
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
    VTK_SD_LOG("event CSV opened filepath=" << filepath_.string());
    return true;
}

void VarjoEventCsvLogger::close()
{
    if (file_.is_open()) {
        VTK_SD_LOG("closing event CSV filepath=" << filepath_.string());
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
        VTK_SD_TRACE("write event CSV row=" << record.rowIndex << " type=" << record.eventTypeName);
        file_ << row(record);
    } else {
        VTK_SD_WARN("event CSV write skipped because file is not open");
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
    VTK_SD_ERROR(last_error_);
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
{
    VTK_SD_LOG("VarjoEventService constructor session=" << session_.get()
        << " queueSize=" << queue_size_
        << " pollIntervalMs=" << poll_interval_ms_
        << " maxEventsPerPoll=" << max_events_per_poll_);
}

VarjoEventService::~VarjoEventService()
{
    VTK_SD_LOG("VarjoEventService destructor running=" << (running_.load() ? "true" : "false"));
    stop();
}

bool VarjoEventService::start()
{
    VTK_SD_SCOPE("VarjoEventService::start");
    if (running_.load()) {
        VTK_SD_LOG("event service already running");
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
    VTK_SD_LOG("starting event service worker thread");
    thread_ = std::thread(&VarjoEventService::worker, this);
    last_error_.clear();
    return true;
}

void VarjoEventService::stop()
{
    VTK_SD_LOG("VarjoEventService::stop running=" << (running_.load() ? "true" : "false"));
    stop_signal_.store(true);
    if (thread_.joinable()) {
        VTK_SD_LOG("joining event service worker thread");
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
    VTK_SD_LOG("requestEvents returned=" << out.size());
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
    VTK_SD_LOG("event service worker started");
    while (!stop_signal_.load()) {
        const auto events = event_queue_.pollAll(max_events_per_poll_);
        if (!events.empty()) {
            VTK_SD_LOG("event service polled events=" << events.size());
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& event : events) {
                VarjoEventRecord record{};
                record.rowIndex = row_count_.fetch_add(1);
                record.event = event;
                record.eventTypeName = VarjoEventQueue::eventTypeToString(event.header.type);

                logger_.write(record);
                queue_.push_back(record);
                while (queue_.size() > queue_size_) {
                    VTK_SD_TRACE("event queue over capacity size=" << queue_.size() << " limit=" << queue_size_);
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
    VTK_SD_LOG("event service worker stopped rowCount=" << row_count_.load());
}

void VarjoEventService::setLastError(std::string message) const
{
    last_error_ = std::move(message);
    VTK_SD_ERROR(last_error_);
}
