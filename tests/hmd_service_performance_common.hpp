#pragma once

#include "hmd_service_test_common.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace vtk_hmd_performance_test {

using Clock = std::chrono::steady_clock;

inline int boundedEnvironmentInteger(const char* name, int defaultValue, int minimumValue, int maximumValue)
{
    const char* value = std::getenv(name);
    if (!value || *value == '\0') {
        return defaultValue;
    }

    try {
        return std::clamp(std::stoi(value), minimumValue, maximumValue);
    } catch (...) {
        return defaultValue;
    }
}

inline int measurementSeconds()
{
    return boundedEnvironmentInteger("VARJOTOOLKIT_HMD_PERFORMANCE_SECONDS", 10, 3, 120);
}

inline int warmupSeconds()
{
    return boundedEnvironmentInteger("VARJOTOOLKIT_HMD_PERFORMANCE_WARMUP_SECONDS", 1, 0, 30);
}

struct RateSummary {
    double minimum = 0.0;
    double average = 0.0;
    double maximum = 0.0;
    double observed_average = 0.0;
    double elapsed_seconds = 0.0;
    uint64_t count_at_start = 0;
    uint64_t count_at_end = 0;
    size_t interval_count = 0;
};

inline RateSummary summarizeRates(
    const std::vector<double>& rates,
    uint64_t countAtStart,
    uint64_t countAtEnd,
    double elapsedSeconds)
{
    VTK_HMD_TEST_REQUIRE(!rates.empty());
    VTK_HMD_TEST_REQUIRE(countAtEnd >= countAtStart);
    VTK_HMD_TEST_REQUIRE(elapsedSeconds > 0.0);

    const auto extrema = std::minmax_element(rates.begin(), rates.end());
    const double sum = std::accumulate(rates.begin(), rates.end(), 0.0);

    RateSummary summary{};
    summary.minimum = *extrema.first;
    summary.average = sum / static_cast<double>(rates.size());
    summary.maximum = *extrema.second;
    summary.observed_average = static_cast<double>(countAtEnd - countAtStart) / elapsedSeconds;
    summary.elapsed_seconds = elapsedSeconds;
    summary.count_at_start = countAtStart;
    summary.count_at_end = countAtEnd;
    summary.interval_count = rates.size();
    return summary;
}

inline void printSummary(
    const std::string& label,
    const RateSummary& summary,
    uint64_t received,
    uint64_t processed,
    uint64_t written,
    std::optional<uint64_t> dropped,
    std::optional<uint64_t> writeFailures)
{
    std::cout
        << std::fixed << std::setprecision(3)
        << "[ PERFORMANCE ] " << label
        << " intervals=" << summary.interval_count
        << " min=" << summary.minimum
        << " avg=" << summary.average
        << " max=" << summary.maximum
        << " observedAvg=" << summary.observed_average
        << " elapsedSeconds=" << summary.elapsed_seconds
        << " received=" << received
        << " processed=" << processed
        << " written=" << written;

    if (dropped.has_value()) {
        std::cout << " dropped=" << dropped.value();
    } else {
        std::cout << " dropped=n/a";
    }

    if (writeFailures.has_value()) {
        std::cout << " writeFailures=" << writeFailures.value();
    } else {
        std::cout << " writeFailures=n/a";
    }

    std::cout << '\n';
}

template <typename RateGetter, typename CountGetter>
RateSummary measureRate(
    const std::string& label,
    RateGetter&& rateGetter,
    CountGetter&& countGetter,
    int durationSeconds = measurementSeconds(),
    int warmupDurationSeconds = warmupSeconds())
{
    std::cout
        << "Performance measurement: " << label
        << " warmupSeconds=" << warmupDurationSeconds
        << " measurementSeconds=" << durationSeconds
        << '\n';

    if (warmupDurationSeconds > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(warmupDurationSeconds));
    }

    // Discard the warmup interval and establish the getter's next one-second baseline.
    (void)rateGetter();

    const uint64_t countAtStart = static_cast<uint64_t>(countGetter());
    uint64_t previousCount = countAtStart;
    const auto measurementStart = Clock::now();
    std::vector<double> rates;
    rates.reserve(static_cast<size_t>(durationSeconds));

    for (int second = 1; second <= durationSeconds; ++second) {
        std::this_thread::sleep_until(measurementStart + std::chrono::seconds(second));

        const uint64_t count = static_cast<uint64_t>(countGetter());
        const double rate = static_cast<double>(rateGetter());

        VTK_HMD_TEST_REQUIRE(count >= previousCount);
        VTK_HMD_TEST_REQUIRE(std::isfinite(rate));
        VTK_HMD_TEST_REQUIRE(rate >= 0.0);

        rates.push_back(rate);
        std::cout
            << "[ INTERVAL    ] " << label
            << " second=" << second
            << " count=" << count
            << " delta=" << (count - previousCount)
            << " rate=" << rate
            << '\n';
        previousCount = count;
    }

    const double elapsedSeconds = std::chrono::duration<double>(Clock::now() - measurementStart).count();
    const uint64_t countAtEnd = static_cast<uint64_t>(countGetter());
    const RateSummary summary = summarizeRates(rates, countAtStart, countAtEnd, elapsedSeconds);

    VTK_HMD_TEST_REQUIRE(summary.count_at_end > summary.count_at_start);
    VTK_HMD_TEST_REQUIRE(summary.maximum > 0.0);
    VTK_HMD_TEST_REQUIRE(summary.observed_average > 0.0);
    return summary;
}

struct DualRateSummary {
    RateSummary first;
    RateSummary second;
};

template <typename FirstRateGetter, typename FirstCountGetter, typename SecondRateGetter, typename SecondCountGetter>
DualRateSummary measureDualRates(
    const std::string& firstLabel,
    FirstRateGetter&& firstRateGetter,
    FirstCountGetter&& firstCountGetter,
    const std::string& secondLabel,
    SecondRateGetter&& secondRateGetter,
    SecondCountGetter&& secondCountGetter,
    int durationSeconds = measurementSeconds(),
    int warmupDurationSeconds = warmupSeconds())
{
    std::cout
        << "Dual performance measurement: " << firstLabel << " / " << secondLabel
        << " warmupSeconds=" << warmupDurationSeconds
        << " measurementSeconds=" << durationSeconds
        << '\n';

    if (warmupDurationSeconds > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(warmupDurationSeconds));
    }

    (void)firstRateGetter();
    (void)secondRateGetter();

    const uint64_t firstCountAtStart = static_cast<uint64_t>(firstCountGetter());
    const uint64_t secondCountAtStart = static_cast<uint64_t>(secondCountGetter());
    uint64_t previousFirstCount = firstCountAtStart;
    uint64_t previousSecondCount = secondCountAtStart;

    const auto measurementStart = Clock::now();
    std::vector<double> firstRates;
    std::vector<double> secondRates;
    firstRates.reserve(static_cast<size_t>(durationSeconds));
    secondRates.reserve(static_cast<size_t>(durationSeconds));

    for (int second = 1; second <= durationSeconds; ++second) {
        std::this_thread::sleep_until(measurementStart + std::chrono::seconds(second));

        const uint64_t firstCount = static_cast<uint64_t>(firstCountGetter());
        const uint64_t secondCount = static_cast<uint64_t>(secondCountGetter());
        const double firstRate = static_cast<double>(firstRateGetter());
        const double secondRate = static_cast<double>(secondRateGetter());

        VTK_HMD_TEST_REQUIRE(firstCount >= previousFirstCount);
        VTK_HMD_TEST_REQUIRE(secondCount >= previousSecondCount);
        VTK_HMD_TEST_REQUIRE(std::isfinite(firstRate));
        VTK_HMD_TEST_REQUIRE(std::isfinite(secondRate));
        VTK_HMD_TEST_REQUIRE(firstRate >= 0.0);
        VTK_HMD_TEST_REQUIRE(secondRate >= 0.0);

        firstRates.push_back(firstRate);
        secondRates.push_back(secondRate);

        std::cout
            << "[ INTERVAL    ] " << firstLabel
            << " second=" << second
            << " count=" << firstCount
            << " delta=" << (firstCount - previousFirstCount)
            << " rate=" << firstRate
            << '\n';
        std::cout
            << "[ INTERVAL    ] " << secondLabel
            << " second=" << second
            << " count=" << secondCount
            << " delta=" << (secondCount - previousSecondCount)
            << " rate=" << secondRate
            << '\n';

        previousFirstCount = firstCount;
        previousSecondCount = secondCount;
    }

    const double elapsedSeconds = std::chrono::duration<double>(Clock::now() - measurementStart).count();
    const uint64_t firstCountAtEnd = static_cast<uint64_t>(firstCountGetter());
    const uint64_t secondCountAtEnd = static_cast<uint64_t>(secondCountGetter());

    DualRateSummary result{};
    result.first = summarizeRates(firstRates, firstCountAtStart, firstCountAtEnd, elapsedSeconds);
    result.second = summarizeRates(secondRates, secondCountAtStart, secondCountAtEnd, elapsedSeconds);

    VTK_HMD_TEST_REQUIRE(result.first.count_at_end > result.first.count_at_start);
    VTK_HMD_TEST_REQUIRE(result.second.count_at_end > result.second.count_at_start);
    VTK_HMD_TEST_REQUIRE(result.first.maximum > 0.0);
    VTK_HMD_TEST_REQUIRE(result.second.maximum > 0.0);
    VTK_HMD_TEST_REQUIRE(result.first.observed_average > 0.0);
    VTK_HMD_TEST_REQUIRE(result.second.observed_average > 0.0);
    return result;
}

} // namespace vtk_hmd_performance_test
