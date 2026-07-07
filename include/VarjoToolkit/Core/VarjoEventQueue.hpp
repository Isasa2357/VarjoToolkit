#pragma once

#include <Varjo.h>
#include <Varjo_events.h>

#include <memory>
#include <string>
#include <vector>

#include <VarjoToolkit/Core/VarjoSession.hpp>

class VarjoEventQueue {
public:
    explicit VarjoEventQueue(varjo_Session* session);
    explicit VarjoEventQueue(std::shared_ptr<varjo_Session> session);
    explicit VarjoEventQueue(const VarjoSession& session);
    ~VarjoEventQueue();

    VarjoEventQueue(const VarjoEventQueue&) = delete;
    VarjoEventQueue& operator=(const VarjoEventQueue&) = delete;
    VarjoEventQueue(VarjoEventQueue&& other) noexcept;
    VarjoEventQueue& operator=(VarjoEventQueue&& other) noexcept;

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Session* session() const;
    varjo_Event* eventStorage() const;

    bool poll(varjo_Event& outEvent);
    std::vector<varjo_Event> pollAll(size_t maxEvents = 64);

    const std::string& lastError() const;

    static std::string eventTypeToString(varjo_EventType type);

private:
    void resetEventStorage();
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    varjo_Event* event_ = nullptr;
    mutable std::string last_error_;
};
