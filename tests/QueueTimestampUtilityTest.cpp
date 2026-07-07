#include <VarjoToolkit/DataStream/VarjoDataStreamFrameQueue.hpp>
#include <VarjoToolkit/Utilities/VarjoTimestampMapping.hpp>

#include <chrono>
#include <deque>
#include <iostream>
#include <string>

namespace {

struct TestFrame {
    int value = 0;
};

int fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

bool expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }
    return true;
}

bool expectEqualInt64(int64_t actual, int64_t expected, const std::string& message)
{
    if (actual != expected) {
        std::cerr << "[FAIL] " << message << " expected=" << expected << " actual=" << actual << "\n";
        return false;
    }
    return true;
}

bool expectEqualString(const std::string& actual, const std::string& expected, const std::string& message)
{
    if (actual != expected) {
        std::cerr << "[FAIL] " << message << " expected=" << expected << " actual=" << actual << "\n";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit queue/timestamp utility test\n";

    VarjoDataStreamFrameQueue<TestFrame> queue(2);
    if (!expectEqualInt64(static_cast<int64_t>(queue.capacity()), 2, "initial queue capacity")) return 1;
    if (!expectEqualInt64(static_cast<int64_t>(queue.size()), 0, "initial queue size")) return 1;

    if (!expectEqualInt64(static_cast<int64_t>(queue.push(TestFrame{1})), 0, "first push should not drop")) return 1;
    if (!expectEqualInt64(static_cast<int64_t>(queue.push(TestFrame{2})), 0, "second push should not drop")) return 1;
    if (!expectEqualInt64(static_cast<int64_t>(queue.push(TestFrame{3})), 1, "third push should drop oldest")) return 1;
    if (!expectEqualInt64(static_cast<int64_t>(queue.size()), 2, "queue size after bounded push")) return 1;

    std::deque<TestFrame> drained;
    queue.drain(drained);
    if (!expectEqualInt64(static_cast<int64_t>(drained.size()), 2, "drained frame count")) return 1;
    if (!expectEqualInt64(drained[0].value, 2, "oldest frame should be dropped")) return 1;
    if (!expectEqualInt64(drained[1].value, 3, "newest frame should remain")) return 1;
    if (!expectEqualInt64(static_cast<int64_t>(queue.size()), 0, "queue should be empty after drain")) return 1;

    queue.setCapacity(1);
    if (!expectEqualInt64(static_cast<int64_t>(queue.capacity()), 1, "capacity after setCapacity")) return 1;
    queue.push(TestFrame{10});
    queue.push(TestFrame{11});
    std::deque<TestFrame> swapped;
    queue.waitSwap(swapped, []() { return false; });
    if (!expectEqualInt64(static_cast<int64_t>(swapped.size()), 1, "waitSwap should swap available frames")) return 1;
    if (!expectEqualInt64(swapped.front().value, 11, "waitSwap should receive newest retained frame")) return 1;

    queue.clear();
    if (!expectEqualInt64(static_cast<int64_t>(queue.size()), 0, "clear should empty queue")) return 1;

    VarjoTimestampMapping nullMapping(nullptr);
    varjo_Nanoseconds ns = 123;
    if (nullMapping.convertVarjoTimestampToUnixNs(10, ns)) {
        return fail("null mapping should not convert timestamp to ns");
    }
    if (!expectEqualInt64(ns, 0, "failed ns conversion should zero output")) return 1;

    int64_t us = 123;
    if (nullMapping.convertVarjoTimestampToUnixUs(10, us)) {
        return fail("null mapping should not convert timestamp to us");
    }
    if (!expectEqualInt64(us, 0, "failed us conversion should zero output")) return 1;

    const auto zero = std::chrono::system_clock::time_point{};
    if (!expectEqualInt64(VarjoTimestampMapping::systemTimeToUnixUs(zero), 0, "epoch unix us")) return 1;
    if (!expectEqualString(VarjoTimestampMapping::formatUtcIso8601(zero), "1970-01-01T00:00:00.000000Z", "epoch UTC format")) return 1;

    const auto fractional = std::chrono::system_clock::time_point(std::chrono::microseconds(1234567));
    if (!expectEqualInt64(VarjoTimestampMapping::systemTimeToUnixUs(fractional), 1234567, "fractional unix us")) return 1;
    if (!expectEqualString(VarjoTimestampMapping::formatUtcIso8601(fractional), "1970-01-01T00:00:01.234567Z", "fractional UTC format")) return 1;

    const auto sample = nullMapping.sampleCurrentMapping();
    if (sample.valid) {
        return fail("null timestamp mapping sample should be invalid");
    }

    std::cout << "[PASS] VarjoToolkit queue/timestamp utility test passed\n";
    return 0;
}
