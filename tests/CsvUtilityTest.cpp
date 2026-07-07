#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int fail(const std::string& message, const std::string& actual = {}, const std::string& expected = {})
{
    std::cerr << "[FAIL] " << message << "\n";
    if (!expected.empty() || !actual.empty()) {
        std::cerr << "  expected: " << expected << "\n";
        std::cerr << "  actual  : " << actual << "\n";
    }
    return 1;
}

bool expectEqual(const std::string& actual, const std::string& expected, const std::string& label)
{
    if (actual != expected) {
        fail(label, actual, expected);
        return false;
    }
    return true;
}

size_t csvFieldCountByComma(const std::string& csv)
{
    if (csv.empty()) {
        return 0;
    }
    return static_cast<size_t>(std::count(csv.begin(), csv.end(), ',')) + 1;
}

bool expectFieldCountEqual(const std::string& row, const std::string& header, const std::string& label)
{
    const auto row_count = csvFieldCountByComma(row);
    const auto header_count = csvFieldCountByComma(header);
    if (row_count != header_count) {
        fail(label, std::to_string(row_count), std::to_string(header_count));
        return false;
    }
    return true;
}

} // namespace

int main()
{
    using namespace VarjoToolkit::Csv;

    std::cout << "VarjoToolkit CSV utility test\n";

    if (!expectEqual(makeHeader("coord", {"x", "y", "z"}), "coord.x,coord.y,coord.z", "generic dxyz-style header")) {
        return 1;
    }

    if (!expectEqual(join({number(1.25), number(-2.0), number(3.5)}), "1.25,-2,3.5", "generic dxyz-style value row")) {
        return 1;
    }

    const varjo_Vector2Df v2{1.0f, 2.0f};
    if (!expectEqual(headerForVector2Df("uv"), "uv.x,uv.y", "Vector2Df header")) {
        return 1;
    }
    if (!expectEqual(toCsv(v2), "1,2", "Vector2Df row")) {
        return 1;
    }

    const varjo_Vector3D v3{1.25, -2.0, 3.5};
    if (!expectEqual(header<varjo_Vector3D>("coord"), "coord.x,coord.y,coord.z", "Vector3D typed header")) {
        return 1;
    }
    if (!expectEqual(toCsv(v3), "1.25,-2,3.5", "Vector3D row")) {
        return 1;
    }

    varjo_Ray ray{};
    ray.origin[0] = 1.0;
    ray.origin[1] = 2.0;
    ray.origin[2] = 3.0;
    ray.forward[0] = 4.0;
    ray.forward[1] = 5.0;
    ray.forward[2] = 6.0;
    if (!expectEqual(headerForRay("gaze"), "gaze.origin.x,gaze.origin.y,gaze.origin.z,gaze.forward.x,gaze.forward.y,gaze.forward.z", "Ray header")) {
        return 1;
    }
    if (!expectEqual(toCsv(ray), "1,2,3,4,5,6", "Ray row")) {
        return 1;
    }

    varjo_Matrix matrix{};
    for (int i = 0; i < 16; ++i) {
        matrix.value[i] = static_cast<double>(i);
    }
    if (!expectEqual(headerForMatrix("pose"), "pose.m[0],pose.m[1],pose.m[2],pose.m[3],pose.m[4],pose.m[5],pose.m[6],pose.m[7],pose.m[8],pose.m[9],pose.m[10],pose.m[11],pose.m[12],pose.m[13],pose.m[14],pose.m[15]", "Matrix header")) {
        return 1;
    }
    if (!expectEqual(toCsv(matrix), "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15", "Matrix row")) {
        return 1;
    }

    VarjoProjectedGazePosition projected{};
    projected.leftEye = varjo_Vector2Df{1.0f, 2.0f};
    projected.rightEye = varjo_Vector2Df{3.0f, 4.0f};
    projected.combinedEyeToLeft = varjo_Vector2Df{5.0f, 6.0f};
    projected.combinedEyeToRight = varjo_Vector2Df{7.0f, 8.0f};
    if (!expectEqual(
            headerForProjectedGazePosition("gazePos"),
            "gazePos.leftEye.x,gazePos.leftEye.y,gazePos.rightEye.x,gazePos.rightEye.y,gazePos.combinedEyeToLeft.x,gazePos.combinedEyeToLeft.y,gazePos.combinedEyeToRight.x,gazePos.combinedEyeToRight.y",
            "ProjectedGazePosition header")) {
        return 1;
    }
    if (!expectEqual(toCsv(projected), "1,2,3,4,5,6,7,8", "ProjectedGazePosition row")) {
        return 1;
    }

    VarjoFrameInfoSnapshot frame_snapshot{};
    frame_snapshot.displayTime = 123;
    frame_snapshot.frameNumber = 456;
    frame_snapshot.valid = true;
    if (!expectEqual(headerForFrameInfoSnapshot("frame", 0), "frame.displayTime,frame.frameNumber,frame.valid", "FrameInfoSnapshot zero-view header")) {
        return 1;
    }
    if (!expectEqual(toCsv(frame_snapshot, 0), "123,456,1", "FrameInfoSnapshot zero-view row")) {
        return 1;
    }

    varjo_StreamConfig stream_config{};
    stream_config.streamId = 9;
    stream_config.channelFlags = varjo_ChannelFlag_Left | varjo_ChannelFlag_Right;
    stream_config.streamType = varjo_StreamType_DistortedColor;
    stream_config.bufferType = varjo_BufferType_CPU;
    stream_config.format = varjo_TextureFormat_NV12;
    stream_config.frameRate = 90;
    stream_config.width = 2880;
    stream_config.height = 1404;
    stream_config.rowStride = 2880;
    if (!expectFieldCountEqual(toCsv(stream_config), headerForStreamConfig("stream"), "StreamConfig field count")) {
        return 1;
    }

    varjo_CameraPropertyValue int_value{};
    int_value.type = varjo_CameraPropertyDataType_Int;
    int_value.value.intValue = 42;
    if (!expectEqual(headerForCameraPropertyValue("value"), "value.type,value.doubleValue,value.intValue,value.boolValue", "CameraPropertyValue header")) {
        return 1;
    }
    if (!expectEqual(toCsv(int_value), "1,,42,", "CameraPropertyValue int row")) {
        return 1;
    }

    varjo_WorldObject world_object{};
    world_object.id = 10;
    world_object.typeMask = varjo_WorldComponentTypeMask_Pose | varjo_WorldComponentTypeMask_ObjectMarker;
    if (!expectEqual(toCsv(world_object), "10,3,0,0", "WorldObject row")) {
        return 1;
    }

    varjo_WorldObjectMarkerComponent marker{};
    marker.id = 77;
    marker.flags = varjo_WorldObjectMarkerFlags_DoPrediction;
    marker.error = varjo_WorldObjectMarkerError_None;
    marker.size.width = 0.1;
    marker.size.height = 0.2;
    marker.size.depth = 0.3;
    if (!expectEqual(toCsv(marker), "77,1,0,0.10000000000000001,0.20000000000000001,0.29999999999999999", "WorldObjectMarkerComponent row")) {
        return 1;
    }

    varjo_Vector2Df vertices[2] = {varjo_Vector2Df{1.0f, 2.0f}, varjo_Vector2Df{3.0f, 4.0f}};
    varjo_Mesh2Df mesh{};
    mesh.vertices = vertices;
    mesh.vertexCount = 2;
    if (!expectEqual(headerForMesh2Df("mesh", 2), "mesh.vertexCount,mesh.vertices[0].x,mesh.vertices[0].y,mesh.vertices[1].x,mesh.vertices[1].y", "Mesh2Df fixed header")) {
        return 1;
    }
    if (!expectEqual(toCsv(mesh, 2), "2,1,2,3,4", "Mesh2Df fixed row")) {
        return 1;
    }

    varjo_Event event{};
    event.header.type = varjo_EventType_Button;
    event.header.timestamp = 12345;
    event.data.button.pressed = varjo_True;
    event.data.button.buttonId = varjo_ButtonId_Application;
    if (!expectFieldCountEqual(toCsv(event), headerForEvent("event"), "Event field count")) {
        return 1;
    }

    varjo_StreamFrame stream_frame{};
    stream_frame.type = varjo_StreamType_DistortedColor;
    stream_frame.id = 5;
    stream_frame.frameNumber = 6;
    stream_frame.channels = varjo_ChannelFlag_Left;
    stream_frame.dataFlags = varjo_DataFlag_Buffer;
    stream_frame.metadata.distortedColor.timestamp = 100;
    stream_frame.metadata.distortedColor.ev = 1.0;
    stream_frame.metadata.distortedColor.exposureTime = 0.5;
    stream_frame.metadata.distortedColor.whiteBalanceTemperature = 6500.0;
    stream_frame.metadata.distortedColor.cameraCalibrationConstant = 2.0;
    if (!expectFieldCountEqual(toCsv(stream_frame), headerForStreamFrame("frame"), "StreamFrame field count")) {
        return 1;
    }

    VarjoCameraPropertyInfo property_info{};
    property_info.configType = varjo_CameraPropertyConfigType_List;
    property_info.currentMode = varjo_CameraPropertyMode_Manual;
    property_info.currentValue = int_value;
    property_info.supported = true;
    property_info.valid = true;
    if (!expectEqual(toCsv(property_info), "1,0,0,2,1,,42,,1,1", "VarjoCameraPropertyInfo row")) {
        return 1;
    }

    std::cout << "[PASS] VarjoToolkit CSV utility test passed\n";
    return 0;
}
