#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>

#include <stdexcept>
#include <utility>

namespace {

varjo_LayerFlags ResolveLayerFlags(varjo_LayerFlags flags) noexcept
{
    // Most VarjoToolkit/VarjoXR use cases are MR overlay layers rendered onto a
    // transparent render target. Submitting varjo_LayerFlagNone can make the
    // compositor treat the transparent clear color as an opaque black layer.
    // Keep explicit non-zero flags untouched, but make the default safe for MR.
    if (flags == varjo_LayerFlagNone) {
        return varjo_LayerFlag_BlendMode_AlphaBlend;
    }
    return flags;
}

} // namespace

VarjoMultiProjLayer::VarjoMultiProjLayer(int32_t viewCount, varjo_LayerFlags flags, varjo_Space space)
{
    layer_.header.type = varjo_LayerMultiProjType;
    layer_.header.flags = ResolveLayerFlags(flags);
    layer_.space = space;
    resize(viewCount);
}

varjo_LayerMultiProj* VarjoMultiProjLayer::get()
{
    refreshPointers();
    return &layer_;
}

const varjo_LayerMultiProj* VarjoMultiProjLayer::get() const
{
    return &layer_;
}

varjo_LayerHeader* VarjoMultiProjLayer::header()
{
    refreshPointers();
    return &layer_.header;
}

const varjo_LayerHeader* VarjoMultiProjLayer::header() const
{
    return &layer_.header;
}

int32_t VarjoMultiProjLayer::viewCount() const
{
    return static_cast<int32_t>(views_.size());
}

void VarjoMultiProjLayer::resize(int32_t viewCount)
{
    if (viewCount < 0) {
        viewCount = 0;
    }
    views_.resize(static_cast<size_t>(viewCount));
    refreshPointers();
}

void VarjoMultiProjLayer::setFlags(varjo_LayerFlags flags)
{
    layer_.header.flags = ResolveLayerFlags(flags);
}

void VarjoMultiProjLayer::setSpace(varjo_Space space)
{
    layer_.space = space;
}

varjo_LayerMultiProjView& VarjoMultiProjLayer::view(size_t index)
{
    if (index >= views_.size()) {
        throw std::out_of_range("VarjoMultiProjLayer view index out of range");
    }
    return views_[index];
}

const varjo_LayerMultiProjView& VarjoMultiProjLayer::view(size_t index) const
{
    if (index >= views_.size()) {
        throw std::out_of_range("VarjoMultiProjLayer view index out of range");
    }
    return views_[index];
}

void VarjoMultiProjLayer::setView(size_t index, const varjo_Matrix& projection, const varjo_Matrix& viewMatrix, const varjo_SwapChainViewport& viewport, varjo_ViewExtension* extension)
{
    auto& v = view(index);
    v.extension = extension;
    v.projection = projection;
    v.view = viewMatrix;
    v.viewport = viewport;
}

void VarjoMultiProjLayer::refreshPointers()
{
    layer_.viewCount = static_cast<int32_t>(views_.size());
    layer_.views = views_.empty() ? nullptr : views_.data();
}

VarjoLayerFrame::VarjoLayerFrame(varjo_Session* session)
    : session_(session)
{}

VarjoLayerFrame::VarjoLayerFrame(std::shared_ptr<varjo_Session> session)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
{}

VarjoLayerFrame::VarjoLayerFrame(const VarjoSession& session)
    : VarjoLayerFrame(session.shared())
{}

bool VarjoLayerFrame::valid() const
{
    return session_ != nullptr;
}

varjo_Session* VarjoLayerFrame::session() const
{
    return session_;
}

bool VarjoLayerFrame::begin()
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }
    varjo_BeginFrameWithLayers(session_);
    begun_ = true;
    last_error_.clear();
    return true;
}

bool VarjoLayerFrame::end(const std::vector<varjo_LayerHeader*>& layers, int64_t frameNumber)
{
    if (!session_) {
        setLastError("session is null");
        return false;
    }
    auto mutableLayers = layers;
    varjo_SubmitInfoLayers submitInfo = makeSubmitInfo(mutableLayers, frameNumber);
    varjo_EndFrameWithLayers(session_, &submitInfo);
    begun_ = false;
    last_error_.clear();
    return true;
}

bool VarjoLayerFrame::end(VarjoMultiProjLayer& layer, int64_t frameNumber)
{
    std::vector<varjo_LayerHeader*> layers{layer.header()};
    return end(layers, frameNumber);
}

bool VarjoLayerFrame::endEmpty(int64_t frameNumber)
{
    std::vector<varjo_LayerHeader*> layers;
    return end(layers, frameNumber);
}

const std::string& VarjoLayerFrame::lastError() const
{
    return last_error_;
}

varjo_SubmitInfoLayers VarjoLayerFrame::makeSubmitInfo(const std::vector<varjo_LayerHeader*>& layers, int64_t frameNumber)
{
    varjo_SubmitInfoLayers submitInfo{};
    submitInfo.frameNumber = frameNumber;
    submitInfo.layerCount = static_cast<int32_t>(layers.size());
    submitInfo.layers = layers.empty() ? nullptr : const_cast<varjo_LayerHeader**>(layers.data());
    return submitInfo;
}

void VarjoLayerFrame::setLastError(std::string message) const
{
    last_error_ = std::move(message);
}
