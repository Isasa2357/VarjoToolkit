#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace VarjoToolkit::Csv {
namespace {

std::string scopedName(const std::string& name, const std::string& field)
{
    if (name.empty()) {
        return field;
    }
    if (field.empty()) {
        return name;
    }
    return name + "." + field;
}

std::vector<std::string> indexedFields(const std::string& itemName, size_t count)
{
    std::vector<std::string> out;
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        out.push_back(itemName + "[" + std::to_string(i) + "]");
    }
    return out;
}

std::vector<std::string> splitCsvFields(const std::string& csv)
{
    std::vector<std::string> out;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) {
        out.push_back(item);
    }
    return out;
}

size_t csvFieldCount(const std::string& csv)
{
    if (csv.empty()) {
        return 0;
    }
    return splitCsvFields(csv).size();
}

std::string uinteger(uint64_t value)
{
    return std::to_string(static_cast<unsigned long long>(value));
}

std::string activeCameraValueField(
    const varjo_CameraPropertyValue& value,
    varjo_CameraPropertyDataType fieldType)
{
    if (value.type != fieldType) {
        return {};
    }

    switch (fieldType) {
    case varjo_CameraPropertyDataType_Double:
        return number(value.value.doubleValue);
    case varjo_CameraPropertyDataType_Int:
        return integer(value.value.intValue);
    case varjo_CameraPropertyDataType_Bool:
        return boolean(value.value.boolValue == varjo_True);
    default:
        return {};
    }
}

std::string emptyDistortedColorMetadataCsv()
{
    return emptyFields(csvFieldCount(headerForDistortedColorFrameMetadata("metadata.distortedColor")));
}

std::string emptyEnvironmentCubemapMetadataCsv()
{
    return emptyFields(csvFieldCount(headerForEnvironmentCubemapFrameMetadata("metadata.environmentCubemap")));
}

std::string emptyEyeCameraMetadataCsv()
{
    return emptyFields(csvFieldCount(headerForEyeCameraFrameMetadata("metadata.eyeCamera")));
}

std::vector<std::string> emptyEventPayloadFields()
{
    return {
        {},      // visibility.visible
        {}, {},  // button.pressed, button.buttonId
        {},      // trackingStatus.status
        {},      // headsetStatus.status
        {},      // displayStatus.status
        {},      // standbyStatus.onStandby
        {},      // foreground.isForeground
        {},      // mrDeviceStatus.status
        {},      // mrCameraPropertyChange.type
        {},      // dataStreamStart.streamId
        {},      // dataStreamStop.streamId
        {},      // textureSizeChange.typeMask
        {}       // visibilityMeshChange.viewIndex
    };
}

} // namespace

std::string number(double value)
{
    std::ostringstream oss;
    oss << std::setprecision(17) << value;
    return oss.str();
}

std::string integer(long long value)
{
    return std::to_string(value);
}

std::string boolean(bool value)
{
    return value ? "1" : "0";
}

std::string pointer(const void* value)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(value);
    return oss.str();
}

std::string join(std::initializer_list<std::string> fields)
{
    std::vector<std::string> v(fields.begin(), fields.end());
    return join(v);
}

std::string join(const std::vector<std::string>& fields)
{
    std::string out;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i != 0) {
            out.push_back(',');
        }
        out += fields[i];
    }
    return out;
}

std::string emptyFields(size_t count)
{
    if (count == 0) {
        return {};
    }
    return std::string(count - 1, ',');
}

std::string makeHeader(const std::string& name, std::initializer_list<std::string> fields)
{
    std::vector<std::string> v(fields.begin(), fields.end());
    return makeHeader(name, v);
}

std::string makeHeader(const std::string& name, const std::vector<std::string>& fields)
{
    std::vector<std::string> out;
    out.reserve(fields.size());
    for (const auto& field : fields) {
        out.push_back(scopedName(name, field));
    }
    return join(out);
}

std::string makeIndexedHeader(const std::string& name, const std::string& itemName, size_t count)
{
    return makeHeader(name, indexedFields(itemName, count));
}

std::string values(const double* valuesPtr, size_t count)
{
    std::vector<std::string> fields;
    fields.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        fields.push_back(number(valuesPtr[i]));
    }
    return join(fields);
}

std::string values(const float* valuesPtr, size_t count)
{
    std::vector<std::string> fields;
    fields.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        fields.push_back(number(static_cast<double>(valuesPtr[i])));
    }
    return join(fields);
}

std::string values(const int* valuesPtr, size_t count)
{
    std::vector<std::string> fields;
    fields.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        fields.push_back(integer(valuesPtr[i]));
    }
    return join(fields);
}

std::string toCsv(const varjo_Vector2Df& value)
{
    return join({number(value.x), number(value.y)});
}

std::string headerForVector2Df(const std::string& name)
{
    return makeHeader(name, {"x", "y"});
}

std::string toCsv(const varjo_Vector3D& value)
{
    return join({number(value.x), number(value.y), number(value.z)});
}

std::string headerForVector3D(const std::string& name)
{
    return makeHeader(name, {"x", "y", "z"});
}

std::string toCsv(const varjo_Matrix& value)
{
    return values(value.value, 16);
}

std::string headerForMatrix(const std::string& name)
{
    return makeIndexedHeader(name, "m", 16);
}

std::string toCsv(const varjo_Matrix3x3& value)
{
    return values(value.value, 9);
}

std::string headerForMatrix3x3(const std::string& name)
{
    return makeIndexedHeader(name, "m", 9);
}

std::string toCsv(const varjo_Ray& value)
{
    return join({values(value.origin, 3), values(value.forward, 3)});
}

std::string headerForRay(const std::string& name)
{
    return join({
        makeHeader(scopedName(name, "origin"), {"x", "y", "z"}),
        makeHeader(scopedName(name, "forward"), {"x", "y", "z"})
    });
}

std::string toCsv(const varjo_ViewInfo& value)
{
    return join({
        values(value.projectionMatrix, 16),
        values(value.viewMatrix, 16),
        integer(value.preferredWidth),
        integer(value.preferredHeight),
        boolean(value.enabled == varjo_True)
    });
}

std::string headerForViewInfo(const std::string& name)
{
    return join({
        makeIndexedHeader(scopedName(name, "projectionMatrix"), "v", 16),
        makeIndexedHeader(scopedName(name, "viewMatrix"), "v", 16),
        makeHeader(name, {"preferredWidth", "preferredHeight", "enabled"})
    });
}

std::string toCsv(const varjo_BufferMetadata& value)
{
    return join({
        integer(value.format),
        integer(value.type),
        integer(value.byteSize),
        integer(value.rowStride),
        integer(value.width),
        integer(value.height)
    });
}

std::string headerForBufferMetadata(const std::string& name)
{
    return makeHeader(name, {"format", "type", "byteSize", "rowStride", "width", "height"});
}

std::string toCsv(const varjo_CameraIntrinsics2& value)
{
    return join({
        integer(value.model),
        number(value.principalPointX),
        number(value.principalPointY),
        number(value.focalLengthX),
        number(value.focalLengthY),
        values(value.distortionCoefficients, 8)
    });
}

std::string headerForCameraIntrinsics2(const std::string& name)
{
    return join({
        makeHeader(name, {"model", "principalPointX", "principalPointY", "focalLengthX", "focalLengthY"}),
        makeIndexedHeader(scopedName(name, "distortionCoefficients"), "v", 8)
    });
}

std::string toCsv(const varjo_StreamConfig& value)
{
    return join({
        integer(value.streamId),
        uinteger(value.channelFlags),
        integer(value.streamType),
        integer(value.bufferType),
        integer(value.format),
        toCsv(value.streamTransform),
        integer(value.frameRate),
        integer(value.width),
        integer(value.height),
        integer(value.rowStride)
    });
}

std::string headerForStreamConfig(const std::string& name)
{
    return join({
        makeHeader(name, {"streamId", "channelFlags", "streamType", "bufferType", "format"}),
        headerForMatrix(scopedName(name, "streamTransform")),
        makeHeader(name, {"frameRate", "width", "height", "rowStride"})
    });
}

std::string toCsv(const varjo_DistortedColorFrameMetadata& value)
{
    return join({
        integer(value.timestamp),
        number(value.ev),
        number(value.exposureTime),
        number(value.whiteBalanceTemperature),
        values(value.wbNormalizationData.whiteBalanceColorGains, 3),
        toCsv(value.wbNormalizationData.invCCM),
        toCsv(value.wbNormalizationData.ccm),
        number(value.cameraCalibrationConstant)
    });
}

std::string headerForDistortedColorFrameMetadata(const std::string& name)
{
    return join({
        makeHeader(name, {"timestamp", "ev", "exposureTime", "whiteBalanceTemperature"}),
        makeIndexedHeader(scopedName(name, "wbNormalizationData.whiteBalanceColorGains"), "v", 3),
        headerForMatrix3x3(scopedName(name, "wbNormalizationData.invCCM")),
        headerForMatrix3x3(scopedName(name, "wbNormalizationData.ccm")),
        makeHeader(name, {"cameraCalibrationConstant"})
    });
}

std::string toCsv(const varjo_EnvironmentCubemapFrameMetadata& value)
{
    return join({
        integer(value.timestamp),
        integer(value.mode),
        number(value.whiteBalanceTemperature),
        number(value.brightnessNormalizationGain),
        values(value.wbNormalizationData.whiteBalanceColorGains, 3),
        toCsv(value.wbNormalizationData.invCCM),
        toCsv(value.wbNormalizationData.ccm)
    });
}

std::string headerForEnvironmentCubemapFrameMetadata(const std::string& name)
{
    return join({
        makeHeader(name, {"timestamp", "mode", "whiteBalanceTemperature", "brightnessNormalizationGain"}),
        makeIndexedHeader(scopedName(name, "wbNormalizationData.whiteBalanceColorGains"), "v", 3),
        headerForMatrix3x3(scopedName(name, "wbNormalizationData.invCCM")),
        headerForMatrix3x3(scopedName(name, "wbNormalizationData.ccm"))
    });
}

std::string toCsv(const varjo_EyeCameraFrameMetadata& value)
{
    return join({integer(value.timestamp), uinteger(value.glintMaskLeft), uinteger(value.glintMaskRight)});
}

std::string headerForEyeCameraFrameMetadata(const std::string& name)
{
    return makeHeader(name, {"timestamp", "glintMaskLeft", "glintMaskRight"});
}

std::string toCsv(const varjo_StreamFrame& value)
{
    std::string distorted = emptyDistortedColorMetadataCsv();
    std::string cubemap = emptyEnvironmentCubemapMetadataCsv();
    std::string eyeCamera = emptyEyeCameraMetadataCsv();

    if (value.type == varjo_StreamType_DistortedColor) {
        distorted = toCsv(value.metadata.distortedColor);
    } else if (value.type == varjo_StreamType_EnvironmentCubemap) {
        cubemap = toCsv(value.metadata.environmentCubemap);
    } else if (value.type == varjo_StreamType_EyeCamera) {
        eyeCamera = toCsv(value.metadata.eyeCamera);
    }

    return join({
        integer(value.type),
        integer(value.id),
        integer(value.frameNumber),
        uinteger(value.channels),
        uinteger(value.dataFlags),
        toCsv(value.hmdPose),
        distorted,
        cubemap,
        eyeCamera
    });
}

std::string headerForStreamFrame(const std::string& name)
{
    return join({
        makeHeader(name, {"type", "id", "frameNumber", "channels", "dataFlags"}),
        headerForMatrix(scopedName(name, "hmdPose")),
        headerForDistortedColorFrameMetadata(scopedName(name, "metadata.distortedColor")),
        headerForEnvironmentCubemapFrameMetadata(scopedName(name, "metadata.environmentCubemap")),
        headerForEyeCameraFrameMetadata(scopedName(name, "metadata.eyeCamera"))
    });
}

std::string toCsv(const varjo_WorldObject& value)
{
    return join({
        integer(value.id),
        uinteger(value.typeMask),
        uinteger(value.reserved[0]),
        uinteger(value.reserved[1])
    });
}

std::string headerForWorldObject(const std::string& name)
{
    return makeHeader(name, {"id", "typeMask", "reserved[0]", "reserved[1]"});
}

std::string toCsv(const varjo_WorldObjectMarkerComponent& value)
{
    return join({
        integer(value.id),
        integer(value.flags),
        integer(value.error),
        number(value.size.width),
        number(value.size.height),
        number(value.size.depth)
    });
}

std::string headerForWorldObjectMarkerComponent(const std::string& name)
{
    return makeHeader(name, {"id", "flags", "error", "size.width", "size.height", "size.depth"});
}

std::string toCsv(const varjo_Mesh2Df& value)
{
    return integer(value.vertexCount);
}

std::string toCsv(const varjo_Mesh2Df& value, size_t fixedVertexCount)
{
    std::vector<std::string> fields;
    fields.reserve(1 + fixedVertexCount);
    fields.push_back(integer(value.vertexCount));

    for (size_t i = 0; i < fixedVertexCount; ++i) {
        if (value.vertices && i < static_cast<size_t>(value.vertexCount)) {
            fields.push_back(toCsv(value.vertices[i]));
        } else {
            fields.push_back(emptyFields(2));
        }
    }
    return join(fields);
}

std::string headerForMesh2Df(const std::string& name)
{
    return makeHeader(name, {"vertexCount"});
}

std::string headerForMesh2Df(const std::string& name, size_t fixedVertexCount)
{
    std::vector<std::string> fields;
    fields.reserve(1 + fixedVertexCount);
    fields.push_back(makeHeader(name, {"vertexCount"}));
    for (size_t i = 0; i < fixedVertexCount; ++i) {
        fields.push_back(headerForVector2Df(scopedName(name, "vertices[" + std::to_string(i) + "]")));
    }
    return join(fields);
}

std::string toCsv(const varjo_Event& value)
{
    std::vector<std::string> fields;
    fields.reserve(16);
    fields.push_back(integer(value.header.type));
    fields.push_back(integer(value.header.timestamp));

    auto payload = emptyEventPayloadFields();
    switch (value.header.type) {
    case varjo_EventType_Visibility:
        payload[0] = boolean(value.data.visibility.visible == varjo_True);
        break;
    case varjo_EventType_Button:
        payload[1] = boolean(value.data.button.pressed == varjo_True);
        payload[2] = integer(value.data.button.buttonId);
        break;
    case varjo_EventType_TrackingStatus:
        payload[3] = integer(value.data.trackingStatus.status);
        break;
    case varjo_EventType_HeadsetStatus:
        payload[4] = integer(value.data.headsetStatus.status);
        break;
    case varjo_EventType_DisplayStatus:
        payload[5] = integer(value.data.displayStatus.status);
        break;
    case varjo_EventType_StandbyStatus:
        payload[6] = boolean(value.data.standbyStatus.onStandby == varjo_True);
        break;
    case varjo_EventType_Foreground:
        payload[7] = boolean(value.data.foreground.isForeground == varjo_True);
        break;
    case varjo_EventType_MRDeviceStatus:
        payload[8] = integer(value.data.mrDeviceStatus.status);
        break;
    case varjo_EventType_MRCameraPropertyChange:
        payload[9] = integer(value.data.mrCameraPropertyChange.type);
        break;
    case varjo_EventType_DataStreamStart:
        payload[10] = integer(value.data.dataStreamStart.streamId);
        break;
    case varjo_EventType_DataStreamStop:
        payload[11] = integer(value.data.dataStreamStop.streamId);
        break;
    case varjo_EventType_TextureSizeChange:
        payload[12] = integer(value.data.textureSizeChange.typeMask);
        break;
    case varjo_EventType_VisibilityMeshChange:
        payload[13] = integer(value.data.visibilityMeshChange.viewIndex);
        break;
    default:
        break;
    }

    fields.insert(fields.end(), payload.begin(), payload.end());
    return join(fields);
}

std::string headerForEvent(const std::string& name)
{
    return join({
        makeHeader(name, {"header.type", "header.timestamp"}),
        makeHeader(name, {
            "visibility.visible",
            "button.pressed",
            "button.buttonId",
            "trackingStatus.status",
            "headsetStatus.status",
            "displayStatus.status",
            "standbyStatus.onStandby",
            "foreground.isForeground",
            "mrDeviceStatus.status",
            "mrCameraPropertyChange.type",
            "dataStreamStart.streamId",
            "dataStreamStop.streamId",
            "textureSizeChange.typeMask",
            "visibilityMeshChange.viewIndex"
        })
    });
}

std::string toCsv(const varjo_CameraPropertyValue& value)
{
    return join({
        integer(value.type),
        activeCameraValueField(value, varjo_CameraPropertyDataType_Double),
        activeCameraValueField(value, varjo_CameraPropertyDataType_Int),
        activeCameraValueField(value, varjo_CameraPropertyDataType_Bool)
    });
}

std::string headerForCameraPropertyValue(const std::string& name)
{
    return makeHeader(name, {"type", "doubleValue", "intValue", "boolValue"});
}

std::string toCsv(const varjo_Gaze& value)
{
    return join({
        toCsv(value.leftEye),
        toCsv(value.rightEye),
        toCsv(value.gaze),
        number(value.focusDistance),
        number(value.stability),
        integer(value.captureTime),
        integer(value.leftStatus),
        integer(value.rightStatus),
        integer(value.status),
        integer(value.frameNumber)
    });
}

std::string headerForGaze(const std::string& name)
{
    return join({
        headerForRay(scopedName(name, "leftEye")),
        headerForRay(scopedName(name, "rightEye")),
        headerForRay(scopedName(name, "gaze")),
        makeHeader(name, {"focusDistance", "stability", "captureTime", "leftStatus", "rightStatus", "status", "frameNumber"})
    });
}

std::string toCsv(const varjo_EyeMeasurements& value)
{
    return join({
        integer(value.frameNumber),
        integer(value.captureTime),
        number(value.interPupillaryDistanceInMM),
        number(value.leftPupilIrisDiameterRatio),
        number(value.rightPupilIrisDiameterRatio),
        number(value.leftPupilDiameterInMM),
        number(value.rightPupilDiameterInMM),
        number(value.leftIrisDiameterInMM),
        number(value.rightIrisDiameterInMM),
        number(value.leftEyeOpenness),
        number(value.rightEyeOpenness)
    });
}

std::string headerForEyeMeasurements(const std::string& name)
{
    return makeHeader(name, {
        "frameNumber",
        "captureTime",
        "interPupillaryDistanceInMM",
        "leftPupilIrisDiameterRatio",
        "rightPupilIrisDiameterRatio",
        "leftPupilDiameterInMM",
        "rightPupilDiameterInMM",
        "leftIrisDiameterInMM",
        "rightIrisDiameterInMM",
        "leftEyeOpenness",
        "rightEyeOpenness"
    });
}

std::string toCsv(const VarjoProjectedGazePosition& value)
{
    return join({
        toCsv(value.leftEye),
        toCsv(value.rightEye),
        toCsv(value.combinedEyeToLeft),
        toCsv(value.combinedEyeToRight)
    });
}

std::string headerForProjectedGazePosition(const std::string& name)
{
    return join({
        headerForVector2Df(scopedName(name, "leftEye")),
        headerForVector2Df(scopedName(name, "rightEye")),
        headerForVector2Df(scopedName(name, "combinedEyeToLeft")),
        headerForVector2Df(scopedName(name, "combinedEyeToRight"))
    });
}

std::string toCsv(const VarjoFrameInfoSnapshot& value)
{
    return toCsv(value, value.views.size());
}

std::string toCsv(const VarjoFrameInfoSnapshot& value, size_t fixedViewCount)
{
    std::vector<std::string> fields;
    fields.reserve(fixedViewCount + 3);

    for (size_t i = 0; i < fixedViewCount; ++i) {
        if (i < value.views.size()) {
            fields.push_back(toCsv(value.views[i]));
        } else {
            fields.push_back(emptyFields(csvFieldCount(headerForViewInfo("view"))));
        }
    }

    fields.push_back(integer(value.displayTime));
    fields.push_back(integer(value.frameNumber));
    fields.push_back(boolean(value.valid));
    return join(fields);
}

std::string headerForFrameInfoSnapshot(const std::string& name, size_t viewCount)
{
    std::vector<std::string> fields;
    fields.reserve(viewCount + 3);

    for (size_t i = 0; i < viewCount; ++i) {
        fields.push_back(headerForViewInfo(scopedName(name, "views[" + std::to_string(i) + "]")));
    }

    fields.push_back(makeHeader(name, {"displayTime", "frameNumber", "valid"}));
    return join(fields);
}

std::string toCsv(const VarjoCameraPropertyInfo& value)
{
    return join({
        integer(value.configType),
        integer(static_cast<long long>(value.supportedModes.size())),
        integer(static_cast<long long>(value.supportedValues.size())),
        integer(value.currentMode),
        toCsv(value.currentValue),
        boolean(value.supported),
        boolean(value.valid)
    });
}

std::string headerForCameraPropertyInfo(const std::string& name)
{
    return join({
        makeHeader(name, {"configType", "supportedModeCount", "supportedValueCount", "currentMode"}),
        headerForCameraPropertyValue(scopedName(name, "currentValue")),
        makeHeader(name, {"supported", "valid"})
    });
}

} // namespace VarjoToolkit::Csv
