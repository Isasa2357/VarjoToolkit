#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

#include <iostream>
#include <string>

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

    std::cout << "[PASS] VarjoToolkit CSV utility test passed\n";
    return 0;
}
