#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <iterator>
#include <algorithm>
#include <atomic>
#include <utility>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <deque>

#include <Varjo.h>

#include <boost/circular_buffer.hpp>

struct VarjoProjectedGazePosition {
    varjo_Vector2Df leftEye;
    varjo_Vector2Df rightEye;
    varjo_Vector2Df combinedEyeToLeft;
    varjo_Vector2Df combinedEyeToRight;
};

struct FrameInfo {
    std::vector<varjo_ViewInfo> views;
    varjo_Nanoseconds displayTime;
    int64_t frameNumber;
};



struct VarjoEyeTrackingData {
    varjo_Gaze gaze;                                                                // 通常GazeのRayやステータス
    varjo_EyeMeasurements measurements;                                             // ユーザの眼の瞳孔や光彩の計測
    std::optional<double> userIPD;                                                  // ユーザのIPD
    std::optional<double> hmdIPD;                                                   // HMDが設定しているIPD
    std::string ipdAdjustmentMode;                                                  // 多分HMDのIPDの設定モード
    std::optional<varjo_Gaze> renderingGaze;                                        // レンダリング向けのGazeのRayやステータス
    VarjoProjectedGazePosition gazePos_toVideo;                                     // 通常Gazeから計算した2D動画向けの視線位置
    std::optional<VarjoProjectedGazePosition> renderingGazePos_toVideo;             // renderingGazeから計算した2D動画向けの視線位置
    VarjoProjectedGazePosition gazePos_toVarjoDisplay;                              // 通常Gazeから計算したVarjoディスプレイ向けの視線位置
	std::optional<VarjoProjectedGazePosition> renderingGazePos_toVarjoDisplay;      // renderingGazeから計算したVarjoディスプレイ向けの視線位置
    FrameInfo frameInfo;                                                            // 視線を取得した時のフレーム情報
    std::optional<FrameInfo> renderingGazeFrameInfo;                                // renderingGazeに対応するフレーム情報
};

class VarjoEyeTrackingProvider {

public:
    // Gaze output filter type
    enum class OutputFilterType {
        // Output filter is disabled
        NONE,

        // Standard smoothing output filter
        STANDARD
    };

    // Gaze output update frequency
    enum class OutputFrequency {
        // Maximum frequency supported by currently connected device
        MAXIMUM,
        // 100Hz frequency (supported by all devices)
        _100HZ,
        // 200Hz frequency (supported by VR-3, XR-3, XR-4 and Aero devices)
        _200HZ
    };

    // Gaze tracking status
    enum class Status {
        // Application is not allowed to access gaze data (privacy setting in VarjoBase)
        NOT_AVAILABLE,

        // Headset is not connected
        NOT_CONNECTED,

        // Gaze tracking is not calibrated
        NOT_CALIBRATED,

        // Gaze tracking is being calibrated
        CALIBRATING,

        // Gaze tracking is calibrated and can provide data for application
        CALIBRATED
    };

    VarjoEyeTrackingProvider(const std::shared_ptr<varjo_Session>& session);

    ~VarjoEyeTrackingProvider();

    void initialize(OutputFilterType outputFilterType, OutputFrequency outputFrequency);

    void shutdown();

    Status getStatus() const;

    std::vector<std::pair<varjo_Gaze, varjo_EyeMeasurements>> getGazeDataWithEyeMeasurements() const;

    std::optional<double> getUserIPD() const;

    std::optional<double> getHMDIPD() const;

    std::string getIPDAdjustmentMode() const;

    std::optional<varjo_Gaze> getRenderingGaze() const;

	std::vector<VarjoEyeTrackingData> getEyeTrackingData();

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
        const std::vector<varjo_ViewInfo>& viewInfo
    ) const;

    FrameInfo requestFrameInfo();

    void getFrameInfoWorkerFunction();

private:
    const std::shared_ptr<varjo_Session> session_;
    const int viewCount_;

    boost::circular_buffer<FrameInfo> frameInfos_;
    std::mutex frameInfoMtx_;
    std::thread getFrameInfoWorker_;
    std::atomic_bool workerStopSignal_{false};
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
        const VarjoEyeTrackingProvider::OutputFilterType outputFilterType,
        const VarjoEyeTrackingProvider::OutputFrequency outputFrequency,
        const std::string& filepath,
        const size_t queueSize = 1000,
        const int acquireFrequencyMs = 5
    );

    bool start();

    void stop();


    std::deque<VarjoEyeTrackingData> requestData();

private:

    void dataRequestWorker();


private:

    const std::shared_ptr<varjo_Session> session_;
    const VarjoEyeTrackingProvider::OutputFilterType outputFilterType_;
    const VarjoEyeTrackingProvider::OutputFrequency outputFrequency_;
    VarjoEyeTrackingProvider eyeTracker_;

    VarjoEyeTrackingDataLogger logger_;


    std::deque<VarjoEyeTrackingData> dataQueue_;
    const size_t dataQueueMaxSize_;
    std::mutex dataQueueMutex_;

    std::thread dataRequestThread_;
    const int acquireFrequencyMs_;
    std::atomic_bool threadEndSignal_{false};
};

