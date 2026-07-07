#pragma once

#include <Varjo_layers.h>

#include <memory>
#include <string>

#include <VarjoToolkit/Core/VarjoSession.hpp>

struct ID3D11Device;
struct ID3D12CommandQueue;

class VarjoSwapChain {
public:
    VarjoSwapChain() = default;
    ~VarjoSwapChain();

    VarjoSwapChain(const VarjoSwapChain&) = delete;
    VarjoSwapChain& operator=(const VarjoSwapChain&) = delete;
    VarjoSwapChain(VarjoSwapChain&& other) noexcept;
    VarjoSwapChain& operator=(VarjoSwapChain&& other) noexcept;

    static VarjoSwapChain createD3D11(varjo_Session* session, ID3D11Device* device, const varjo_SwapChainConfig2& config);
    static VarjoSwapChain createD3D11(std::shared_ptr<varjo_Session> session, ID3D11Device* device, const varjo_SwapChainConfig2& config);
    static VarjoSwapChain createD3D12(varjo_Session* session, ID3D12CommandQueue* queue, const varjo_SwapChainConfig2& config);
    static VarjoSwapChain createD3D12(std::shared_ptr<varjo_Session> session, ID3D12CommandQueue* queue, const varjo_SwapChainConfig2& config);

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Session* session() const;
    varjo_SwapChain* get() const;
    const varjo_SwapChainConfig2& config() const;

    varjo_Texture image(int32_t index) const;
    bool acquire(int32_t& outIndex);
    void release();
    bool acquired() const;
    int32_t acquiredIndex() const;

    varjo_SwapChainViewport fullViewport(int32_t arrayIndex = 0) const;
    const std::string& lastError() const;

    static varjo_SwapChainConfig2 makeConfig(varjo_TextureFormat format, int32_t width, int32_t height, int32_t textureCount = 3, int32_t arraySize = 1);

private:
    explicit VarjoSwapChain(std::shared_ptr<varjo_Session> owner, varjo_Session* session, varjo_SwapChain* swapChain, const varjo_SwapChainConfig2& config);
    void free();
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    varjo_SwapChain* swap_chain_ = nullptr;
    varjo_SwapChainConfig2 config_{};
    bool acquired_ = false;
    int32_t acquired_index_ = -1;
    mutable std::string last_error_;
};
