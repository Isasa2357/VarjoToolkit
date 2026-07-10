#pragma once

#include <Varjo.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Utilities/VarjoRunCountingDeque.hpp>
#include <VarjoToolkit/Utilities/VarjoRunResetSignal.hpp>

struct VarjoProjectedGazePosition {
    varjo_Vector2Df leftEye;
    varjo_Vector2Df rightEye;
    varjo_Vector2Df combinedEyeToLeft;
    varjo_Vector2Df combinedEyeToRight;
};

struct FrameInfo {
    std::vector<varjo_ViewInfo> views;
    varjo_Nanoseconds displayTime = 0;
    int64_t frameNumber = 0;
};

struct VarjoEyeTrackingData {
    varjo_Gaze gaze;
    varjo_EyeMeasurements measurements;
    std::optional<double> userIPD;
    std::optional<double> hmdIPD;
    std::string ipdAdjustmentMode;
    std::optional<varjo_Gaze> renderingGaze;
    VarjoProjectedGazePosition gazePos_toVideo;
    std::optional<VarjoProjectedGazePosition> renderingGazePos_toVideo;
    VarjoProjectedGazePosition gazePos_toVarjoDisplay;
    std::optional<VarjoProjectedGazePosition> renderingGazePos_toVarjoDisplay;
    FrameInfo frameInfo;
    std::optional<FrameInfo> renderingGazeFrameInfo;
};

class VarjoEyeTrackingProvider {
public:
    enum class OutputFilterType {
        NONE,
        STANDARD
    };

    enum class OutputFrequency {
        MAXIMUM,
        _100HZ,
        _200HZ
    };

    enum class Status {
        NOT_AVAILABLE,
        NOT_CONNECTED,
        NOT_CALIBRATED,
        CALIBRATING,
        CALIBRATED
    };

    explicit VarjoEyeTrackingProvider(const std::shared_ptr<varjo_Session>& session);
    ~VarjoEyeTrackingProvider();

    void initialize(OutputFilterType outputFilterType, OutputFrequency outputFrequency);
    void shutdown();

    // Supplies frame timing and view matrices captured by the single external
    // rendering synchronization owner. This class never calls varjo_WaitSync.
    bool submitFrameInfo(const VarjoFrameInfoSnapshot& snapshot);

    Status getStatus() const;
    std::vector<std::pair<varjo_Gaze, varjo_EyeMeasurements>> getGazeDataWithEyeMeasurements() const;
    std::optional<double> getUserIPD() const;
    std::optional<double> getHMDIPD() const;
    std::string getIPDAdjustmentMode() const;
    std::optional<varjo_Gaze> getRenderingGaze() const;
    std::vector<VarjoEyeTrackingData> getEyeTrackingData();

    uint64_t submittedFrameInfoCount() const;
    uint64_t droppedFrameInfoCount() const;

private:
    VarjoProjectedGazePosition calcProjectedGazePositionToVarjoDisplay(
        const varjo_Gaze& gaze,
        const std::vector<varjo_ViewInfo>& viewInfo) const;
    varjo_Vector2Df calcProjectedGazePositionToVarjoDisplayOneRay(
        const varjo_Ray& gazeRay,
        double focusDistance,
        const std::vector<varjo_ViewInfo>& viewInfo,
        size_t targetViewIndex) const;
    VarjoProjectedGazePosition calcProjectedGazePositionToVideo(
        const varjo_Gaze& gaze,
        const std::vector<varjo_ViewInfo>& viewInfo) const;

private:
    const std::shared_ptr<varjo_Session> session_;
    const int viewCount_;

    std::deque<FrameInfo> frameInfos_;
    const size_t frameInfoCapacity_ = 512;
    mutable std::mutex frameInfoMtx_;
    uint64_t submittedFrameInfoCount_ = 0;
    uint64_t droppedFrameInfoCount_ = 0;
};

class VarjoEyeTrackingDataLogger {
public:
    VarjoEyeTrackingDataLogger(const std::string& filepath, std::shared_ptr<varjo_Session> session);
    ~VarjoEyeTrackingDataLogger();

    bool open();
    void close();
    void write(const VarjoEyeTrackingData& data);

private:
    std::string getHeaderCsvString();
    std::string varjoEyeTrackingDataToCsvString(const VarjoEyeTrackingData& data);
    std::string varjoGazeToCsvString(const varjo_Gaze& gaze);
    std::string varjoEyeMeasurementsToCsvString(const varjo_EyeMeasurements& measurements);
    std::string varjoProjectedGazePositionToCsvString(const VarjoProjectedGazePosition& gazePos);
    std::string frameInfoToCsvString(const FrameInfo& frameInfo);

private:
    std::shared_ptr<varjo_Session> session_;
    std::filesystem::path filepath_;
    std::ofstream logfile_;
    const int viewCount_;
};

class VarjoEyeTrackingService {
public:
    VarjoEyeTrackingService(
        const std::shared_ptr<varjo_Session>& session,
        VarjoEyeTrackingProvider::OutputFilterType outputFilterType,
        VarjoEyeTrackingProvider::OutputFrequency outputFrequency,
        const std::string& filepath,
        size_t queueSize = 1000,
        int acquireFrequencyMs = 5);

    bool start();
    void stop();

    // Call once per externally synchronized rendering frame.
    bool submitFrameInfo(const VarjoFrameInfoSnapshot& snapshot);

    std::deque<VarjoEyeTrackingData> requestData();

    uint64_t receivedSampleCount() const noexcept { return dataQueue_.receivedCount(); }
    uint64_t processedSampleCount() const noexcept { return receivedSampleCount(); }
    uint64_t writtenSampleCount() const noexcept { return receivedSampleCount(); }
    uint64_t droppedSampleCount() const noexcept { return dataQueue_.droppedCount(); }
    double getSamplesPerSecond() const { return dataQueue_.samplesPerSecond(); }

    uint64_t submittedFrameInfoCount() const
    {
        return eyeTracker_.submittedFrameInfoCount();
    }

    uint64_t droppedFrameInfoCount() const
    {
        return eyeTracker_.droppedFrameInfoCount();
    }

private:
    void dataRequestWorker();

private:
    const std::shared_ptr<varjo_Session> session_;
    const VarjoEyeTrackingProvider::OutputFilterType outputFilterType_;
    const VarjoEyeTrackingProvider::OutputFrequency outputFrequency_;
    VarjoEyeTrackingProvider eyeTracker_;
    VarjoEyeTrackingDataLogger logger_;

    VarjoToolkit::RunCountingDeque<VarjoEyeTrackingData> dataQueue_;
    const size_t dataQueueMaxSize_;
    std::mutex dataQueueMutex_;

    std::thread dataRequestThread_;
    const int acquireFrequencyMs_;
    VarjoToolkit::RunResetSignal threadEndSignal_{false, &dataQueue_};
};
