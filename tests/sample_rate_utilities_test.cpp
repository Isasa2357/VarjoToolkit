#include <VarjoToolkit/DataStream/VarjoDataStreamFrameQueue.hpp>
#include <VarjoToolkit/Utilities/VarjoRunCountingDeque.hpp>
#include <VarjoToolkit/Utilities/VarjoRunResetSignal.hpp>
#include <VarjoToolkit/Utilities/VarjoSampleRateCounter.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

void require(bool condition, const char* expression)
{
    if (!condition) {
        throw std::runtime_error(std::string("requirement failed: ") + expression);
    }
}

#define REQUIRE(expression) require(static_cast<bool>(expression), #expression)

struct ChannelFrame {
    int channel_index = 0;
    int payload = 0;
};

void testDataStreamQueueMetrics()
{
    VarjoDataStreamFrameQueue<ChannelFrame> queue(2);
    queue.clear();

    REQUIRE(queue.push(ChannelFrame{1, 10}) == 0);
    REQUIRE(queue.push(ChannelFrame{2, 20}) == 0);
    REQUIRE(queue.push(ChannelFrame{1, 30}) == 1);

    REQUIRE(queue.size() == 2);
    REQUIRE(queue.pushedCount() == 3);
    REQUIRE(queue.pushedCountForChannel(1) == 2);
    REQUIRE(queue.pushedCountForChannel(2) == 1);
    REQUIRE(queue.pushedCountForChannel(3) == 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1050));
    REQUIRE(queue.pushedRatePerSecond() > 0.0);
    REQUIRE(queue.pushedRatePerSecondForChannel(1) > 0.0);
    REQUIRE(queue.pushedRatePerSecondForChannel(2) > 0.0);

    queue.clear();
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.pushedCount() == 0);
    REQUIRE(queue.pushedCountForChannel(1) == 0);
    REQUIRE(queue.pushedRatePerSecond() == 0.0);
    REQUIRE(queue.pushedRatePerSecondForChannel(1) == 0.0);
}

void testRunResetSignal()
{
    VarjoToolkit::RunCountingDeque<int> samples;
    VarjoToolkit::RunResetSignal signal(true, &samples);

    samples.push_back(1);
    samples.push_back(2);
    REQUIRE(samples.size() == 2);
    REQUIRE(samples.receivedCount() == 2);

    signal.store(false);
    REQUIRE(!signal.load());
    REQUIRE(samples.empty());
    REQUIRE(samples.receivedCount() == 0);
    REQUIRE(samples.samplesPerSecond() == 0.0);

    samples.push_back(3);
    REQUIRE(samples.receivedCount() == 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(1050));
    REQUIRE(samples.samplesPerSecond() > 0.0);

    signal.store(true);
    REQUIRE(signal.load());
    REQUIRE(samples.receivedCount() == 1);

    signal.store(false);
    REQUIRE(samples.empty());
    REQUIRE(samples.receivedCount() == 0);
    REQUIRE(samples.samplesPerSecond() == 0.0);
}

void testSampleRateCounterReset()
{
    VarjoToolkit::SampleRateCounter counter;
    counter.reset(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1050));
    REQUIRE(counter.update(21) > 0.0);

    counter.resetPerformance();
    REQUIRE(counter.update(0) == 0.0);
}

} // namespace

int main()
{
    try {
        testDataStreamQueueMetrics();
        testRunResetSignal();
        testSampleRateCounterReset();
        std::cout << "VarjoToolkit sample-rate utility tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "VarjoToolkit sample-rate utility tests failed: " << e.what() << '\n';
        return 1;
    }
}
