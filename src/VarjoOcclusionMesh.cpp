#include <VarjoToolkit/Rendering/VarjoOcclusionMesh.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>

#include <utility>

VarjoOcclusionMesh::VarjoOcclusionMesh(varjo_Session* session, int32_t viewIndex, varjo_WindingOrder windingOrder)
    : session_(session)
    , view_index_(viewIndex)
    , winding_order_(windingOrder)
{
    VTK_SD_LOG("VarjoOcclusionMesh raw constructor session=" << session_ << " viewIndex=" << view_index_ << " winding=" << static_cast<int64_t>(winding_order_));
    if (session_) {
        mesh_ = varjo_CreateOcclusionMesh(session_, view_index_, winding_order_);
        VTK_SD_LOG("varjo_CreateOcclusionMesh returned " << mesh_);
    }
    if (!mesh_) {
        setLastError("failed to create Varjo occlusion mesh");
    }
}

VarjoOcclusionMesh::VarjoOcclusionMesh(std::shared_ptr<varjo_Session> session, int32_t viewIndex, varjo_WindingOrder windingOrder)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
    , view_index_(viewIndex)
    , winding_order_(windingOrder)
{
    VTK_SD_LOG("VarjoOcclusionMesh shared constructor session=" << session_ << " viewIndex=" << view_index_ << " winding=" << static_cast<int64_t>(winding_order_));
    if (session_) {
        mesh_ = varjo_CreateOcclusionMesh(session_, view_index_, winding_order_);
        VTK_SD_LOG("varjo_CreateOcclusionMesh returned " << mesh_);
    }
    if (!mesh_) {
        setLastError("failed to create Varjo occlusion mesh");
    }
}

VarjoOcclusionMesh::VarjoOcclusionMesh(const VarjoSession& session, int32_t viewIndex, varjo_WindingOrder windingOrder)
    : VarjoOcclusionMesh(session.shared(), viewIndex, windingOrder)
{}

VarjoOcclusionMesh::~VarjoOcclusionMesh()
{
    if (mesh_) {
        VTK_SD_LOG("varjo_FreeOcclusionMesh mesh=" << mesh_ << " viewIndex=" << view_index_);
        varjo_FreeOcclusionMesh(mesh_);
        mesh_ = nullptr;
    }
}

VarjoOcclusionMesh::VarjoOcclusionMesh(VarjoOcclusionMesh&& other) noexcept
    : session_owner_(std::move(other.session_owner_))
    , session_(std::exchange(other.session_, nullptr))
    , mesh_(std::exchange(other.mesh_, nullptr))
    , view_index_(std::exchange(other.view_index_, 0))
    , winding_order_(std::exchange(other.winding_order_, varjo_WindingOrder_Clockwise))
    , last_error_(std::move(other.last_error_))
{
    VTK_SD_LOG("VarjoOcclusionMesh move constructor mesh=" << mesh_ << " viewIndex=" << view_index_);
}

VarjoOcclusionMesh& VarjoOcclusionMesh::operator=(VarjoOcclusionMesh&& other) noexcept
{
    if (this != &other) {
        if (mesh_) {
            VTK_SD_LOG("VarjoOcclusionMesh move assignment freeing current mesh=" << mesh_);
            varjo_FreeOcclusionMesh(mesh_);
        }
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        mesh_ = std::exchange(other.mesh_, nullptr);
        view_index_ = std::exchange(other.view_index_, 0);
        winding_order_ = std::exchange(other.winding_order_, varjo_WindingOrder_Clockwise);
        last_error_ = std::move(other.last_error_);
        VTK_SD_LOG("VarjoOcclusionMesh move assignment new mesh=" << mesh_ << " viewIndex=" << view_index_);
    }
    return *this;
}

bool VarjoOcclusionMesh::valid() const
{
    return mesh_ != nullptr;
}

const varjo_Mesh2Df* VarjoOcclusionMesh::get() const
{
    return mesh_;
}

int32_t VarjoOcclusionMesh::viewIndex() const
{
    return view_index_;
}

varjo_WindingOrder VarjoOcclusionMesh::windingOrder() const
{
    return winding_order_;
}

VarjoOcclusionMesh::Snapshot VarjoOcclusionMesh::snapshot() const
{
    Snapshot out{};
    if (!mesh_) {
        VTK_SD_WARN("occlusion mesh snapshot requested with null mesh");
        return out;
    }
    out.vertexCount = mesh_->vertexCount;
    out.valid = true;
    if (mesh_->vertices && mesh_->vertexCount > 0) {
        out.vertices.assign(mesh_->vertices, mesh_->vertices + mesh_->vertexCount);
    }
    VTK_SD_LOG("occlusion mesh snapshot viewIndex=" << view_index_ << " vertexCount=" << out.vertexCount << " copiedVertices=" << out.vertices.size());
    return out;
}

const std::string& VarjoOcclusionMesh::lastError() const
{
    return last_error_;
}

void VarjoOcclusionMesh::setLastError(std::string message) const
{
    last_error_ = std::move(message);
    VTK_SD_ERROR(last_error_);
}
