#include <VarjoToolkit/MR/VarjoChromaKey.hpp>

#include <VarjoToolkit/Core/VarjoScopedLock.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {

double clamp01(double value)
{
    return std::max(0.0, std::min(1.0, value));
}

std::string tripleToString(const double values[3])
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3)
        << values[0] << "," << values[1] << "," << values[2];
    return oss.str();
}

} // namespace

VarjoChromaKey::VarjoChromaKey(varjo_Session* session)
    : session_(session)
{}

VarjoChromaKey::VarjoChromaKey(std::shared_ptr<varjo_Session> session)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
{}

VarjoChromaKey::VarjoChromaKey(const VarjoSession& session)
    : VarjoChromaKey(session.shared())
{}

bool VarjoChromaKey::valid() const
{
    return session_ != nullptr;
}

varjo_Session* VarjoChromaKey::session() const
{
    return session_;
}

std::shared_ptr<varjo_Session> VarjoChromaKey::sharedSession() const
{
    return session_owner_;
}

bool VarjoChromaKey::ownsSession() const
{
    return static_cast<bool>(session_owner_);
}

void VarjoChromaKey::setEnabled(bool enabled)
{
    if (!session_) {
        setLastError("session is null");
        return;
    }

    varjo_MRSetChromaKey(session_, enabled ? varjo_True : varjo_False);
    last_error_.clear();
}

void VarjoChromaKey::setGlobalEnabled(bool enabled)
{
    if (!session_) {
        setLastError("session is null");
        return;
    }

    varjo_MRSetChromaKeyGlobal(session_, enabled ? varjo_True : varjo_False);
    last_error_.clear();
}

int32_t VarjoChromaKey::configCount() const
{
    if (!session_) {
        return 0;
    }
    return varjo_MRGetChromaKeyConfigCount(session_);
}

std::vector<varjo_ChromaKeyConfig> VarjoChromaKey::getConfigs() const
{
    std::vector<varjo_ChromaKeyConfig> out;
    const int32_t count = configCount();
    if (count <= 0) {
        return out;
    }

    out.reserve(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        out.push_back(getConfig(i));
    }
    return out;
}

varjo_ChromaKeyConfig VarjoChromaKey::getConfig(int32_t index) const
{
    if (!session_ || !validateIndex(index)) {
        return makeDisabledConfig();
    }
    return varjo_MRGetChromaKeyConfig(session_, index);
}

bool VarjoChromaKey::setConfig(int32_t index, const varjo_ChromaKeyConfig& config)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }
    if (!validateIndex(index)) {
        return false;
    }

    VarjoScopedLock lock(session_, varjo_LockType_ChromaKey);
    if (!lock) {
        setLastError(lock.lastError());
        return false;
    }

    return setConfigUnlocked(index, config);
}

bool VarjoChromaKey::setConfigs(const std::vector<varjo_ChromaKeyConfig>& configs)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    const int32_t count = configCount();
    if (count <= 0) {
        setLastError("no chroma key config slots are available");
        return false;
    }
    if (configs.size() > static_cast<size_t>(count)) {
        setLastError("too many chroma key configs for available slots");
        return false;
    }

    VarjoScopedLock lock(session_, varjo_LockType_ChromaKey);
    if (!lock) {
        setLastError(lock.lastError());
        return false;
    }

    for (size_t i = 0; i < configs.size(); ++i) {
        if (!setConfigUnlocked(static_cast<int32_t>(i), configs[i])) {
            return false;
        }
    }
    for (int32_t i = static_cast<int32_t>(configs.size()); i < count; ++i) {
        if (!setConfigUnlocked(i, makeDisabledConfig())) {
            return false;
        }
    }

    last_error_.clear();
    return true;
}

bool VarjoChromaKey::disableConfig(int32_t index)
{
    return setConfig(index, makeDisabledConfig());
}

bool VarjoChromaKey::disableAllConfigs()
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    const int32_t count = configCount();
    if (count <= 0) {
        setLastError("no chroma key config slots are available");
        return false;
    }

    VarjoScopedLock lock(session_, varjo_LockType_ChromaKey);
    if (!lock) {
        setLastError(lock.lastError());
        return false;
    }

    for (int32_t i = 0; i < count; ++i) {
        if (!setConfigUnlocked(i, makeDisabledConfig())) {
            return false;
        }
    }

    last_error_.clear();
    return true;
}

const std::string& VarjoChromaKey::lastError() const
{
    return last_error_;
}

varjo_ChromaKeyConfig VarjoChromaKey::makeDisabledConfig()
{
    varjo_ChromaKeyConfig config{};
    config.type = varjo_ChromaKeyType_Disabled;
    return config;
}

varjo_ChromaKeyConfig VarjoChromaKey::makeHSVConfig(
    double targetH,
    double targetS,
    double targetV,
    double toleranceH,
    double toleranceS,
    double toleranceV,
    double falloffH,
    double falloffS,
    double falloffV)
{
    double target[3] = {targetH, targetS, targetV};
    double tolerance[3] = {toleranceH, toleranceS, toleranceV};
    double falloff[3] = {falloffH, falloffS, falloffV};
    return makeHSVConfig(target, tolerance, falloff);
}

varjo_ChromaKeyConfig VarjoChromaKey::makeHSVConfig(
    const double targetColor[3],
    const double tolerance[3],
    const double falloff[3])
{
    varjo_ChromaKeyConfig config{};
    config.type = varjo_ChromaKeyType_HSV;
    for (int i = 0; i < 3; ++i) {
        config.params.hsv.targetColor[i] = clamp01(targetColor[i]);
        config.params.hsv.tolerance[i] = clamp01(tolerance[i]);
        config.params.hsv.falloff[i] = clamp01(falloff[i]);
    }
    return config;
}

std::string VarjoChromaKey::typeToString(varjo_ChromaKeyType type)
{
    switch (type) {
    case varjo_ChromaKeyType_Disabled:
        return "Disabled";
    case varjo_ChromaKeyType_HSV:
        return "HSV";
    default:
        return "Unknown(" + std::to_string(type) + ")";
    }
}

std::string VarjoChromaKey::configToString(const varjo_ChromaKeyConfig& config)
{
    std::ostringstream oss;
    oss << typeToString(config.type);
    if (config.type == varjo_ChromaKeyType_HSV) {
        oss << " target=[" << tripleToString(config.params.hsv.targetColor) << "]"
            << " tolerance=[" << tripleToString(config.params.hsv.tolerance) << "]"
            << " falloff=[" << tripleToString(config.params.hsv.falloff) << "]";
    }
    return oss.str();
}

bool VarjoChromaKey::setConfigUnlocked(int32_t index, const varjo_ChromaKeyConfig& config)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }
    if (!validateIndex(index)) {
        return false;
    }

    varjo_MRSetChromaKeyConfig(session_, index, &config);
    last_error_.clear();
    return true;
}

bool VarjoChromaKey::validateIndex(int32_t index) const
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    const int32_t count = configCount();
    if (index < 0 || index >= count) {
        setLastError("chroma key config index out of range");
        return false;
    }
    return true;
}

void VarjoChromaKey::setLastError(std::string message) const
{
    last_error_ = std::move(message);
}
