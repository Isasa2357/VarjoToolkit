#pragma once

#include <Varjo.h>
#include <Varjo_mr.h>

#include <memory>
#include <string>
#include <vector>

#include <VarjoToolkit/Core/VarjoSession.hpp>

// C++ wrapper for Varjo chroma key control and configuration.
//
// This wrapper exposes application/global enable toggles, config enumeration,
// get/set/reset helpers, and HSV config creation. Configuration writes use
// VarjoScopedLock internally.
class VarjoChromaKey {
public:
    explicit VarjoChromaKey(varjo_Session* session);
    explicit VarjoChromaKey(std::shared_ptr<varjo_Session> session);
    explicit VarjoChromaKey(const VarjoSession& session);
    ~VarjoChromaKey() = default;

    VarjoChromaKey(const VarjoChromaKey&) = delete;
    VarjoChromaKey& operator=(const VarjoChromaKey&) = delete;
    VarjoChromaKey(VarjoChromaKey&&) noexcept = default;
    VarjoChromaKey& operator=(VarjoChromaKey&&) noexcept = default;

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Session* session() const;
    std::shared_ptr<varjo_Session> sharedSession() const;
    bool ownsSession() const;

    void setEnabled(bool enabled);
    void setGlobalEnabled(bool enabled);

    int32_t configCount() const;
    std::vector<varjo_ChromaKeyConfig> getConfigs() const;
    varjo_ChromaKeyConfig getConfig(int32_t index) const;

    bool setConfig(int32_t index, const varjo_ChromaKeyConfig& config);
    bool setConfigs(const std::vector<varjo_ChromaKeyConfig>& configs);
    bool disableConfig(int32_t index);
    bool disableAllConfigs();

    const std::string& lastError() const;

    static varjo_ChromaKeyConfig makeDisabledConfig();
    static varjo_ChromaKeyConfig makeHSVConfig(
        double targetH,
        double targetS,
        double targetV,
        double toleranceH,
        double toleranceS,
        double toleranceV,
        double falloffH = 0.0,
        double falloffS = 0.0,
        double falloffV = 0.0);
    static varjo_ChromaKeyConfig makeHSVConfig(
        const double targetColor[3],
        const double tolerance[3],
        const double falloff[3]);

    static std::string typeToString(varjo_ChromaKeyType type);
    static std::string configToString(const varjo_ChromaKeyConfig& config);

private:
    bool setConfigUnlocked(int32_t index, const varjo_ChromaKeyConfig& config);
    bool validateIndex(int32_t index) const;
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    mutable std::string last_error_;
};
