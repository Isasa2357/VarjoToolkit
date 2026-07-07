#pragma once

#include <Varjo.h>
#include <Varjo_mr.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <VarjoToolkit/Core/VarjoSession.hpp>

// Cached information for one Varjo MR camera property.
struct VarjoCameraPropertyInfo {
    varjo_CameraPropertyConfigType configType = varjo_CameraPropertyConfigType_List;
    std::vector<varjo_CameraPropertyMode> supportedModes;
    std::vector<varjo_CameraPropertyValue> supportedValues;
    varjo_CameraPropertyMode currentMode = varjo_CameraPropertyMode_Off;
    varjo_CameraPropertyValue currentValue{};
    bool supported = false;
    bool valid = false;
};

// C++ wrapper for Varjo mixed reality camera properties.
//
// This is the Toolkit counterpart of the Varjo examples CameraManager class.
// It focuses on reusable property enumeration, cached status, value/mode
// helpers, and lock-safe property changes. It intentionally does not print logs
// or own UI behavior.
class VarjoCameraProperties {
public:
    explicit VarjoCameraProperties(varjo_Session* session);
    explicit VarjoCameraProperties(std::shared_ptr<varjo_Session> session);
    explicit VarjoCameraProperties(const VarjoSession& session);
    ~VarjoCameraProperties();

    VarjoCameraProperties(const VarjoCameraProperties&) = delete;
    VarjoCameraProperties& operator=(const VarjoCameraProperties&) = delete;
    VarjoCameraProperties(VarjoCameraProperties&&) = delete;
    VarjoCameraProperties& operator=(VarjoCameraProperties&&) = delete;

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Session* session() const;
    std::shared_ptr<varjo_Session> sharedSession() const;
    bool ownsSession() const;

    static std::vector<varjo_CameraPropertyType> defaultPropertyTypes();

    const std::vector<varjo_CameraPropertyType>& propertyTypes() const;
    void setPropertyTypes(std::vector<varjo_CameraPropertyType> propertyTypes);

    bool enumerate(bool mrAvailable = true);
    bool update(varjo_CameraPropertyType type);
    bool updateAll();
    void clearCache();

    const VarjoCameraPropertyInfo* propertyInfo(varjo_CameraPropertyType type) const;
    std::optional<VarjoCameraPropertyInfo> propertyInfoCopy(varjo_CameraPropertyType type) const;

    std::vector<varjo_CameraPropertyMode> getSupportedModes(varjo_CameraPropertyType type) const;
    std::vector<varjo_CameraPropertyValue> getSupportedValues(varjo_CameraPropertyType type) const;
    varjo_CameraPropertyConfigType getConfigType(varjo_CameraPropertyType type) const;
    varjo_CameraPropertyMode getMode(varjo_CameraPropertyType type) const;
    varjo_CameraPropertyValue getValue(varjo_CameraPropertyType type) const;

    bool supportsMode(varjo_CameraPropertyType type, varjo_CameraPropertyMode mode) const;
    bool supportsValue(varjo_CameraPropertyType type, const varjo_CameraPropertyValue& value) const;

    bool setAutoMode(varjo_CameraPropertyType type);
    bool setMode(varjo_CameraPropertyType type, varjo_CameraPropertyMode mode);
    bool setValue(varjo_CameraPropertyType type, const varjo_CameraPropertyValue& value);
    bool setManualValue(varjo_CameraPropertyType type, const varjo_CameraPropertyValue& value);
    bool reset(varjo_CameraPropertyType type);
    bool resetAll();
    bool applyNextModeOrValue(varjo_CameraPropertyType type);

    bool acquireLock();
    void releaseLock();
    bool holdingLock() const;

    const std::string& lastError() const;

    static varjo_CameraPropertyValue makeBoolValue(bool value);
    static varjo_CameraPropertyValue makeIntValue(int64_t value);
    static varjo_CameraPropertyValue makeDoubleValue(double value);

    static bool valuesEqual(const varjo_CameraPropertyValue& lhs, const varjo_CameraPropertyValue& rhs);

    static std::string propertyTypeToString(varjo_CameraPropertyType type, bool brief = false);
    static std::string propertyModeToString(varjo_CameraPropertyMode mode);
    static std::string propertyConfigTypeToString(varjo_CameraPropertyConfigType configType);
    static std::string propertyDataTypeToString(varjo_CameraPropertyDataType dataType);
    static std::string propertyValueToString(const varjo_CameraPropertyValue& value);
    static std::string propertyValueToString(varjo_CameraPropertyType type, const varjo_CameraPropertyValue& value);

    std::string propertyAsString(varjo_CameraPropertyType type) const;

private:
    bool setModeUnlocked(varjo_CameraPropertyType type, varjo_CameraPropertyMode mode);
    bool setValueUnlocked(varjo_CameraPropertyType type, const varjo_CameraPropertyValue& value);
    int findModeIndex(varjo_CameraPropertyMode mode, const std::vector<varjo_CameraPropertyMode>& modes) const;
    int findValueIndex(const varjo_CameraPropertyValue& value, const std::vector<varjo_CameraPropertyValue>& values) const;
    bool setModeByModuloIndex(varjo_CameraPropertyType type, int index);
    bool setValueByModuloIndex(varjo_CameraPropertyType type, int index);
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;

    std::vector<varjo_CameraPropertyType> property_types_;
    std::unordered_map<varjo_CameraPropertyType, VarjoCameraPropertyInfo> property_infos_;

    bool holding_lock_ = false;
    mutable std::string last_error_;
};
