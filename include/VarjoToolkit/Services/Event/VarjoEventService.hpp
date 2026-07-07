#pragma once

#include <Varjo.h>
#include <Varjo_events.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <VarjoToolkit/Core/VarjoEventQueue.hpp>

struct VarjoEventRecord {
    uint64_t rowIndex = 0;
    std::string eventTypeName;
    varjo_Event event{};
};

class VarjoEventCsvLogger {
public:
    explicit VarjoEventCsvLogger(std::filesystem::path filepath);
    ~VarjoEventCsvLogger();

    bool open();
    void close();
    bool isOpen() const;
    void write(const VarjoEventRecord& record);

    const std::filesystem::path& filepath() const;
    const std::string& lastError() const;

    static std::string header();
    static std::string row(const VarjoEventRecord& record);

private:
    void setLastError(std::string message) const;

private:
    std::filesystem::path filepath_;
    std::ofstream file_;
    mutable std::string last_error_;
};

class VarjoEventService {
public:
    VarjoEventService(
        std::shared_ptr<varjo_Session> session,
        std::filesystem::path filepath,
        size_t queueSize = 1000,
        int pollIntervalMs = 10,
        size_t maxEventsPerPoll = 64);
    ~VarjoEventService();

    VarjoEventService(const VarjoEventService&) = delete;
    VarjoEventService& operator=(const VarjoEventService&) = delete;

    bool start();
    void stop();
    bool running() const;

    std::deque<VarjoEventRecord> requestEvents();
    uint64_t rowCount() const;
    const std::string& lastError() const;

private:
    void worker();
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_;
    VarjoEventQueue event_queue_;
    VarjoEventCsvLogger logger_;

    std::deque<VarjoEventRecord> queue_;
    size_t queue_size_ = 1000;
    int poll_interval_ms_ = 10;
    size_t max_events_per_poll_ = 64;
    mutable std::mutex mutex_;

    std::thread thread_;
    std::atomic_bool stop_signal_{false};
    std::atomic_bool running_{false};
    std::atomic_uint64_t row_count_{0};
    mutable std::string last_error_;
};
