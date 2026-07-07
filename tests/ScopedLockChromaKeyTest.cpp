#include <VarjoToolkit/Core/VarjoScopedLock.hpp>
#include <VarjoToolkit/MR/VarjoChromaKey.hpp>

#include <iostream>
#include <string>
#include <utility>

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
    std::cout << "VarjoToolkit scoped lock and chroma key test\n";

    if (!expectEqual(VarjoScopedLock::lockTypeToString(varjo_LockType_Camera), "Camera", "Camera lock type string")) {
        return 1;
    }
    if (!expectEqual(VarjoScopedLock::lockTypeToString(varjo_LockType_ChromaKey), "ChromaKey", "ChromaKey lock type string")) {
        return 1;
    }
    if (!expectEqual(VarjoScopedLock::lockTypeToString(varjo_LockType_EnvironmentCubemap), "EnvironmentCubemap", "EnvironmentCubemap lock type string")) {
        return 1;
    }

    VarjoScopedLock null_lock(nullptr, varjo_LockType_Camera);
    if (null_lock.locked()) {
        return fail("VarjoScopedLock constructed from nullptr should not lock");
    }
    if (null_lock.lastError().empty()) {
        return fail("failed VarjoScopedLock should set lastError");
    }

    VarjoScopedLock moved_null_lock(std::move(null_lock));
    if (moved_null_lock.locked()) {
        return fail("moved null VarjoScopedLock should not lock");
    }
    moved_null_lock.unlock();

    if (!expectEqual(VarjoChromaKey::typeToString(varjo_ChromaKeyType_Disabled), "Disabled", "Disabled chroma key type string")) {
        return 1;
    }
    if (!expectEqual(VarjoChromaKey::typeToString(varjo_ChromaKeyType_HSV), "HSV", "HSV chroma key type string")) {
        return 1;
    }

    const auto disabled = VarjoChromaKey::makeDisabledConfig();
    if (disabled.type != varjo_ChromaKeyType_Disabled) {
        return fail("makeDisabledConfig should create disabled config");
    }
    if (!expectEqual(VarjoChromaKey::configToString(disabled), "Disabled", "Disabled config string")) {
        return 1;
    }

    const auto hsv = VarjoChromaKey::makeHSVConfig(
        1.2, -0.1, 0.5,
        0.1, 0.2, 0.3,
        0.4, 0.5, 0.6);
    if (hsv.type != varjo_ChromaKeyType_HSV) {
        return fail("makeHSVConfig should create HSV config");
    }
    if (hsv.params.hsv.targetColor[0] != 1.0 || hsv.params.hsv.targetColor[1] != 0.0 || hsv.params.hsv.targetColor[2] != 0.5) {
        return fail("makeHSVConfig should clamp targetColor to [0,1]");
    }
    if (hsv.params.hsv.tolerance[0] != 0.1 || hsv.params.hsv.tolerance[1] != 0.2 || hsv.params.hsv.tolerance[2] != 0.3) {
        return fail("makeHSVConfig should store tolerance");
    }
    if (hsv.params.hsv.falloff[0] != 0.4 || hsv.params.hsv.falloff[1] != 0.5 || hsv.params.hsv.falloff[2] != 0.6) {
        return fail("makeHSVConfig should store falloff");
    }

    VarjoChromaKey null_chroma(nullptr);
    if (null_chroma.valid()) {
        return fail("VarjoChromaKey constructed from nullptr should be invalid");
    }
    if (null_chroma.configCount() != 0) {
        return fail("null VarjoChromaKey should report zero config count");
    }
    if (!null_chroma.getConfigs().empty()) {
        return fail("null VarjoChromaKey should return no configs");
    }
    if (null_chroma.setConfig(0, hsv)) {
        return fail("setConfig should fail for null VarjoChromaKey");
    }
    if (null_chroma.lastError().empty()) {
        return fail("failed VarjoChromaKey operation should set lastError");
    }
    if (null_chroma.disableAllConfigs()) {
        return fail("disableAllConfigs should fail for null VarjoChromaKey");
    }

    std::cout << "[PASS] VarjoToolkit scoped lock and chroma key test passed\n";
    return 0;
}
