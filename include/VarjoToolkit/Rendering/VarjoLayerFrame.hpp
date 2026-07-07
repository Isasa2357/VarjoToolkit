#pragma once

#include <Varjo_layers.h>

#include <memory>
#include <string>
#include <vector>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>

class VarjoMultiProjLayer {
public:
    explicit VarjoMultiProjLayer(int32_t viewCount = 0, varjo_LayerFlags flags = varjo_LayerFlagNone, varjo_Space space = varjo_SpaceLocal);

    varjo_LayerMultiProj* get();
    const varjo_LayerMultiProj* get() const;
    varjo_LayerHeader* header();
    const varjo_LayerHeader* header() const;

    int32_t viewCount() const;
    void resize(int32_t viewCount);
    void setFlags(varjo_LayerFlags flags);
    void setSpace(varjo_Space space);

    varjo_LayerMultiProjView& view(size_t index);
    const varjo_LayerMultiProjView& view(size_t index) const;
    void setView(size_t index, const varjo_Matrix& projection, const varjo_Matrix& viewMatrix, const varjo_SwapChainViewport& viewport, varjo_ViewExtension* extension = nullptr);

private:
    void refreshPointers();

private:
    std::vector<varjo_LayerMultiProjView> views_;
    varjo_LayerMultiProj layer_{};
};

class VarjoLayerFrame {
public:
    explicit VarjoLayerFrame(varjo_Session* session);
    explicit VarjoLayerFrame(std::shared_ptr<varjo_Session> session);
    explicit VarjoLayerFrame(const VarjoSession& session);

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Session* session() const;

    bool begin();
    bool end(const std::vector<varjo_LayerHeader*>& layers, int64_t frameNumber);
    bool end(VarjoMultiProjLayer& layer, int64_t frameNumber);
    bool endEmpty(int64_t frameNumber);

    const std::string& lastError() const;

    static varjo_SubmitInfoLayers makeSubmitInfo(const std::vector<varjo_LayerHeader*>& layers, int64_t frameNumber);

private:
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    bool begun_ = false;
    mutable std::string last_error_;
};
