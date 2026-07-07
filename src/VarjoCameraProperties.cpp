#include <VarjoToolkit/MR/VarjoCameraProperties.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {

const std::unordered_map<int64_t, std::string> kAutoExposureBehaviorNames = {
    {varjo_AutoExposureBehavior_Normal, "Normal"},
    {varjo_AutoExposureBehavior_PreventOverexposure, "Prevent Overexposure"},
};

} // namespace

VarjoCameraProperties::VarjoCameraProperties(varjo_Session* session)
    : session_(session)
    , property_types_(defaultPropertyTypes())
{
    clearCache();
}

VarjoCameraProperties::VarjoCameraProperties(std::shared_ptr<varjo_Session> session)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
    , property_types_(defaultPropertyTypes())
{
    clearCache();
}

VarjoCameraProperties::VarjoCameraProperties(const VarjoSession& session)
    : VarjoCameraProperties(session.shared())
{}

VarjoCameraProperties::~VarjoCameraProperties()
{
    releaseLock();
}

bool VarjoCameraProperties::valid() const
{
    return session_ != nullptr;
}

varjo_Session* VarjoCameraProperties::session() const
{
    return session_;
}

std::shared_ptr<varjo_Session> VarjoCameraProperties::sharedSession() const
{
    return session_owner_;
}

bool VarjoCameraProperties::ownsSession() const
{
    return static_cast<bool>(session_owner_);
}

std::vector<varjo_CameraPropertyType> VarjoCameraProperties::defaultPropertyTypes()
{
    return {
        varjo_CameraPropertyType_ExposureTime,
        varjo_CameraPropertyType_ISOValue,
        varjo_CameraPropertyType_WhiteBalance,
        varjo_CameraPropertyType_FlickerCompensation,
        varjo_CameraPropertyType_Sharpness,
        varjo_CameraPropertyType_EyeReprojection,
        varjo_CameraPropertyType_AutoExposureBehavior,
        varjo_CameraPropertyType_FocusDistance,
    };
}

const std::vector<varjo_CameraPropertyType>& VarjoCameraProperties::propertyTypes() const
{
    return property_types_;
}

void VarjoCameraProperties::setPropertyTypes(std::vector<varjo_CameraPropertyType> propertyTypes)
{
    property_types_ = std::move(propertyTypes);
    clearCache();
}

bool VarjoCameraProperties::enumerate(bool mrAvailable)
{
    clearCache();

    if (!mrAvailable) {
        setLastError("mixed reality is not available");
        return false;
    }
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    bool any_supported = false;
    for (const auto type : property_types_) {
        VarjoCameraPropertyInfo info{};
        const int32_t mode_count = varjo_MRGetCameraPropertyModeCount(session_, type);
        if (mode_count <= 0) {
            property_infos_[type] = info;
            continue;
        }

        info.supportedModes.resize(static_cast<size_t>(mode_count));
        varjo_MRGetCameraPropertyModes(session_, type, info.supportedModes.data(), mode_count);

        const int32_t value_count = varjo_MRGetCameraPropertyValueCount(session_, type);
        if (value_count > 0) {
            info.supportedValues.resize(static_cast<size_t>(value_count));
            varjo_MRGetCameraPropertyValues(session_, type, info.supportedValues.data(), value_count);
        }

        info.configType = varjo_MRGetCameraPropertyConfigType(session_, type);
        info.currentMode = varjo_MRGetCameraPropertyMode(session_, type);
        info.currentValue = varjo_MRGetCameraPropertyValue(session_, type);
        info.supported = true;
        info.valid = true;
        property_infos_[type] = info;
        any_supported = true;
    }

    if (!any_supported) {
        setLastError("no supported camera properties were found");
        return false;
    }

    last_error_.clear();
    return true;
}

bool VarjoCameraProperties::update(varjo_CameraPropertyType type)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    auto it = property_infos_.find(type);
    if (it == property_infos_.end()) {
        property_infos_.emplace(type, VarjoCameraPropertyInfo{});
        it = property_infos_.find(type);
    }

    VarjoCameraPropertyInfo& info = it->second;
    const int32_t mode_count = varjo_MRGetCameraPropertyModeCount(session_, type);
    if (mode_count > 0) {
        info.supportedModes.resize(static_cast<size_t>(mode_count));
        varjo_MRGetCameraPropertyModes(session_, type, info.supportedModes.data(), mode_count);
        info.supported = true;
    }

    const int32_t value_count = varjo_MRGetCameraPropertyValueCount(session_, type);
    if (value_count > 0) {
        info.supportedValues.resize(static_cast<size_t>(value_count));
        varjo_MRGetCameraPropertyValues(session_, type, info.supportedValues.data(), value_count);
    }

    if (!info.supported) {
        setLastError("camera property is not supported: " + propertyTypeToString(type));
        return false;
    }

    info.configType = varjo_MRGetCameraPropertyConfigType(session_, type);
    info.currentMode = varjo_MRGetCameraPropertyMode(session_, type);
    info.currentValue = varjo_MRGetCameraPropertyValue(session_, type);
    info.valid = true;
    last_error_.clear();
    return true;
}

bool VarjoCameraProperties::updateAll()
{
    bool ok = true;
    for (const auto type : property_types_) {
        ok = update(type) && ok;
    }
    return ok;
}

void VarjoCameraProperties::clearCache()
{
    property_infos_.clear();
    for (const auto type : property_types_) {
        property_infos_.emplace(type, VarjoCameraPropertyInfo{});
    }
}

const VarjoCameraPropertyInfo* VarjoCameraProperties::propertyInfo(varjo_CameraPropertyType type) const
{
    const auto it = property_infos_.find(type);
    if (it == property_infos_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::optional<VarjoCameraPropertyInfo> VarjoCameraProperties::propertyInfoCopy(varjo_CameraPropertyType type) const
{
    const auto* info = propertyInfo(type);
    if (!info) {
        return std::nullopt;
    }
    return *info;
}

std::vector<varjo_CameraPropertyMode> VarjoCameraProperties::getSupportedModes(varjo_CameraPropertyType type) const
{
    if (!session_) {
        return {};
    }

    const int32_t count = varjo_MRGetCameraPropertyModeCount(session_, type);
    if (count <= 0) {
        return {};
    }
    std::vector<varjo_CameraPropertyMode> modes(static_cast<size_t>(count));
    varjo_MRGetCameraPropertyModes(session_, type, modes.data(), count);
    return modes;
}

std::vector<varjo_CameraPropertyValue> VarjoCameraProperties::getSupportedValues(varjo_CameraPropertyType type) const
{
    if (!session_) {
        return {};
    }

    const int32_t count = varjo_MRGetCameraPropertyValueCount(session_, type);
    if (count <= 0) {
        return {};
    }
    std::vector<varjo_CameraPropertyValue> values(static_cast<size_t>(count));
    varjo_MRGetCameraPropertyValues(session_, type, values.data(), count);
    return values;
}

varjo_CameraPropertyConfigType VarjoCameraProperties::getConfigType(varjo_CameraPropertyType type) const
{
    if (!session_) {
        return varjo_CameraPropertyConfigType_List;
    }
    return varjo_MRGetCameraPropertyConfigType(session_, type);
}

varjo_CameraPropertyMode VarjoCameraProperties::getMode(varjo_CameraPropertyType type) const
{
    if (!session_) {
        return varjo_CameraPropertyMode_Off;
    }
    return varjo_MRGetCameraPropertyMode(session_, type);
}

varjo_CameraPropertyValue VarjoCameraProperties::getValue(varjo_CameraPropertyType type) const
{
    if (!session_) {
        return varjo_CameraPropertyValue{};
    }
    return varjo_MRGetCameraPropertyValue(session_, type);
}

bool VarjoCameraProperties::supportsMode(varjo_CameraPropertyType type, varjo_CameraPropertyMode mode) const
{
    const auto* info = propertyInfo(type);
    if (!info) {
        return false;
    }
    return std::find(info->supportedModes.begin(), info->supportedModes.end(), mode) != info->supportedModes.end();
}

bool VarjoCameraProperties::supportsValue(varjo_CameraPropertyType type, const varjo_CameraPropertyValue& value) const
{
    const auto* info = propertyInfo(type);
    if (!info) {
        return false;
    }
    return findValueIndex(value, info->supportedValues) >= 0;
}

bool VarjoCameraProperties::setAutoMode(varjo_CameraPropertyType type)
{
    return setMode(type, varjo_CameraPropertyMode_Auto);
}

bool VarjoCameraProperties::setMode(varjo_CameraPropertyType type, varjo_CameraPropertyMode mode)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    const bool need_lock = !holding_lock_;
    if (need_lock && !acquireLock()) {
        return false;
    }

    const bool ok = setModeUnlocked(type, mode);

    if (need_lock) {
        releaseLock();
    }
    return ok;
}

bool VarjoCameraProperties::setValue(varjo_CameraPropertyType type, const varjo_CameraPropertyValue& value)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    const bool need_lock = !holding_lock_;
    if (need_lock && !acquireLock()) {
        return false;
    }

    const bool ok = setValueUnlocked(type, value);

    if (need_lock) {
        releaseLock();
    }
    return ok;
}

bool VarjoCameraProperties::setManualValue(varjo_CameraPropertyType type, const varjo_CameraPropertyValue& value)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    const bool need_lock = !holding_lock_;
    if (need_lock && !acquireLock()) {
        return false;
    }

    const bool value_ok = setValueUnlocked(type, value);
    const bool mode_ok = value_ok && setModeUnlocked(type, varjo_CameraPropertyMode_Manual);

    if (need_lock) {
        releaseLock();
    }
    return value_ok && mode_ok;
}

bool VarjoCameraProperties::reset(varjo_CameraPropertyType type)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    const bool need_lock = !holding_lock_;
    if (need_lock && !acquireLock()) {
        return false;
    }

    varjo_MRResetCameraProperty(session_, type);
    const bool ok = update(type);

    if (need_lock) {
        releaseLock();
    }
    return ok;
}

bool VarjoCameraProperties::resetAll()
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    const bool need_lock = !holding_lock_;
    if (need_lock && !acquireLock()) {
        return false;
    }

    varjo_MRResetCameraProperties(session_);
    const bool ok = updateAll();

    if (need_lock) {
        releaseLock();
    }
    return ok;
}

bool VarjoCameraProperties::applyNextModeOrValue(varjo_CameraPropertyType type)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    const bool need_lock = !holding_lock_;
    if (need_lock && !acquireLock()) {
        return false;
    }

    bool ok = false;
    const auto current_mode = varjo_MRGetCameraPropertyMode(session_, type);
    const auto modes = getSupportedModes(type);

    if (current_mode == varjo_CameraPropertyMode_Manual) {
        const auto current_value = varjo_MRGetCameraPropertyValue(session_, type);
        const auto values = getSupportedValues(type);
        const int current_value_index = findValueIndex(current_value, values);
        if (current_value_index >= 0 && current_value_index + 1 < static_cast<int>(values.size())) {
            ok = setValueByModuloIndex(type, current_value_index + 1);
            if (need_lock) {
                releaseLock();
            }
            return ok;
        }
    }

    const int current_mode_index = findModeIndex(current_mode, modes);
    if (current_mode_index >= 0) {
        ok = setModeByModuloIndex(type, current_mode_index + 1);
    } else {
        setLastError("current camera property mode was not found in supported modes");
    }

    if (need_lock) {
        releaseLock();
    }
    return ok;
}

bool VarjoCameraProperties::acquireLock()
{
    if (holding_lock_) {
        return true;
    }
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    const bool ok = varjo_Lock(session_, varjo_LockType_Camera) == varjo_True;
    holding_lock_ = ok;
    if (!ok) {
        setLastError("failed to acquire Varjo camera lock");
    } else {
        last_error_.clear();
    }
    return ok;
}

void VarjoCameraProperties::releaseLock()
{
    if (!holding_lock_ || !session_) {
        holding_lock_ = false;
        return;
    }

    varjo_Unlock(session_, varjo_LockType_Camera);
    holding_lock_ = false;
}

bool VarjoCameraProperties::holdingLock() const
{
    return holding_lock_;
}

const std::string& VarjoCameraProperties::lastError() const
{
    return last_error_;
}

varjo_CameraPropertyValue VarjoCameraProperties::makeBoolValue(bool value)
{
    varjo_CameraPropertyValue out{};
    out.type = varjo_CameraPropertyDataType_Bool;
    out.value.boolValue = value ? varjo_True : varjo_False;
    return out;
}

varjo_CameraPropertyValue VarjoCameraProperties::makeIntValue(int64_t value)
{
    varjo_CameraPropertyValue out{};
    out.type = varjo_CameraPropertyDataType_Int;
    out.value.intValue = value;
    return out;
}

varjo_CameraPropertyValue VarjoCameraProperties::makeDoubleValue(double value)
{
    varjo_CameraPropertyValue out{};
    out.type = varjo_CameraPropertyDataType_Double;
    out.value.doubleValue = value;
    return out;
}

bool VarjoCameraProperties::valuesEqual(const varjo_CameraPropertyValue& lhs, const varjo_CameraPropertyValue& rhs)
{
    if (lhs.type != rhs.type) {
        return false;
    }

    switch (lhs.type) {
    case varjo_CameraPropertyDataType_Bool:
        return lhs.value.boolValue == rhs.value.boolValue;
    case varjo_CameraPropertyDataType_Int:
        return lhs.value.intValue == rhs.value.intValue;
    case varjo_CameraPropertyDataType_Double:
        return lhs.value.doubleValue == rhs.value.doubleValue;
    default:
        return false;
    }
}

std::string VarjoCameraProperties::propertyTypeToString(varjo_CameraPropertyType type, bool brief)
{
    switch (type) {
    case varjo_CameraPropertyType_ExposureTime: return brief ? "Exp" : "Exposure Time";
    case varjo_CameraPropertyType_ISOValue: return brief ? "ISO" : "ISO Value";
    case varjo_CameraPropertyType_WhiteBalance: return brief ? "WB" : "White Balance";
    case varjo_CameraPropertyType_FlickerCompensation: return brief ? "Flick" : "Flicker Compensation";
    case varjo_CameraPropertyType_Sharpness: return brief ? "Sharp" : "Sharpness";
    case varjo_CameraPropertyType_EyeReprojection: return brief ? "EyeReproj" : "Eye Reprojection";
    case varjo_CameraPropertyType_AutoExposureBehavior: return brief ? "AEBehavior" : "AE Behavior";
    case varjo_CameraPropertyType_FocusDistance: return brief ? "FocDist" : "Focus Distance";
    default: return "Unknown";
    }
}

std::string VarjoCameraProperties::propertyModeToString(varjo_CameraPropertyMode mode)
{
    switch (mode) {
    case varjo_CameraPropertyMode_Off: return "Off";
    case varjo_CameraPropertyMode_Auto: return "Auto";
    case varjo_CameraPropertyMode_Manual: return "Manual";
    default: return "Unknown";
    }
}

std::string VarjoCameraProperties::propertyConfigTypeToString(varjo_CameraPropertyConfigType configType)
{
    switch (configType) {
    case varjo_CameraPropertyConfigType_List: return "List";
    case varjo_CameraPropertyConfigType_Range: return "Range";
    default: return "Unknown";
    }
}

std::string VarjoCameraProperties::propertyDataTypeToString(varjo_CameraPropertyDataType dataType)
{
    switch (dataType) {
    case varjo_CameraPropertyDataType_Int: return "Int";
    case varjo_CameraPropertyDataType_Double: return "Double";
    case varjo_CameraPropertyDataType_Bool: return "Bool";
    default: return "Unknown";
    }
}

std::string VarjoCameraProperties::propertyValueToString(const varjo_CameraPropertyValue& value)
{
    std::ostringstream ss;
    switch (value.type) {
    case varjo_CameraPropertyDataType_Bool:
        ss << (value.value.boolValue == varjo_True ? "true" : "false");
        break;
    case varjo_CameraPropertyDataType_Int:
        ss << value.value.intValue;
        break;
    case varjo_CameraPropertyDataType_Double:
        ss << std::fixed << std::setprecision(2) << value.value.doubleValue;
        break;
    default:
        ss << "(invalid)";
        break;
    }
    return ss.str();
}

std::string VarjoCameraProperties::propertyValueToString(varjo_CameraPropertyType type, const varjo_CameraPropertyValue& value)
{
    if (type == varjo_CameraPropertyType_AutoExposureBehavior && value.type == varjo_CameraPropertyDataType_Int) {
        const auto it = kAutoExposureBehaviorNames.find(value.value.intValue);
        if (it != kAutoExposureBehaviorNames.end()) {
            return it->second;
        }
        return "Unknown";
    }

    return propertyValueToString(value);
}

std::string VarjoCameraProperties::propertyAsString(varjo_CameraPropertyType type) const
{
    const auto* info = propertyInfo(type);
    if (!info || !info->valid) {
        return propertyTypeToString(type) + ": unsupported";
    }

    return propertyTypeToString(type) + ": " +
        propertyModeToString(info->currentMode) + ", " +
        propertyValueToString(type, info->currentValue);
}

bool VarjoCameraProperties::setModeUnlocked(varjo_CameraPropertyType type, varjo_CameraPropertyMode mode)
{
    const auto* info = propertyInfo(type);
    if (info && !info->supportedModes.empty() && !supportsMode(type, mode)) {
        setLastError("requested mode is not supported for property: " + propertyTypeToString(type));
        return false;
    }

    varjo_MRSetCameraPropertyMode(session_, type, mode);
    return update(type);
}

bool VarjoCameraProperties::setValueUnlocked(varjo_CameraPropertyType type, const varjo_CameraPropertyValue& value)
{
    const auto* info = propertyInfo(type);
    if (info && !info->supportedValues.empty() && !supportsValue(type, value) && info->configType == varjo_CameraPropertyConfigType_List) {
        setLastError("requested value is not supported for property: " + propertyTypeToString(type));
        return false;
    }

    varjo_MRSetCameraPropertyValue(session_, type, &value);
    return update(type);
}

int VarjoCameraProperties::findModeIndex(varjo_CameraPropertyMode mode, const std::vector<varjo_CameraPropertyMode>& modes) const
{
    const auto it = std::find(modes.begin(), modes.end(), mode);
    if (it == modes.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(modes.begin(), it));
}

int VarjoCameraProperties::findValueIndex(const varjo_CameraPropertyValue& value, const std::vector<varjo_CameraPropertyValue>& values) const
{
    const auto it = std::find_if(values.begin(), values.end(), [&] (const varjo_CameraPropertyValue& item) {
        return valuesEqual(value, item);
    });
    if (it == values.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(values.begin(), it));
}

bool VarjoCameraProperties::setModeByModuloIndex(varjo_CameraPropertyType type, int index)
{
    const auto modes = getSupportedModes(type);
    if (modes.empty()) {
        setLastError("no supported modes for property: " + propertyTypeToString(type));
        return false;
    }

    const int wrapped = ((index % static_cast<int>(modes.size())) + static_cast<int>(modes.size())) % static_cast<int>(modes.size());
    return setModeUnlocked(type, modes[static_cast<size_t>(wrapped)]);
}

bool VarjoCameraProperties::setValueByModuloIndex(varjo_CameraPropertyType type, int index)
{
    const auto values = getSupportedValues(type);
    if (values.empty()) {
        setLastError("no supported values for property: " + propertyTypeToString(type));
        return false;
    }

    const int wrapped = ((index % static_cast<int>(values.size())) + static_cast<int>(values.size())) % static_cast<int>(values.size());
    return setValueUnlocked(type, values[static_cast<size_t>(wrapped)]);
}

void VarjoCameraProperties::setLastError(std::string message) const
{
    last_error_ = std::move(message);
}
