#pragma once

#include <Varjo.h>
#include <Varjo_math.h>

#include <memory>
#include <string>
#include <vector>

#include <VarjoToolkit/Core/VarjoSession.hpp>

class VarjoOcclusionMesh {
public:
    struct Snapshot {
        int64_t vertexCount = 0;
        std::vector<varjo_Vector2Df> vertices;
        bool valid = false;
    };

    VarjoOcclusionMesh(varjo_Session* session, int32_t viewIndex, varjo_WindingOrder windingOrder = varjo_WindingOrder_Clockwise);
    VarjoOcclusionMesh(std::shared_ptr<varjo_Session> session, int32_t viewIndex, varjo_WindingOrder windingOrder = varjo_WindingOrder_Clockwise);
    VarjoOcclusionMesh(const VarjoSession& session, int32_t viewIndex, varjo_WindingOrder windingOrder = varjo_WindingOrder_Clockwise);
    ~VarjoOcclusionMesh();

    VarjoOcclusionMesh(const VarjoOcclusionMesh&) = delete;
    VarjoOcclusionMesh& operator=(const VarjoOcclusionMesh&) = delete;
    VarjoOcclusionMesh(VarjoOcclusionMesh&& other) noexcept;
    VarjoOcclusionMesh& operator=(VarjoOcclusionMesh&& other) noexcept;

    bool valid() const;
    explicit operator bool() const { return valid(); }

    const varjo_Mesh2Df* get() const;
    int32_t viewIndex() const;
    varjo_WindingOrder windingOrder() const;
    Snapshot snapshot() const;

    const std::string& lastError() const;

private:
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    varjo_Mesh2Df* mesh_ = nullptr;
    int32_t view_index_ = 0;
    varjo_WindingOrder winding_order_ = varjo_WindingOrder_Clockwise;
    mutable std::string last_error_;
};
