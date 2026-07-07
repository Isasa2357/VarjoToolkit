#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <VarjoToolkit/Services/EyeTracking/VarjoEyeTrackingService.hpp>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>

namespace {

struct Vec4 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 0.0;
};

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

Vec3 addVec3(const Vec3& a, const Vec3& b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 mulVec3(const Vec3& v, double s) { return Vec3{v.x * s, v.y * s, v.z * s}; }
double dotVec3(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 crossVec3(const Vec3& a, const Vec3& b)
{
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

double lengthVec3(const Vec3& v) { return std::sqrt(dotVec3(v, v)); }

Vec3 normalizeVec3(const Vec3& v, const Vec3& fallback)
{
    const double len = lengthVec3(v);
    if (len <= 1.0e-9) {
        return fallback;
    }
    return mulVec3(v, 1.0 / len);
}

Vec3 toVec3(const varjo_Vector3D& v) { return Vec3{v.x, v.y, v.z}; }
varjo_Vector3D toVarjoVector3D(const Vec3& v) { return varjo_Vector3D{v.x, v.y, v.z}; }

Vec4 mulMat4Vec4(const double mat[16], const Vec4& v)
{
    // Varjo matrix is column-major. result = M * v.
    return Vec4{
        mat[0] * v.x + mat[4] * v.y + mat[8] * v.z + mat[12] * v.w,
        mat[1] * v.x + mat[5] * v.y + mat[9] * v.z + mat[13] * v.w,
        mat[2] * v.x + mat[6] * v.y + mat[10] * v.z + mat[14] * v.w,
        mat[3] * v.x + mat[7] * v.y + mat[11] * v.z + mat[15] * v.w
    };
}

Vec3 viewMatrixEyeOriginWorld(const double view[16])
{
    // view is world -> eye. For Varjo view matrices this is rigid, so inverse
    // origin is -R^T t. This avoids using current varjo_FrameGetPose().
    const double tx = view[12];
    const double ty = view[13];
    const double tz = view[14];
    return Vec3{
        -(view[0] * tx + view[1] * ty + view[2] * tz),
        -(view[4] * tx + view[5] * ty + view[6] * tz),
        -(view[8] * tx + view[9] * ty + view[10] * tz)
    };
}

Vec3 transformViewVectorToWorld(const double view[16], const Vec3& v)
{
    // Inverse rotation of the world->eye view matrix: R^T * v.
    return Vec3{
        view[0] * v.x + view[1] * v.y + view[2] * v.z,
        view[4] * v.x + view[5] * v.y + view[6] * v.z,
        view[8] * v.x + view[9] * v.y + view[10] * v.z
    };
}

bool reconstructHeadBasisFromFrameInfoViews(
    const std::vector<varjo_ViewInfo>& views,
    Vec3& center_world,
    Vec3& right_world,
    Vec3& up_world,
    Vec3& forward_world,
    Vec3& positive_z_world)
{
    if (views.size() < 2) {
        return false;
    }

    const Vec3 leftEye = viewMatrixEyeOriginWorld(views[0].viewMatrix);
    const Vec3 rightEye = viewMatrixEyeOriginWorld(views[1].viewMatrix);
    center_world = mulVec3(addVec3(leftEye, rightEye), 0.5);

    right_world = normalizeVec3(addVec3(
        transformViewVectorToWorld(views[0].viewMatrix, Vec3{1.0, 0.0, 0.0}),
        transformViewVectorToWorld(views[1].viewMatrix, Vec3{1.0, 0.0, 0.0})), Vec3{1.0, 0.0, 0.0});

    up_world = normalizeVec3(addVec3(
        transformViewVectorToWorld(views[0].viewMatrix, Vec3{0.0, 1.0, 0.0}),
        transformViewVectorToWorld(views[1].viewMatrix, Vec3{0.0, 1.0, 0.0})), Vec3{0.0, 1.0, 0.0});

    forward_world = normalizeVec3(addVec3(
        transformViewVectorToWorld(views[0].viewMatrix, Vec3{0.0, 0.0, -1.0}),
        transformViewVectorToWorld(views[1].viewMatrix, Vec3{0.0, 0.0, -1.0})), Vec3{0.0, 0.0, -1.0});

    right_world = normalizeVec3(right_world, normalizeVec3(crossVec3(forward_world, up_world), Vec3{1.0, 0.0, 0.0}));
    up_world = normalizeVec3(crossVec3(right_world, forward_world), up_world);
    positive_z_world = normalizeVec3(mulVec3(forward_world, -1.0), Vec3{0.0, 0.0, 1.0});
    return true;
}

varjo_Vector3D transformHeadPointToWorldUsingFrameInfo(
    const std::vector<varjo_ViewInfo>& views,
    const varjo_Vector3D& pointHeadRH)
{
    Vec3 center{}, right{}, up{}, forward{}, positiveZ{};
    if (!reconstructHeadBasisFromFrameInfoViews(views, center, right, up, forward, positiveZ)) {
        return pointHeadRH;
    }

    const Vec3 p = toVec3(pointHeadRH);
    const Vec3 world = addVec3(addVec3(addVec3(center, mulVec3(right, p.x)), mulVec3(up, p.y)), mulVec3(positiveZ, p.z));
    return toVarjoVector3D(world);
}

void setCurrentThreadLowPriorityForWaitSync()
{
    // This worker calls varjo_WaitSync only to collect frameInfo for gaze
    // projection. It must not compete with the main rendering/camera path.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
}

void copyViewInfo(const varjo_ViewInfo& src, varjo_ViewInfo& dst)
{
    std::copy(std::begin(src.projectionMatrix), std::end(src.projectionMatrix), std::begin(dst.projectionMatrix));
    std::copy(std::begin(src.viewMatrix), std::end(src.viewMatrix), std::begin(dst.viewMatrix));
    dst.preferredWidth = src.preferredWidth;
    dst.preferredHeight = src.preferredHeight;
    dst.enabled = src.enabled;
    dst.reserved = src.reserved;
}

void copyFrameInfo(const FrameInfo& src, FrameInfo& dst)
{
    dst.views.resize(src.views.size());
    for (size_t i = 0; i < src.views.size(); ++i) {
        copyViewInfo(src.views[i], dst.views[i]);
    }
    dst.displayTime = src.displayTime;
    dst.frameNumber = src.frameNumber;
}

FrameInfo makeEmptyFrameInfo(size_t viewCount)
{
    FrameInfo frameInfo{};
    frameInfo.views.resize(viewCount);
    frameInfo.displayTime = 0;
    frameInfo.frameNumber = 0;
    return frameInfo;
}

FrameInfo makeFrameInfoFromSnapshot(const VarjoFrameInfoSnapshot& snapshot, size_t fallbackViewCount)
{
    FrameInfo frameInfo{};
    frameInfo.views = snapshot.views;
    if (frameInfo.views.empty()) {
        frameInfo.views.resize(fallbackViewCount);
    }
    frameInfo.displayTime = snapshot.displayTime;
    frameInfo.frameNumber = snapshot.frameNumber;
    return frameInfo;
}

template <typename FrameInfoContainer>
std::optional<FrameInfo> findFrameInfoAtOrBeforeOrClosestBoundary(
    const FrameInfoContainer& frameInfos,
    varjo_Nanoseconds target)
{
    if (frameInfos.empty()) {
        return std::nullopt;
    }

    auto it = std::upper_bound(
        frameInfos.begin(),
        frameInfos.end(),
        target,
        [](varjo_Nanoseconds targetTime, const FrameInfo& frameInfo) {
            return targetTime < frameInfo.displayTime;
        });

    if (it == frameInfos.begin()) {
        FrameInfo out{};
        copyFrameInfo(*it, out);
        return out;
    }

    --it;
    FrameInfo out{};
    copyFrameInfo(*it, out);
    return out;
}

varjo_Vector3D toRightHandedPointFromGaze(const double p[3])
{
    return varjo_Vector3D{p[0], p[1], -p[2]};
}

varjo_Vector3D toRightHandedDirectionFromGaze(const double d[3])
{
    return varjo_Vector3D{d[0], d[1], -d[2]};
}

varjo_Vector3D makeGazePointInHeadSpaceRH(const varjo_Ray& ray, double distance)
{
    const varjo_Vector3D origin = toRightHandedPointFromGaze(ray.origin);
    const varjo_Vector3D forward = toRightHandedDirectionFromGaze(ray.forward);

    return varjo_Vector3D{
        origin.x + forward.x * distance,
        origin.y + forward.y * distance,
        origin.z + forward.z * distance
    };
}

bool isFinite2(const varjo_Vector2Df& p)
{
    return std::isfinite(p.x) && std::isfinite(p.y);
}

varjo_Vector2Df ndcToDisplayUv01(const varjo_Vector2Df& ndc)
{
    if (!isFinite2(ndc)) {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        return varjo_Vector2Df{nan, nan};
    }

    return varjo_Vector2Df{
        ndc.x * 0.5f + 0.5f,
        0.5f - ndc.y * 0.5f
    };
}

std::string optionalDoubleToCsv(const std::optional<double>& value)
{
    if (!value.has_value()) {
        return {};
    }
    return VarjoToolkit::Csv::number(value.value());
}

std::string optionalStringToCsv(const std::string& value)
{
    return value;
}

std::string emptyGazeCsv()
{
    return VarjoToolkit::Csv::emptyFields(25);
}

std::string emptyProjectedCsv()
{
    return VarjoToolkit::Csv::emptyFields(8);
}

size_t viewInfoCsvFieldCount()
{
    return 16 + 16 + 3;
}

size_t frameInfoCsvFieldCount(size_t viewCount)
{
    return viewCount * viewInfoCsvFieldCount() + 2;
}

std::string emptyFrameInfoCsv(size_t viewCount)
{
    return VarjoToolkit::Csv::emptyFields(frameInfoCsvFieldCount(viewCount));
}

std::string frameInfoHeaderCsv(const std::string& name, size_t viewCount)
{
    std::vector<std::string> fields;
    fields.reserve(viewCount + 1);
    for (size_t i = 0; i < viewCount; ++i) {
        fields.push_back(VarjoToolkit::Csv::headerForViewInfo(name + ".views[" + std::to_string(i) + "]"));
    }
    fields.push_back(VarjoToolkit::Csv::makeHeader(name, {"displayTime", "frameNumber"}));
    return VarjoToolkit::Csv::join(fields);
}

} // namespace

VarjoEyeTrackingProvider::VarjoEyeTrackingProvider(const std::shared_ptr<varjo_Session>& session)
    : session_(session)
    , frameInfos_(boost::circular_buffer<FrameInfo>(512))
    , viewCount_(session ? varjo_GetViewCount(session.get()) : 0)
{}

VarjoEyeTrackingProvider::~VarjoEyeTrackingProvider()
{
    shutdown();
}

void VarjoEyeTrackingProvider::initialize(OutputFilterType outputFilterType, OutputFrequency outputFrequency)
{
    if (this->getFrameInfoWorker_.joinable()) {
        return;
    }

    this->workerStopSignal_.store(false);
    this->getFrameInfoWorker_ = std::thread(&VarjoEyeTrackingProvider::getFrameInfoWorkerFunction, this);
    SetThreadPriority(static_cast<HANDLE>(this->getFrameInfoWorker_.native_handle()), THREAD_PRIORITY_LOWEST);

    while (!this->workerStopSignal_.load() && !this->frameInfos_.full()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    varjo_GazeParameters params[2];
    params[0].key = varjo_GazeParametersKey_OutputFilterType;
    switch (outputFilterType) {
    case OutputFilterType::NONE:
        params[0].value = varjo_GazeParametersValue_OutputFilterNone;
        break;
    case OutputFilterType::STANDARD:
    default:
        params[0].value = varjo_GazeParametersValue_OutputFilterStandard;
        break;
    }

    params[1].key = varjo_GazeParametersKey_OutputFrequency;
    switch (outputFrequency) {
    case OutputFrequency::_100HZ:
        params[1].value = varjo_GazeParametersValue_OutputFrequency100Hz;
        break;
    case OutputFrequency::_200HZ:
        params[1].value = varjo_GazeParametersValue_OutputFrequency200Hz;
        break;
    case OutputFrequency::MAXIMUM:
    default:
        params[1].value = varjo_GazeParametersValue_OutputFrequencyMaximumSupported;
        break;
    }

    varjo_GazeInitWithParameters(this->session_.get(), params, static_cast<int32_t>(std::size(params)));
}

void VarjoEyeTrackingProvider::shutdown()
{
    this->workerStopSignal_.store(true);
    if (this->getFrameInfoWorker_.joinable()) {
        this->getFrameInfoWorker_.join();
    }
}

VarjoEyeTrackingProvider::Status VarjoEyeTrackingProvider::getStatus() const
{
    varjo_SyncProperties(this->session_.get());

    if (!varjo_GetPropertyBool(this->session_.get(), varjo_PropertyKey_GazeAllowed)) {
        return Status::NOT_AVAILABLE;
    }
    if (!varjo_GetPropertyBool(this->session_.get(), varjo_PropertyKey_HMDConnected)) {
        return Status::NOT_CONNECTED;
    }
    if (varjo_GetPropertyBool(this->session_.get(), varjo_PropertyKey_GazeCalibrating)) {
        return Status::CALIBRATING;
    }
    if (varjo_GetPropertyBool(this->session_.get(), varjo_PropertyKey_GazeCalibrated)) {
        return Status::CALIBRATED;
    }
    return Status::NOT_CALIBRATED;
}

std::vector<std::pair<varjo_Gaze, varjo_EyeMeasurements>> VarjoEyeTrackingProvider::getGazeDataWithEyeMeasurements() const
{
    constexpr size_t c_growStep = 16;
    std::array<varjo_Gaze, c_growStep> gazeArray;
    std::array<varjo_EyeMeasurements, c_growStep> eyeMeasurementsArray;

    std::vector<std::pair<varjo_Gaze, varjo_EyeMeasurements>> output;
    int32_t newItems = 0;
    do {
        newItems = varjo_GetGazeDataArray(this->session_.get(), gazeArray.data(), eyeMeasurementsArray.data(), c_growStep);
        output.reserve(output.size() + static_cast<size_t>(newItems));
        for (int32_t i = 0; i < newItems; ++i) {
            output.push_back({gazeArray[i], eyeMeasurementsArray[i]});
        }
    } while (newItems == static_cast<int32_t>(c_growStep));

    return output;
}

std::optional<double> VarjoEyeTrackingProvider::getUserIPD() const
{
    varjo_SyncProperties(this->session_.get());
    const double estimate = varjo_GetPropertyDouble(this->session_.get(), varjo_PropertyKey_GazeIPDEstimate);
    return (estimate <= 0.0) ? std::nullopt : std::make_optional(estimate);
}

std::optional<double> VarjoEyeTrackingProvider::getHMDIPD() const
{
    varjo_SyncProperties(this->session_.get());
    const double positionInMM = varjo_GetPropertyDouble(this->session_.get(), varjo_PropertyKey_IPDPosition);
    return (positionInMM <= 0.0) ? std::nullopt : std::make_optional(positionInMM);
}

std::string VarjoEyeTrackingProvider::getIPDAdjustmentMode() const
{
    varjo_SyncProperties(this->session_.get());

    const uint32_t strSizeWithNullTerm = varjo_GetPropertyStringSize(this->session_.get(), varjo_PropertyKey_IPDAdjustmentMode);
    if (strSizeWithNullTerm <= 1) {
        return {};
    }

    std::vector<char> buffer(strSizeWithNullTerm);
    varjo_GetPropertyString(this->session_.get(), varjo_PropertyKey_IPDAdjustmentMode, buffer.data(), static_cast<uint32_t>(buffer.size()));
    return std::string(buffer.data());
}

std::optional<varjo_Gaze> VarjoEyeTrackingProvider::getRenderingGaze() const
{
    varjo_Gaze renderingGaze{};
    const auto ret = varjo_GetRenderingGaze(this->session_.get(), &renderingGaze);
    if (!ret) {
        return std::nullopt;
    }
    return renderingGaze;
}

std::vector<VarjoEyeTrackingData> VarjoEyeTrackingProvider::getEyeTrackingData()
{
    auto gaze_and_measurements = this->getGazeDataWithEyeMeasurements();
    if (gaze_and_measurements.empty()) {
        return {};
    }

    const auto userIPD = this->getUserIPD();
    const auto hmdIPD = this->getHMDIPD();
    const auto ipdAdjustmentMode = this->getIPDAdjustmentMode();
    const auto renderingGaze = this->getRenderingGaze();

    std::vector<FrameInfo> frameInfoCopy;
    std::optional<FrameInfo> renderingFrameInfo;
    {
        std::lock_guard lock(this->frameInfoMtx_);
        frameInfoCopy.assign(this->frameInfos_.begin(), this->frameInfos_.end());
        if (renderingGaze.has_value()) {
            renderingFrameInfo = findFrameInfoAtOrBeforeOrClosestBoundary(frameInfoCopy, renderingGaze.value().captureTime);
        }
    }

    if (frameInfoCopy.empty()) {
        frameInfoCopy.push_back(makeEmptyFrameInfo(static_cast<size_t>(this->viewCount_)));
    }

    std::optional<VarjoProjectedGazePosition> renderingGazePos_toVideo = std::nullopt;
    std::optional<VarjoProjectedGazePosition> renderingGazePos_toVarjoDisplay = std::nullopt;
    if (renderingGaze.has_value() && renderingFrameInfo.has_value()) {
        renderingGazePos_toVideo = this->calcProjectedGazePositionToVideo(renderingGaze.value(), renderingFrameInfo->views);
        renderingGazePos_toVarjoDisplay = this->calcProjectedGazePositionToVarjoDisplay(renderingGaze.value(), renderingFrameInfo->views);
    }

    std::vector<VarjoEyeTrackingData> datas(gaze_and_measurements.size());
    for (size_t i = 0; i < gaze_and_measurements.size(); ++i) {
        const auto& gaze = gaze_and_measurements[i].first;
        FrameInfo sampleFrameInfo = findFrameInfoAtOrBeforeOrClosestBoundary(frameInfoCopy, gaze.captureTime).value_or(frameInfoCopy.back());

        VarjoEyeTrackingData data{};
        data.gaze = gaze;
        data.measurements = gaze_and_measurements[i].second;
        data.userIPD = userIPD;
        data.hmdIPD = hmdIPD;
        data.ipdAdjustmentMode = ipdAdjustmentMode;
        data.renderingGaze = renderingGaze;
        data.gazePos_toVideo = this->calcProjectedGazePositionToVideo(gaze, sampleFrameInfo.views);
        data.renderingGazePos_toVideo = renderingGazePos_toVideo;
        data.gazePos_toVarjoDisplay = this->calcProjectedGazePositionToVarjoDisplay(gaze, sampleFrameInfo.views);
        data.renderingGazePos_toVarjoDisplay = renderingGazePos_toVarjoDisplay;
        data.frameInfo = std::move(sampleFrameInfo);
        data.renderingGazeFrameInfo = renderingFrameInfo;
        datas[i] = std::move(data);
    }

    return datas;
}

VarjoProjectedGazePosition VarjoEyeTrackingProvider::calcProjectedGazePositionToVarjoDisplay(
    const varjo_Gaze& gaze,
    const std::vector<varjo_ViewInfo>& viewInfo) const
{
    VarjoProjectedGazePosition out{};
    if (viewInfo.size() < 2) {
        return out;
    }

    out.leftEye = calcProjectedGazePositionToVarjoDisplayOneRay(gaze.leftEye, gaze.focusDistance, viewInfo, 0);
    out.rightEye = calcProjectedGazePositionToVarjoDisplayOneRay(gaze.rightEye, gaze.focusDistance, viewInfo, 1);
    out.combinedEyeToLeft = calcProjectedGazePositionToVarjoDisplayOneRay(gaze.gaze, gaze.focusDistance, viewInfo, 0);
    out.combinedEyeToRight = calcProjectedGazePositionToVarjoDisplayOneRay(gaze.gaze, gaze.focusDistance, viewInfo, 1);
    return out;
}

varjo_Vector2Df VarjoEyeTrackingProvider::calcProjectedGazePositionToVarjoDisplayOneRay(
    const varjo_Ray& gazeRay,
    double focusDistance,
    const std::vector<varjo_ViewInfo>& viewInfo,
    size_t targetViewIndex) const
{
    double distance = focusDistance;
    if (!(distance > 0.01 && distance <= 2.0)) {
        distance = 1.0;
    }

    if (viewInfo.size() <= targetViewIndex || viewInfo.size() < 2) {
        return {0.0f, 0.0f};
    }

    const varjo_Vector3D pointHeadRH = makeGazePointInHeadSpaceRH(gazeRay, distance);
    const varjo_Vector3D pointWorld = transformHeadPointToWorldUsingFrameInfo(viewInfo, pointHeadRH);

    const Vec4 pWorld{pointWorld.x, pointWorld.y, pointWorld.z, 1.0};
    const varjo_ViewInfo& targetView = viewInfo[targetViewIndex];
    const Vec4 pEye = mulMat4Vec4(targetView.viewMatrix, pWorld);
    const Vec4 pClip = mulMat4Vec4(targetView.projectionMatrix, pEye);

    if (std::abs(pClip.w) < 1e-12) {
        return {0.0f, 0.0f};
    }

    varjo_Vector2Df out{};
    out.x = static_cast<float>(pClip.x / pClip.w);
    out.y = static_cast<float>(pClip.y / pClip.w);
    if (std::isfinite(out.x) && std::isfinite(out.y)) {
        return out;
    }
    return {0.0f, 0.0f};
}

VarjoProjectedGazePosition VarjoEyeTrackingProvider::calcProjectedGazePositionToVideo(
    const varjo_Gaze& gaze,
    const std::vector<varjo_ViewInfo>& viewInfo) const
{
    const VarjoProjectedGazePosition ndc = this->calcProjectedGazePositionToVarjoDisplay(gaze, viewInfo);

    VarjoProjectedGazePosition result{};
    result.leftEye = ndcToDisplayUv01(ndc.leftEye);
    result.rightEye = ndcToDisplayUv01(ndc.rightEye);
    result.combinedEyeToLeft = ndcToDisplayUv01(ndc.combinedEyeToLeft);
    result.combinedEyeToRight = ndcToDisplayUv01(ndc.combinedEyeToRight);
    return result;
}

FrameInfo VarjoEyeTrackingProvider::requestFrameInfo()
{
    VarjoFrameInfo frameInfo(this->session_);
    if (!frameInfo || !frameInfo.waitSync()) {
        return makeEmptyFrameInfo(static_cast<size_t>(this->viewCount_));
    }

    return makeFrameInfoFromSnapshot(frameInfo.snapshot(), static_cast<size_t>(this->viewCount_));
}

void VarjoEyeTrackingProvider::getFrameInfoWorkerFunction()
{
    setCurrentThreadLowPriorityForWaitSync();

    while (!this->workerStopSignal_.load()) {
        auto frameInfo = this->requestFrameInfo();
        {
            std::lock_guard lock(this->frameInfoMtx_);
            this->frameInfos_.push_back(std::move(frameInfo));
        }
    }
}

VarjoEyeTrackingDataLogger::VarjoEyeTrackingDataLogger(const std::string& filepath, std::shared_ptr<varjo_Session> session)
    : session_(std::move(session))
    , filepath_(filepath)
    , viewCount_(varjo_GetViewCount(session_.get()))
{}

VarjoEyeTrackingDataLogger::~VarjoEyeTrackingDataLogger()
{
    close();
}

bool VarjoEyeTrackingDataLogger::open()
{
    if (this->logfile_.is_open()) {
        return false;
    }

    this->logfile_.open(this->filepath_.string());
    if (this->logfile_.is_open()) {
        this->logfile_ << this->getHeaderCsvString();
    }
    return this->logfile_.is_open();
}

void VarjoEyeTrackingDataLogger::close()
{
    if (!this->logfile_.is_open()) {
        return;
    }
    this->logfile_.close();
}

void VarjoEyeTrackingDataLogger::write(const VarjoEyeTrackingData& data)
{
    this->logfile_ << this->varjoEyeTrackingDataToCsvString(data);
}

std::string VarjoEyeTrackingDataLogger::getHeaderCsvString()
{
    return VarjoToolkit::Csv::join({
        VarjoToolkit::Csv::headerForGaze("gaze"),
        VarjoToolkit::Csv::headerForEyeMeasurements("measurements"),
        "userIPD,hmdIPD,ipdAdjustmentMode",
        VarjoToolkit::Csv::headerForGaze("renderingGaze"),
        VarjoToolkit::Csv::headerForProjectedGazePosition("gazePos_toVideo"),
        VarjoToolkit::Csv::headerForProjectedGazePosition("renderingGazePos_toVideo"),
        VarjoToolkit::Csv::headerForProjectedGazePosition("gazePos_toVarjoDisplay"),
        VarjoToolkit::Csv::headerForProjectedGazePosition("renderingGazePos_toVarjoDisplay"),
        frameInfoHeaderCsv("frameInfo", static_cast<size_t>(viewCount_)),
        frameInfoHeaderCsv("renderingGazeFrameInfo", static_cast<size_t>(viewCount_))
    }) + "\n";
}

std::string VarjoEyeTrackingDataLogger::varjoEyeTrackingDataToCsvString(const VarjoEyeTrackingData& data)
{
    return VarjoToolkit::Csv::join({
        varjoGazeToCsvString(data.gaze),
        varjoEyeMeasurementsToCsvString(data.measurements),
        optionalDoubleToCsv(data.userIPD),
        optionalDoubleToCsv(data.hmdIPD),
        optionalStringToCsv(data.ipdAdjustmentMode),
        data.renderingGaze.has_value() ? varjoGazeToCsvString(data.renderingGaze.value()) : emptyGazeCsv(),
        varjoProjectedGazePositionToCsvString(data.gazePos_toVideo),
        data.renderingGazePos_toVideo.has_value() ? varjoProjectedGazePositionToCsvString(data.renderingGazePos_toVideo.value()) : emptyProjectedCsv(),
        varjoProjectedGazePositionToCsvString(data.gazePos_toVarjoDisplay),
        data.renderingGazePos_toVarjoDisplay.has_value() ? varjoProjectedGazePositionToCsvString(data.renderingGazePos_toVarjoDisplay.value()) : emptyProjectedCsv(),
        frameInfoToCsvString(data.frameInfo),
        data.renderingGazeFrameInfo.has_value() ? frameInfoToCsvString(data.renderingGazeFrameInfo.value()) : emptyFrameInfoCsv(static_cast<size_t>(viewCount_))
    }) + "\n";
}

std::string VarjoEyeTrackingDataLogger::varjoGazeToCsvString(const varjo_Gaze& gaze)
{
    return VarjoToolkit::Csv::toCsv(gaze);
}

std::string VarjoEyeTrackingDataLogger::varjoEyeMeasurementsToCsvString(const varjo_EyeMeasurements& measurements)
{
    return VarjoToolkit::Csv::toCsv(measurements);
}

std::string VarjoEyeTrackingDataLogger::varjoProjectedGazePositionToCsvString(const VarjoProjectedGazePosition& gazePos)
{
    return VarjoToolkit::Csv::toCsv(gazePos);
}

std::string VarjoEyeTrackingDataLogger::frameInfoToCsvString(const FrameInfo& frameInfo)
{
    std::vector<std::string> fields;
    fields.reserve(static_cast<size_t>(viewCount_) + 1);

    for (int i = 0; i < viewCount_; ++i) {
        if (i >= 0 && static_cast<size_t>(i) < frameInfo.views.size()) {
            fields.push_back(VarjoToolkit::Csv::toCsv(frameInfo.views[static_cast<size_t>(i)]));
        } else {
            fields.push_back(VarjoToolkit::Csv::emptyFields(viewInfoCsvFieldCount()));
        }
    }

    fields.push_back(VarjoToolkit::Csv::join({
        std::to_string(frameInfo.displayTime),
        std::to_string(frameInfo.frameNumber)
    }));
    return VarjoToolkit::Csv::join(fields);
}

VarjoEyeTrackingService::VarjoEyeTrackingService(
    const std::shared_ptr<varjo_Session>& session,
    const VarjoEyeTrackingProvider::OutputFilterType outputFilterType,
    const VarjoEyeTrackingProvider::OutputFrequency outputFrequency,
    const std::string& filepath,
    const size_t queueSize,
    const int acquireFrequencyMs)
    : session_(session)
    , outputFilterType_(outputFilterType)
    , outputFrequency_(outputFrequency)
    , eyeTracker_(session)
    , logger_(filepath, session)
    , dataQueueMaxSize_(queueSize)
    , acquireFrequencyMs_(acquireFrequencyMs)
{}

bool VarjoEyeTrackingService::start()
{
    if (!logger_.open()) {
        return false;
    }

    this->threadEndSignal_.store(false);
    this->eyeTracker_.initialize(this->outputFilterType_, this->outputFrequency_);
    this->dataRequestThread_ = std::thread(&VarjoEyeTrackingService::dataRequestWorker, this);
    return true;
}

void VarjoEyeTrackingService::stop()
{
    this->threadEndSignal_.store(true);
    if (this->dataRequestThread_.joinable()) {
        this->dataRequestThread_.join();
    }
    this->eyeTracker_.shutdown();
    this->logger_.close();
}

std::deque<VarjoEyeTrackingData> VarjoEyeTrackingService::requestData()
{
    std::deque<VarjoEyeTrackingData> out;
    {
        std::lock_guard<std::mutex> lock(this->dataQueueMutex_);
        out.swap(this->dataQueue_);
    }
    return out;
}

void VarjoEyeTrackingService::dataRequestWorker()
{
    while (!this->threadEndSignal_.load()) {
        auto datas = this->eyeTracker_.getEyeTrackingData();
        if (!datas.empty()) {
            std::lock_guard<std::mutex> lock(this->dataQueueMutex_);
            for (const auto& data : datas) {
                this->logger_.write(data);
                this->dataQueue_.push_back(data);
                while (this->dataQueue_.size() > this->dataQueueMaxSize_) {
                    this->dataQueue_.pop_front();
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(this->acquireFrequencyMs_));
    }
}
