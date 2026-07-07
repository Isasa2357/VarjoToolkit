#include <VarjoToolkit/MR/VarjoCameraProperties.hpp>
#include <VarjoToolkit/MR/VarjoChromaKey.hpp>

#include <cmath>
#include <iostream>
#include <string>

namespace {

int fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

bool expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }
    return true;
}

bool expectEqualString(const std::string& actual, const std::string& expected, const std::string& message)
{
    if (actual != expected) {
        std::cerr << "[FAIL] " << message << " expected=" << expected << " actual=" << actual << "\n";
        return false;
    }
    return true;
}

bool expectNear(double actual, double expected, const std::string& message)
{
    if (std::abs(actual - expected) > 1e-12) {
        std::cerr << "[FAIL] " << message << " expected=" << expected << " actual=" << actual << "\n";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit camera/chroma detailed test\n";

    VarjoCameraProperties camera(static_cast<varjo_Session*>(nullptr));
    if (camera.valid()) return fail("null camera properties should be invalid");
    if (camera.enumerate(true)) return fail("enumerate should fail for null session");
    if (camera.lastError().empty()) return fail("enumerate failure should set lastError");
    if (camera.acquireLock()) return fail("acquireLock should fail for null session");
    if (camera.holdingLock()) return fail("holdingLock should be false for null session");
    camera.releaseLock();
    if (camera.setMode(varjo_CameraPropertyType_ExposureTime, varjo_CameraPropertyMode_Auto)) return fail("setMode should fail for null session");
    if (camera.setAutoMode(varjo_CameraPropertyType_ExposureTime)) return fail("setAutoMode should fail for null session");
    if (camera.setValue(varjo_CameraPropertyType_ISOValue, VarjoCameraProperties::makeIntValue(100))) return fail("setValue should fail for null session");
    if (camera.setManualValue(varjo_CameraPropertyType_ISOValue, VarjoCameraProperties::makeIntValue(100))) return fail("setManualValue should fail for null session");
    if (camera.reset(varjo_CameraPropertyType_ISOValue)) return fail("reset should fail for null session");
    if (camera.resetAll()) return fail("resetAll should fail for null session");
    if (camera.applyNextModeOrValue(varjo_CameraPropertyType_ISOValue)) return fail("applyNextModeOrValue should fail for null session");

    auto types = VarjoCameraProperties::defaultPropertyTypes();
    if (!expect(!types.empty(), "default property types should not be empty")) return 1;
    camera.setPropertyTypes({varjo_CameraPropertyType_ISOValue});
    if (!expect(camera.propertyTypes().size() == 1, "setPropertyTypes should replace list")) return 1;
    if (camera.propertyInfo(varjo_CameraPropertyType_ISOValue) == nullptr) return fail("clearCache should create info slots");
    if (camera.propertyInfoCopy(varjo_CameraPropertyType_ISOValue).has_value() == false) return fail("propertyInfoCopy should return cached info");

    const auto boolValue = VarjoCameraProperties::makeBoolValue(true);
    const auto intValue = VarjoCameraProperties::makeIntValue(42);
    const auto doubleValue = VarjoCameraProperties::makeDoubleValue(1.25);
    if (boolValue.type != varjo_CameraPropertyDataType_Bool || boolValue.value.boolValue != varjo_True) return fail("makeBoolValue failed");
    if (intValue.type != varjo_CameraPropertyDataType_Int || intValue.value.intValue != 42) return fail("makeIntValue failed");
    if (doubleValue.type != varjo_CameraPropertyDataType_Double || doubleValue.value.doubleValue != 1.25) return fail("makeDoubleValue failed");
    if (!VarjoCameraProperties::valuesEqual(intValue, VarjoCameraProperties::makeIntValue(42))) return fail("valuesEqual should match equal ints");
    if (VarjoCameraProperties::valuesEqual(intValue, VarjoCameraProperties::makeIntValue(43))) return fail("valuesEqual should reject different ints");
    if (VarjoCameraProperties::valuesEqual(intValue, doubleValue)) return fail("valuesEqual should reject different types");

    if (!expectEqualString(VarjoCameraProperties::propertyModeToString(varjo_CameraPropertyMode_Off), "Off", "mode off string")) return 1;
    if (!expectEqualString(VarjoCameraProperties::propertyModeToString(varjo_CameraPropertyMode_Auto), "Auto", "mode auto string")) return 1;
    if (!expectEqualString(VarjoCameraProperties::propertyModeToString(varjo_CameraPropertyMode_Manual), "Manual", "mode manual string")) return 1;
    if (!expectEqualString(VarjoCameraProperties::propertyConfigTypeToString(varjo_CameraPropertyConfigType_List), "List", "config list string")) return 1;
    if (!expectEqualString(VarjoCameraProperties::propertyConfigTypeToString(varjo_CameraPropertyConfigType_Range), "Range", "config range string")) return 1;
    if (!expectEqualString(VarjoCameraProperties::propertyDataTypeToString(varjo_CameraPropertyDataType_Int), "Int", "type int string")) return 1;
    if (!expectEqualString(VarjoCameraProperties::propertyDataTypeToString(varjo_CameraPropertyDataType_Double), "Double", "type double string")) return 1;
    if (!expectEqualString(VarjoCameraProperties::propertyDataTypeToString(varjo_CameraPropertyDataType_Bool), "Bool", "type bool string")) return 1;
    if (!expectEqualString(VarjoCameraProperties::propertyValueToString(boolValue), "true", "bool value string")) return 1;
    if (!expectEqualString(VarjoCameraProperties::propertyValueToString(intValue), "42", "int value string")) return 1;
    if (!expectEqualString(VarjoCameraProperties::propertyValueToString(varjo_CameraPropertyType_AutoExposureBehavior, VarjoCameraProperties::makeIntValue(varjo_AutoExposureBehavior_Normal)), "Normal", "auto exposure behavior string")) return 1;

    VarjoChromaKey chroma(static_cast<varjo_Session*>(nullptr));
    if (chroma.valid()) return fail("null chroma key should be invalid");
    chroma.setEnabled(true);
    if (chroma.lastError().empty()) return fail("setEnabled should report null session");
    chroma.setGlobalEnabled(false);
    if (chroma.configCount() != 0) return fail("null chroma config count should be zero");
    if (!chroma.getConfigs().empty()) return fail("null chroma configs should be empty");

    const auto disabled = VarjoChromaKey::makeDisabledConfig();
    if (disabled.type != varjo_ChromaKeyType_Disabled) return fail("disabled config type mismatch");
    if (!expectEqualString(VarjoChromaKey::typeToString(varjo_ChromaKeyType_Disabled), "Disabled", "disabled chroma type string")) return 1;
    if (!expectEqualString(VarjoChromaKey::typeToString(varjo_ChromaKeyType_HSV), "HSV", "hsv chroma type string")) return 1;
    if (!expectEqualString(VarjoChromaKey::configToString(disabled), "Disabled", "disabled config string")) return 1;

    const auto hsv = VarjoChromaKey::makeHSVConfig(2.0, -1.0, 0.5, 0.25, 0.50, 0.75, -0.1, 1.1, 0.6);
    if (hsv.type != varjo_ChromaKeyType_HSV) return fail("HSV config type mismatch");
    if (!expectNear(hsv.params.hsv.targetColor[0], 1.0, "target H clamp high")) return 1;
    if (!expectNear(hsv.params.hsv.targetColor[1], 0.0, "target S clamp low")) return 1;
    if (!expectNear(hsv.params.hsv.targetColor[2], 0.5, "target V keep")) return 1;
    if (!expectNear(hsv.params.hsv.tolerance[0], 0.25, "tolerance H keep")) return 1;
    if (!expectNear(hsv.params.hsv.tolerance[1], 0.50, "tolerance S keep")) return 1;
    if (!expectNear(hsv.params.hsv.tolerance[2], 0.75, "tolerance V keep")) return 1;
    if (!expectNear(hsv.params.hsv.falloff[0], 0.0, "falloff H clamp low")) return 1;
    if (!expectNear(hsv.params.hsv.falloff[1], 1.0, "falloff S clamp high")) return 1;
    if (!expectNear(hsv.params.hsv.falloff[2], 0.6, "falloff V keep")) return 1;
    if (VarjoChromaKey::configToString(hsv).find("HSV") == std::string::npos) return fail("HSV config string should mention HSV");

    if (chroma.setConfig(0, hsv)) return fail("setConfig should fail for null session");
    if (chroma.setConfigs({hsv})) return fail("setConfigs should fail for null session");
    if (chroma.disableConfig(0)) return fail("disableConfig should fail for null session");
    if (chroma.disableAllConfigs()) return fail("disableAllConfigs should fail for null session");

    std::cout << "[PASS] VarjoToolkit camera/chroma detailed test passed\n";
    return 0;
}
