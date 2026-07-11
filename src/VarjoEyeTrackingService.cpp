#include <VarjoToolkit/Services/EyeTracking/VarjoEyeTrackingService.hpp>

#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>
#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
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

bool shouldLogCounter(uint64_t value)
{
    return value <= 3 || ((value & (value - 1)) == 0);
}

const char* outputFilterTypeName(VarjoEyeTrackingProvider::OutputFilterType value)
{
    switch (value) {
    case VarjoEyeTrackingProvider::OutputFilterType::NONE: return "NONE";
    case VarjoEyeTrackingProvider::OutputFilterType::STANDARD: return "STANDARD";
    default: return "UNKNOWN";
    }
}

const char* outputFrequencyName(VarjoEyeTrackingProvider::OutputFrequency value)
{
    switch (value) {
    case VarjoEyeTrackingProvider::OutputFrequency::MAXIMUM: return "MAXIMUM";
    case VarjoEyeTrackingProvider::OutputFrequency::_100HZ: return "100HZ";
    case VarjoEyeTrackingProvider::OutputFrequency::_200HZ: return "200HZ";
    default: return "UNKNOWN";
    }
}

const char* gazeStatusName(VarjoEyeTrackingProvider::Status value)
{
    switch (value) {
    case VarjoEyeTrackingProvider::Status::NOT_AVAILABLE: return "NOT_AVAILABLE";
    case VarjoEyeTrackingProvider::Status::NOT_CONNECTED: return "NOT_CONNECTED";
    case VarjoEyeTrackingProvider::Status::NOT_CALIBRATED: return "NOT_CALIBRATED";
    case VarjoEyeTrackingProvider::Status::CALIBRATING: return "CALIBRATING";
    case VarjoEyeTrackingProvider::Status::CALIBRATED: return "CALIBRATED";
    default: return "UNKNOWN";
    }
}

Vec3 addVec3(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 mulVec3(const Vec3& value, double scalar)
{
    return Vec3{value.x * scalar, value.y * scalar, value.z * scalar};
}

double dotVec3(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 crossVec3(const Vec3& a, const Vec3& b)
{
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

double lengthVec3(const Vec3& value)
{
    return std::sqrt(dotVec3(value, value));
}

Vec3 normalizeVec3(const Vec3& value, const Vec3& fallback)
{
    const double length = lengthVec3(value);
    return length > 1.0e-9 ? mulVec3(value, 1.0 / length) : fallback;
}

Vec3 toVec3(const varjo_Vector3D& value)
{
    return Vec3{value.x, value.y, value.z};
}

varjo_Vector3D toVarjoVector3D(const Vec3& value)
{
    return varjo_Vector3D{value.x, value.y, value.z};
}

Vec4 mulMat4Vec4(const double matrix[16], const Vec4& value)
{
    return Vec4{
        matrix[0] * value.x + matrix[4] * value.y + matrix[8] * value.z + matrix[12] * value.w,
        matrix[1] * value.x + matrix[5] * value.y + matrix[9] * value.z + matrix[13] * value.w,
        matrix[2] * value.x + matrix[6] * value.y + matrix[10] * value.z + matrix[14] * value.w,
        matrix[3] * value.x + matrix[7] * value.y + matrix[11] * value.z + matrix[15] * value.w};
}

Vec3 viewMatrixEyeOriginWorld(const double view[16])
{
    const double tx = view[12];
    const double ty = view[13];
    const double tz = view[14];
    return Vec3{
        -(view[0] * tx + view[1] * ty + view[2] * tz),
        -(view[4] * tx + view[5] * ty + view[6] * tz),
        -(view[8] * tx + view[9] * ty + view[10] * tz)};
}

Vec3 transformViewVectorToWorld(const double view[16], const Vec3& value)
{
    return Vec3{
        view[0] * value.x + view[1] * value.y + view[2] * value.z,
        view[4] * value.x + view[5] * value.y + view[6] * value.z,
        view[8] * value.x + view[9] * value.y + view[10] * value.z};
}

bool reconstructHeadBasisFromFrameInfoViews(
    const std::vector<varjo_ViewInfo>& views,
    Vec3& centerWorld,
    Vec3& rightWorld,
    Vec3& upWorld,
    Vec3& forwardWorld,
    Vec3& positiveZWorld)
{
    if (views.size() < 2) return false;

    centerWorld = mulVec3(
        addVec3(
            viewMatrixEyeOriginWorld(views[0].viewMatrix),
            viewMatrixEyeOriginWorld(views[1].viewMatrix)),
        0.5);

    rightWorld = normalizeVec3(
        addVec3(
            transformViewVectorToWorld(views[0].viewMatrix, Vec3{1.0, 0.0, 0.0}),
            transformViewVectorToWorld(views[1].viewMatrix, Vec3{1.0, 0.0, 0.0})),
        Vec3{1.0, 0.0, 0.0});
    upWorld = normalizeVec3(
        addVec3(
            transformViewVectorToWorld(views[0].viewMatrix, Vec3{0.0, 1.0, 0.0}),
            transformViewVectorToWorld(views[1].viewMatrix, Vec3{0.0, 1.0, 0.0})),
        Vec3{0.0, 1.0, 0.0});
    forwardWorld = normalizeVec3(
        addVec3(
            transformViewVectorToWorld(views[0].viewMatrix, Vec3{0.0, 0.0, -1.0}),
            transformViewVectorToWorld(views[1].viewMatrix, Vec3{0.0, 0.0, -1.0})),
        Vec3{0.0, 0.0, -1.0});

    rightWorld = normalizeVec3(
        rightWorld,
        normalizeVec3(crossVec3(forwardWorld, upWorld), Vec3{1.0, 0.0, 0.0}));
    upWorld = normalizeVec3(crossVec3(rightWorld, forwardWorld), upWorld);
    positiveZWorld = normalizeVec3(mulVec3(forwardWorld, -1.0), Vec3{0.0, 0.0, 1.0});
    return true;
}

varjo_Vector3D transformHeadPointToWorldUsingFrameInfo(
    const std::vector<varjo_ViewInfo>& views,
    const varjo_Vector3D& pointHeadRH)
{
    Vec3 center{};
    Vec3 right{};
    Vec3 up{};
    Vec3 forward{};
    Vec3 positiveZ{};
    if (!reconstructHeadBasisFromFrameInfoViews(
            views,
            center,
            right,
            up,
            forward,
            positiveZ)) {
        return pointHeadRH;
    }

    const Vec3 point = toVec3(pointHeadRH);
    return toVarjoVector3D(addVec3(
        addVec3(
            addVec3(center, mulVec3(right, point.x)),
            mulVec3(up, point.y)),
        mulVec3(positiveZ, point.z)));
}

void copyViewInfo(const varjo_ViewInfo& source, varjo_ViewInfo& destination)
{
    std::copy(
        std::begin(source.projectionMatrix),
        std::end(source.projectionMatrix),
        std::begin(destination.projectionMatrix));
    std::copy(
        std::begin(source.viewMatrix),
        std::end(source.viewMatrix),
        std::begin(destination.viewMatrix));
    destination.preferredWidth = source.preferredWidth;
    destination.preferredHeight = source.preferredHeight;
    destination.enabled = source.enabled;
    destination.reserved = source.reserved;
}

void copyFrameInfo(const FrameInfo& source, FrameInfo& destination)
{
    destination.views.resize(source.views.size());
    for (size_t index = 0; index < source.views.size(); ++index) {
        copyViewInfo(source.views[index], destination.views[index]);
    }
    destination.displayTime = source.displayTime;
    destination.frameNumber = source.frameNumber;
}

FrameInfo makeEmptyFrameInfo(size_t viewCount)
{
    FrameInfo result{};
    result.views.resize(viewCount);
    return result;
}

FrameInfo makeFrameInfoFromSnapshot(
    const VarjoFrameInfoSnapshot& snapshot,
    size_t fallbackViewCount)
{
    FrameInfo result{};
    result.views = snapshot.views;
    if (result.views.empty()) result.views.resize(fallbackViewCount);
    result.displayTime = snapshot.displayTime;
    result.frameNumber = snapshot.frameNumber;
    return result;
}

template <typename FrameInfoContainer>
std::optional<FrameInfo> findFrameInfoAtOrBeforeOrClosestBoundary(
    const FrameInfoContainer& frameInfos,
    varjo_Nanoseconds target)
{
    if (frameInfos.empty()) return std::nullopt;

    auto iterator = std::upper_bound(
        frameInfos.begin(),
        frameInfos.end(),
        target,
        [](varjo_Nanoseconds targetTime, const FrameInfo& frameInfo) {
            return targetTime < frameInfo.displayTime;
        });

    if (iterator == frameInfos.begin()) {
        FrameInfo result{};
        copyFrameInfo(*iterator, result);
        return result;
    }

    --iterator;
    FrameInfo result{};
    copyFrameInfo(*iterator, result);
    return result;
}

varjo_Vector3D toRightHandedPointFromGaze(const double value[3])
{
    return varjo_Vector3D{value[0], value[1], -value[2]};
}

varjo_Vector3D toRightHandedDirectionFromGaze(const double value[3])
{
    return varjo_Vector3D{value[0], value[1], -value[2]};
}

varjo_Vector3D makeGazePointInHeadSpaceRH(const varjo_Ray& ray, double distance)
{
    const varjo_Vector3D origin = toRightHandedPointFromGaze(ray.origin);
    const varjo_Vector3D forward = toRightHandedDirectionFromGaze(ray.forward);
    return varjo_Vector3D{
        origin.x + forward.x * distance,
        origin.y + forward.y * distance,
        origin.z + forward.z * distance};
}

bool isFinite2(const varjo_Vector2Df& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

varjo_Vector2Df ndcToDisplayUv01(const varjo_Vector2Df& ndc)
{
    if (!isFinite2(ndc)) {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        return varjo_Vector2Df{nan, nan};
    }
    return varjo_Vector2Df{ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f};
}

std::string optionalDoubleToCsv(const std::optional<double>& value)
{
    return value.has_value() ? VarjoToolkit::Csv::number(*value) : std::string{};
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
    for (size_t index = 0; index < viewCount; ++index) {
        fields.push_back(VarjoToolkit::Csv::headerForViewInfo(
            name + ".views[" + std::to_string(index) + "]"));
    }
    fields.push_back(VarjoToolkit::Csv::makeHeader(
        name,
        {"displayTime", "frameNumber"}));
    return VarjoToolkit::Csv::join(fields);
}

} // namespace

VarjoEyeTrackingProvider::VarjoEyeTrackingProvider(
    const std::shared_ptr<varjo_Session>& session)
    : session_(session)
    , viewCount_(session ? varjo_GetViewCount(session.get()) : 0)
{
    VTK_SD_LOG("VarjoEyeTrackingProvider constructor session=" << session_.get()
        << " viewCount=" << viewCount_);
}

VarjoEyeTrackingProvider::~VarjoEyeTrackingProvider()
{
    shutdown();
}

void VarjoEyeTrackingProvider::initialize(
    OutputFilterType outputFilterType,
    OutputFrequency outputFrequency)
{
    VTK_SD_LOG("VarjoEyeTrackingProvider::initialize filter="
        << outputFilterTypeName(outputFilterType)
        << " frequency=" << outputFrequencyName(outputFrequency));

    {
        std::lock_guard<std::mutex> lock(frameInfoMtx_);
        frameInfos_.clear();
        submittedFrameInfoCount_ = 0;
        droppedFrameInfoCount_ = 0;
    }

    if (!session_) {
        VTK_SD_ERROR("VarjoEyeTrackingProvider initialize failed: session is null");
        return;
    }

    varjo_GazeParameters parameters[2]{};
    parameters[0].key = varjo_GazeParametersKey_OutputFilterType;
    parameters[0].value = outputFilterType == OutputFilterType::NONE
        ? varjo_GazeParametersValue_OutputFilterNone
        : varjo_GazeParametersValue_OutputFilterStandard;

    parameters[1].key = varjo_GazeParametersKey_OutputFrequency;
    switch (outputFrequency) {
    case OutputFrequency::_100HZ:
        parameters[1].value = varjo_GazeParametersValue_OutputFrequency100Hz;
        break;
    case OutputFrequency::_200HZ:
        parameters[1].value = varjo_GazeParametersValue_OutputFrequency200Hz;
        break;
    case OutputFrequency::MAXIMUM:
    default:
        parameters[1].value = varjo_GazeParametersValue_OutputFrequencyMaximumSupported;
        break;
    }

    varjo_GazeInitWithParameters(
        session_.get(),
        parameters,
        static_cast<int32_t>(std::size(parameters)));
    VTK_SD_LOG("VarjoEyeTrackingProvider initialized status="
        << gazeStatusName(getStatus()));
}

void VarjoEyeTrackingProvider::shutdown()
{
    std::lock_guard<std::mutex> lock(frameInfoMtx_);
    frameInfos_.clear();
}

bool VarjoEyeTrackingProvider::submitFrameInfo(
    const VarjoFrameInfoSnapshot& snapshot)
{
    if (!snapshot.valid || snapshot.views.empty() || snapshot.displayTime <= 0) {
        VTK_SD_WARN("VarjoEyeTrackingProvider rejected invalid external frame info");
        return false;
    }

    FrameInfo frameInfo = makeFrameInfoFromSnapshot(
        snapshot,
        static_cast<size_t>(viewCount_));

    std::lock_guard<std::mutex> lock(frameInfoMtx_);
    if (!frameInfos_.empty() &&
        frameInfo.displayTime < frameInfos_.back().displayTime) {
        const auto insertionPoint = std::upper_bound(
            frameInfos_.begin(),
            frameInfos_.end(),
            frameInfo.displayTime,
            [](varjo_Nanoseconds displayTime, const FrameInfo& item) {
                return displayTime < item.displayTime;
            });
        frameInfos_.insert(insertionPoint, std::move(frameInfo));
    } else {
        frameInfos_.push_back(std::move(frameInfo));
    }

    ++submittedFrameInfoCount_;
    while (frameInfos_.size() > frameInfoCapacity_) {
        frameInfos_.pop_front();
        ++droppedFrameInfoCount_;
        if (shouldLogCounter(droppedFrameInfoCount_)) {
            VTK_SD_WARN("VarjoEyeTrackingProvider frame history dropped oldest totalDropped="
                << droppedFrameInfoCount_);
        }
    }
    return true;
}

uint64_t VarjoEyeTrackingProvider::submittedFrameInfoCount() const noexcept
{
    std::lock_guard<std::mutex> lock(frameInfoMtx_);
    return submittedFrameInfoCount_;
}

uint64_t VarjoEyeTrackingProvider::droppedFrameInfoCount() const noexcept
{
    std::lock_guard<std::mutex> lock(frameInfoMtx_);
    return droppedFrameInfoCount_;
}

VarjoEyeTrackingProvider::Status VarjoEyeTrackingProvider::getStatus() const
{
    if (!session_) return Status::NOT_CONNECTED;
    varjo_SyncProperties(session_.get());
    if (!varjo_GetPropertyBool(session_.get(), varjo_PropertyKey_GazeAllowed)) {
        return Status::NOT_AVAILABLE;
    }
    if (!varjo_GetPropertyBool(session_.get(), varjo_PropertyKey_HMDConnected)) {
        return Status::NOT_CONNECTED;
    }
    if (varjo_GetPropertyBool(session_.get(), varjo_PropertyKey_GazeCalibrating)) {
        return Status::CALIBRATING;
    }
    if (varjo_GetPropertyBool(session_.get(), varjo_PropertyKey_GazeCalibrated)) {
        return Status::CALIBRATED;
    }
    return Status::NOT_CALIBRATED;
}

std::vector<std::pair<varjo_Gaze, varjo_EyeMeasurements>>
VarjoEyeTrackingProvider::getGazeDataWithEyeMeasurements() const
{
    constexpr size_t growStep = 16;
    std::array<varjo_Gaze, growStep> gazeArray{};
    std::array<varjo_EyeMeasurements, growStep> measurementArray{};

    std::vector<std::pair<varjo_Gaze, varjo_EyeMeasurements>> output;
    int32_t newItems = 0;
    do {
        newItems = varjo_GetGazeDataArray(
            session_.get(),
            gazeArray.data(),
            measurementArray.data(),
            static_cast<int32_t>(growStep));
        output.reserve(output.size() + static_cast<size_t>(newItems));
        for (int32_t index = 0; index < newItems; ++index) {
            output.push_back({gazeArray[index], measurementArray[index]});
        }
    } while (newItems == static_cast<int32_t>(growStep));
    return output;
}

std::optional<double> VarjoEyeTrackingProvider::getUserIPD() const
{
    varjo_SyncProperties(session_.get());
    const double value = varjo_GetPropertyDouble(
        session_.get(),
        varjo_PropertyKey_GazeIPDEstimate);
    return value > 0.0 ? std::make_optional(value) : std::nullopt;
}

std::optional<double> VarjoEyeTrackingProvider::getHMDIPD() const
{
    varjo_SyncProperties(session_.get());
    const double value = varjo_GetPropertyDouble(
        session_.get(),
        varjo_PropertyKey_IPDPosition);
    return value > 0.0 ? std::make_optional(value) : std::nullopt;
}

std::string VarjoEyeTrackingProvider::getIPDAdjustmentMode() const
{
    varjo_SyncProperties(session_.get());
    const uint32_t size = varjo_GetPropertyStringSize(
        session_.get(),
        varjo_PropertyKey_IPDAdjustmentMode);
    if (size <= 1) return {};

    std::vector<char> buffer(size);
    varjo_GetPropertyString(
        session_.get(),
        varjo_PropertyKey_IPDAdjustmentMode,
        buffer.data(),
        static_cast<uint32_t>(buffer.size()));
    return std::string(buffer.data());
}

std::optional<varjo_Gaze> VarjoEyeTrackingProvider::getRenderingGaze() const
{
    varjo_Gaze renderingGaze{};
    return varjo_GetRenderingGaze(session_.get(), &renderingGaze)
        ? std::make_optional(renderingGaze)
        : std::nullopt;
}

std::vector<VarjoEyeTrackingData>
VarjoEyeTrackingProvider::getEyeTrackingData()
{
    auto gazeAndMeasurements = getGazeDataWithEyeMeasurements();
    if (gazeAndMeasurements.empty()) return {};

    const auto userIPD = getUserIPD();
    const auto hmdIPD = getHMDIPD();
    const auto ipdAdjustmentMode = getIPDAdjustmentMode();
    const auto renderingGaze = getRenderingGaze();

    const varjo_Nanoseconds firstCaptureTime =
        gazeAndMeasurements.front().first.captureTime;
    const varjo_Nanoseconds lastCaptureTime =
        gazeAndMeasurements.back().first.captureTime;
    const varjo_Nanoseconds rangeBeginTime =
        std::min(firstCaptureTime, lastCaptureTime);
    const varjo_Nanoseconds rangeEndTime =
        std::max(firstCaptureTime, lastCaptureTime);

    std::vector<FrameInfo> frameInfoCopy;
    size_t sourceHistorySize = 0;
    {
        std::lock_guard<std::mutex> lock(frameInfoMtx_);
        sourceHistorySize = frameInfos_.size();
        if (!frameInfos_.empty()) {
            const auto comparator =
                [](varjo_Nanoseconds targetTime, const FrameInfo& frameInfo) {
                    return targetTime < frameInfo.displayTime;
                };

            auto rangeBegin = std::upper_bound(
                frameInfos_.begin(),
                frameInfos_.end(),
                rangeBeginTime,
                comparator);
            if (rangeBegin != frameInfos_.begin()) {
                --rangeBegin;
            }

            auto rangeEnd = std::upper_bound(
                frameInfos_.begin(),
                frameInfos_.end(),
                rangeEndTime,
                comparator);
            if (rangeEnd == frameInfos_.begin()) {
                rangeEnd = frameInfos_.begin();
            } else {
                --rangeEnd;
            }

            for (auto iterator = rangeBegin;; ++iterator) {
                FrameInfo copiedFrameInfo{};
                copyFrameInfo(*iterator, copiedFrameInfo);
                frameInfoCopy.push_back(std::move(copiedFrameInfo));
                if (iterator == rangeEnd) break;
            }
        }
    }

    VTK_SD_TRACE("VarjoEyeTrackingProvider copied required frame history firstCaptureTime="
        << firstCaptureTime
        << " lastCaptureTime=" << lastCaptureTime
        << " sourceHistorySize=" << sourceHistorySize
        << " copiedFrameCount=" << frameInfoCopy.size());

    if (frameInfoCopy.empty()) {
        frameInfoCopy.push_back(makeEmptyFrameInfo(static_cast<size_t>(viewCount_)));
    }

    std::optional<FrameInfo> renderingFrameInfo;
    if (renderingGaze.has_value()) {
        renderingFrameInfo = findFrameInfoAtOrBeforeOrClosestBoundary(
            frameInfoCopy,
            renderingGaze->captureTime);
    }

    std::optional<VarjoProjectedGazePosition> renderingVideoPosition;
    std::optional<VarjoProjectedGazePosition> renderingDisplayPosition;
    if (renderingGaze.has_value() && renderingFrameInfo.has_value()) {
        renderingVideoPosition = calcProjectedGazePositionToVideo(
            *renderingGaze,
            renderingFrameInfo->views);
        renderingDisplayPosition = calcProjectedGazePositionToVarjoDisplay(
            *renderingGaze,
            renderingFrameInfo->views);
    }

    std::vector<VarjoEyeTrackingData> output(gazeAndMeasurements.size());
    for (size_t index = 0; index < gazeAndMeasurements.size(); ++index) {
        const auto& gaze = gazeAndMeasurements[index].first;
        FrameInfo sampleFrameInfo =
            findFrameInfoAtOrBeforeOrClosestBoundary(
                frameInfoCopy,
                gaze.captureTime).value_or(frameInfoCopy.back());

        VarjoEyeTrackingData data{};
        data.gaze = gaze;
        data.measurements = gazeAndMeasurements[index].second;
        data.userIPD = userIPD;
        data.hmdIPD = hmdIPD;
        data.ipdAdjustmentMode = ipdAdjustmentMode;
        data.renderingGaze = renderingGaze;
        data.gazePos_toVideo = calcProjectedGazePositionToVideo(
            gaze,
            sampleFrameInfo.views);
        data.renderingGazePos_toVideo = renderingVideoPosition;
        data.gazePos_toVarjoDisplay = calcProjectedGazePositionToVarjoDisplay(
            gaze,
            sampleFrameInfo.views);
        data.renderingGazePos_toVarjoDisplay = renderingDisplayPosition;
        data.frameInfo = std::move(sampleFrameInfo);
        data.renderingGazeFrameInfo = renderingFrameInfo;
        output[index] = std::move(data);
    }
    return output;
}

VarjoProjectedGazePosition
VarjoEyeTrackingProvider::calcProjectedGazePositionToVarjoDisplay(
    const varjo_Gaze& gaze,
    const std::vector<varjo_ViewInfo>& viewInfo) const
{
    VarjoProjectedGazePosition output{};
    if (viewInfo.size() < 2) return output;

    output.leftEye = calcProjectedGazePositionToVarjoDisplayOneRay(
        gaze.leftEye,
        gaze.focusDistance,
        viewInfo,
        0);
    output.rightEye = calcProjectedGazePositionToVarjoDisplayOneRay(
        gaze.rightEye,
        gaze.focusDistance,
        viewInfo,
        1);
    output.combinedEyeToLeft = calcProjectedGazePositionToVarjoDisplayOneRay(
        gaze.gaze,
        gaze.focusDistance,
        viewInfo,
        0);
    output.combinedEyeToRight = calcProjectedGazePositionToVarjoDisplayOneRay(
        gaze.gaze,
        gaze.focusDistance,
        viewInfo,
        1);
    return output;
}

varjo_Vector2Df
VarjoEyeTrackingProvider::calcProjectedGazePositionToVarjoDisplayOneRay(
    const varjo_Ray& gazeRay,
    double focusDistance,
    const std::vector<varjo_ViewInfo>& viewInfo,
    size_t targetViewIndex) const
{
    double distance = focusDistance;
    if (!(distance > 0.01 && distance <= 2.0)) distance = 1.0;
    if (viewInfo.size() < 2 || viewInfo.size() <= targetViewIndex) {
        return {0.0f, 0.0f};
    }

    const varjo_Vector3D pointHead = makeGazePointInHeadSpaceRH(
        gazeRay,
        distance);
    const varjo_Vector3D pointWorld = transformHeadPointToWorldUsingFrameInfo(
        viewInfo,
        pointHead);
    const Vec4 eye = mulMat4Vec4(
        viewInfo[targetViewIndex].viewMatrix,
        Vec4{pointWorld.x, pointWorld.y, pointWorld.z, 1.0});
    const Vec4 clip = mulMat4Vec4(
        viewInfo[targetViewIndex].projectionMatrix,
        eye);

    if (std::abs(clip.w) < 1.0e-12) return {0.0f, 0.0f};
    const varjo_Vector2Df output{
        static_cast<float>(clip.x / clip.w),
        static_cast<float>(clip.y / clip.w)};
    return isFinite2(output) ? output : varjo_Vector2Df{0.0f, 0.0f};
}

VarjoProjectedGazePosition
VarjoEyeTrackingProvider::calcProjectedGazePositionToVideo(
    const varjo_Gaze& gaze,
    const std::vector<varjo_ViewInfo>& viewInfo) const
{
    const auto ndc = calcProjectedGazePositionToVarjoDisplay(gaze, viewInfo);
    return VarjoProjectedGazePosition{
        ndcToDisplayUv01(ndc.leftEye),
        ndcToDisplayUv01(ndc.rightEye),
        ndcToDisplayUv01(ndc.combinedEyeToLeft),
        ndcToDisplayUv01(ndc.combinedEyeToRight)};
}

VarjoEyeTrackingDataLogger::VarjoEyeTrackingDataLogger(
    const std::string& filepath,
    std::shared_ptr<varjo_Session> session)
    : session_(std::move(session))
    , filepath_(filepath)
    , viewCount_(session_ ? varjo_GetViewCount(session_.get()) : 0)
{}

VarjoEyeTrackingDataLogger::~VarjoEyeTrackingDataLogger()
{
    close();
}

bool VarjoEyeTrackingDataLogger::open()
{
    if (logfile_.is_open()) return false;
    logfile_.open(filepath_, std::ios::out | std::ios::trunc);
    if (logfile_.is_open()) logfile_ << getHeaderCsvString();
    return logfile_.is_open();
}

void VarjoEyeTrackingDataLogger::close()
{
    if (logfile_.is_open()) {
        logfile_.flush();
        logfile_.close();
    }
}

void VarjoEyeTrackingDataLogger::write(const VarjoEyeTrackingData& data)
{
    if (logfile_.is_open()) logfile_ << varjoEyeTrackingDataToCsvString(data);
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
        frameInfoHeaderCsv("renderingGazeFrameInfo", static_cast<size_t>(viewCount_))}) + "\n";
}

std::string VarjoEyeTrackingDataLogger::varjoEyeTrackingDataToCsvString(
    const VarjoEyeTrackingData& data)
{
    return VarjoToolkit::Csv::join({
        varjoGazeToCsvString(data.gaze),
        varjoEyeMeasurementsToCsvString(data.measurements),
        optionalDoubleToCsv(data.userIPD),
        optionalDoubleToCsv(data.hmdIPD),
        data.ipdAdjustmentMode,
        data.renderingGaze.has_value()
            ? varjoGazeToCsvString(*data.renderingGaze)
            : emptyGazeCsv(),
        varjoProjectedGazePositionToCsvString(data.gazePos_toVideo),
        data.renderingGazePos_toVideo.has_value()
            ? varjoProjectedGazePositionToCsvString(*data.renderingGazePos_toVideo)
            : emptyProjectedCsv(),
        varjoProjectedGazePositionToCsvString(data.gazePos_toVarjoDisplay),
        data.renderingGazePos_toVarjoDisplay.has_value()
            ? varjoProjectedGazePositionToCsvString(*data.renderingGazePos_toVarjoDisplay)
            : emptyProjectedCsv(),
        frameInfoToCsvString(data.frameInfo),
        data.renderingGazeFrameInfo.has_value()
            ? frameInfoToCsvString(*data.renderingGazeFrameInfo)
            : emptyFrameInfoCsv(static_cast<size_t>(viewCount_))}) + "\n";
}

std::string VarjoEyeTrackingDataLogger::varjoGazeToCsvString(
    const varjo_Gaze& gaze)
{
    return VarjoToolkit::Csv::toCsv(gaze);
}

std::string VarjoEyeTrackingDataLogger::varjoEyeMeasurementsToCsvString(
    const varjo_EyeMeasurements& measurements)
{
    return VarjoToolkit::Csv::toCsv(measurements);
}

std::string VarjoEyeTrackingDataLogger::varjoProjectedGazePositionToCsvString(
    const VarjoProjectedGazePosition& gazePosition)
{
    return VarjoToolkit::Csv::toCsv(gazePosition);
}

std::string VarjoEyeTrackingDataLogger::frameInfoToCsvString(
    const FrameInfo& frameInfo)
{
    std::vector<std::string> fields;
    fields.reserve(static_cast<size_t>(viewCount_) + 1);
    for (int index = 0; index < viewCount_; ++index) {
        if (static_cast<size_t>(index) < frameInfo.views.size()) {
            fields.push_back(VarjoToolkit::Csv::toCsv(
                frameInfo.views[static_cast<size_t>(index)]));
        } else {
            fields.push_back(VarjoToolkit::Csv::emptyFields(
                viewInfoCsvFieldCount()));
        }
    }
    fields.push_back(VarjoToolkit::Csv::join({
        std::to_string(frameInfo.displayTime),
        std::to_string(frameInfo.frameNumber)}));
    return VarjoToolkit::Csv::join(fields);
}

VarjoEyeTrackingService::VarjoEyeTrackingService(
    const std::shared_ptr<varjo_Session>& session,
    VarjoEyeTrackingProvider::OutputFilterType outputFilterType,
    VarjoEyeTrackingProvider::OutputFrequency outputFrequency,
    const std::string& filepath,
    size_t queueSize,
    int acquireFrequencyMs)
    : session_(session)
    , outputFilterType_(outputFilterType)
    , outputFrequency_(outputFrequency)
    , eyeTracker_(session)
    , logger_(filepath, session)
    , dataQueueMaxSize_(std::max<size_t>(1, queueSize))
    , acquireFrequencyMs_(std::max(1, acquireFrequencyMs))
{}

bool VarjoEyeTrackingService::start()
{
    stop();
    if (!logger_.open()) return false;

    threadEndSignal_.store(false);
    eyeTracker_.initialize(outputFilterType_, outputFrequency_);
    dataRequestThread_ = std::thread(
        &VarjoEyeTrackingService::dataRequestWorker,
        this);
    return true;
}

void VarjoEyeTrackingService::stop()
{
    threadEndSignal_.store(true);
    if (dataRequestThread_.joinable()) dataRequestThread_.join();
    eyeTracker_.shutdown();
    logger_.close();
}

bool VarjoEyeTrackingService::submitFrameInfo(
    const VarjoFrameInfoSnapshot& snapshot)
{
    return eyeTracker_.submitFrameInfo(snapshot);
}

std::deque<VarjoEyeTrackingData> VarjoEyeTrackingService::requestData()
{
    std::deque<VarjoEyeTrackingData> output;
    std::lock_guard<std::mutex> lock(dataQueueMutex_);
    output.swap(dataQueue_);
    return output;
}

void VarjoEyeTrackingService::dataRequestWorker()
{
    uint64_t queueDropCount = 0;
    while (!threadEndSignal_.load()) {
        auto dataItems = eyeTracker_.getEyeTrackingData();
        if (!dataItems.empty()) {
            std::lock_guard<std::mutex> lock(dataQueueMutex_);
            for (const auto& data : dataItems) {
                logger_.write(data);
                dataQueue_.push_back(data);
                while (dataQueue_.size() > dataQueueMaxSize_) {
                    dataQueue_.pop_front();
                    ++queueDropCount;
                    if (shouldLogCounter(queueDropCount)) {
                        VTK_SD_WARN("VarjoEyeTrackingService queue dropped oldest totalDropped="
                            << queueDropCount);
                    }
                }
            }
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(acquireFrequencyMs_));
    }
}
