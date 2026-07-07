#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <iomanip>
#include <sstream>
#include <utility>

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

void appendAll(std::vector<std::string>& dst, std::initializer_list<std::string> src)
{
    dst.insert(dst.end(), src.begin(), src.end());
}

void appendVectorHeaderFields(std::vector<std::string>& dst, const std::string& prefix)
{
    appendAll(dst, {
        scopedName(prefix, "x"),
        scopedName(prefix, "y"),
        scopedName(prefix, "z")
    });
}

void appendVector2HeaderFields(std::vector<std::string>& dst, const std::string& prefix)
{
    appendAll(dst, {
        scopedName(prefix, "x"),
        scopedName(prefix, "y")
    });
}

void appendMatrixHeaderFields(std::vector<std::string>& dst, const std::string& prefix)
{
    const auto fields = makeHeader(prefix, indexedFields("m", 16));
    std::stringstream ss(fields);
    std::string item;
    while (std::getline(ss, item, ',')) {
        dst.push_back(item);
    }
}

void appendMatrix3x3HeaderFields(std::vector<std::string>& dst, const std::string& prefix)
{
    const auto fields = makeHeader(prefix, indexedFields("m", 9));
    std::stringstream ss(fields);
    std::string item;
    while (std::getline(ss, item, ',')) {
        dst.push_back(item);
    }
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

void appendCsvFields(std::vector<std::string>& dst, const std::string& csv)
{
    const auto fields = splitCsvFields(csv);
    dst.insert(dst.end(), fields.begin(), fields.end());
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
    return join({
        number(value.x),
        number(value.y)
    });
}

std::string headerForVector2Df(const std::string& name)
{
    return makeHeader(name, {"x", "y"});
}

std::string toCsv(const varjo_Vector3D& value)
{
    return join({
        number(value.x),
        number(value.y),
        number(value.z)
    });
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
    return join({
        values(value.origin, 3),
        values(value.forward, 3)
    });
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
    return makeHeader(name, {
        "format",
        "type",
        "byteSize",
        "rowStride",
        "width",
        "height"
    });
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
        makeHeader(name, {
            "model",
            "principalPointX",
            "principalPointY",
            "focalLengthX",
            "focalLengthY"
        }),
        makeIndexedHeader(scopedName(name, "distortionCoefficients"), "v", 8)
    });
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
        makeHeader(name, {
            "focusDistance",
            "stability",
            "captureTime",
            "leftStatus",
            "rightStatus",
            "status",
            "frameNumber"
        })
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
            fields.push_back(emptyFields(splitCsvFields(headerForViewInfo("view")).size()));
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

} // namespace VarjoToolkit::Csv
