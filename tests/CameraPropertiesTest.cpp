#include <VarjoToolkit/MR/VarjoCameraProperties.hpp>

#include <iostream>
#include <string>

namespace {

int fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

bool expectEqual(const std::string& actual, const std::string& expected, const std::string& label)
{
    if (actual != expected) {
        std::cerr << "[FAIL] " << label << "\n";
        std::cerr << "  expected: " << expected << "\n";
        std::cerr << "  actual  : " << actual << "\n";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit camera properties wrapper test\n";

    const auto property_types = VarjoCameraProperties::defaultPropertyTypes();
    if (property_types.size() != 8) {
        return fail("defaultPropertyTypes should contain the known Varjo camera properties");
    }

    if (!expectEqual(VarjoCameraProperties::propertyTypeToString(varjo_CameraPropertyType_ExposureTime), "Exposure Time", "ExposureTime string")) {
        return 1;
    }
    if (!expectEqual(VarjoCameraProperties::propertyTypeToString(varjo_CameraPropertyType_ExposureTime, true), "Exp", "ExposureTime brief string")) {
        return 1;
    }
    if (!expectEqual(VarjoCameraProperties::propertyModeToString(varjo_CameraPropertyMode_Auto), "Auto", "Auto mode string")) {
        return 1;
    }
    if (!expectEqual(VarjoCameraProperties::propertyConfigTypeToString(varjo_CameraPropertyConfigType_Range), "Range", "Range config string")) {
        return 1;
    }
    if (!expectEqual(VarjoCameraProperties::propertyDataTypeToString(varjo_CameraPropertyDataType_Bool), "Bool", "Bool data type string")) {
        return 1;
    }

    const auto bool_value = VarjoCameraProperties::makeBoolValue(true);
    if (bool_value.type != varjo_CameraPropertyDataType_Bool || bool_value.value.boolValue != varjo_True) {
        return fail("makeBoolValue returned unexpected value");
    }
    if (!expectEqual(VarjoCameraProperties::propertyValueToString(bool_value), "true", "bool value string")) {
        return 1;
    }

    const auto int_value = VarjoCameraProperties::makeIntValue(200);
    if (int_value.type != varjo_CameraPropertyDataType_Int || int_value.value.intValue != 200) {
        return fail("makeIntValue returned unexpected value");
    }
    if (!expectEqual(VarjoCameraProperties::propertyValueToString(int_value), "200", "int value string")) {
        return 1;
    }

    const auto double_value = VarjoCameraProperties::makeDoubleValue(1.25);
    if (double_value.type != varjo_CameraPropertyDataType_Double || double_value.value.doubleValue != 1.25) {
        return fail("makeDoubleValue returned unexpected value");
    }
    if (!expectEqual(VarjoCameraProperties::propertyValueToString(double_value), "1.25", "double value string")) {
        return 1;
    }

    const auto ae_normal = VarjoCameraProperties::makeIntValue(varjo_AutoExposureBehavior_Normal);
    if (!expectEqual(VarjoCameraProperties::propertyValueToString(varjo_CameraPropertyType_AutoExposureBehavior, ae_normal), "Normal", "AE normal value string")) {
        return 1;
    }
    const auto ae_prevent = VarjoCameraProperties::makeIntValue(varjo_AutoExposureBehavior_PreventOverexposure);
    if (!expectEqual(VarjoCameraProperties::propertyValueToString(varjo_CameraPropertyType_AutoExposureBehavior, ae_prevent), "Prevent Overexposure", "AE prevent value string")) {
        return 1;
    }

    if (!VarjoCameraProperties::valuesEqual(int_value, VarjoCameraProperties::makeIntValue(200))) {
        return fail("valuesEqual should match identical int values");
    }
    if (VarjoCameraProperties::valuesEqual(int_value, VarjoCameraProperties::makeIntValue(201))) {
        return fail("valuesEqual should reject different int values");
    }
    if (VarjoCameraProperties::valuesEqual(int_value, double_value)) {
        return fail("valuesEqual should reject different value types");
    }

    VarjoCameraProperties null_camera(nullptr);
    if (null_camera.valid()) {
        return fail("VarjoCameraProperties constructed from nullptr should be invalid");
    }
    if (null_camera.enumerate(true)) {
        return fail("enumerate should fail for null session");
    }
    if (null_camera.lastError().empty()) {
        return fail("failed null enumerate should set lastError");
    }
    if (null_camera.acquireLock()) {
        return fail("acquireLock should fail for null session");
    }
    if (null_camera.holdingLock()) {
        return fail("null camera wrapper should not report lock ownership");
    }
    if (null_camera.setAutoMode(varjo_CameraPropertyType_ExposureTime)) {
        return fail("setAutoMode should fail for null session");
    }
    if (null_camera.resetAll()) {
        return fail("resetAll should fail for null session");
    }
    if (!null_camera.getSupportedModes(varjo_CameraPropertyType_ExposureTime).empty()) {
        return fail("null session should return no supported modes");
    }
    if (!null_camera.getSupportedValues(varjo_CameraPropertyType_ExposureTime).empty()) {
        return fail("null session should return no supported values");
    }

    std::cout << "[PASS] VarjoToolkit camera properties wrapper test passed\n";
    return 0;
}
