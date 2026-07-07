#include <VarjoToolkit/Rendering/VarjoSwapChain.hpp>

#include <utility>

VarjoSwapChain::~VarjoSwapChain()
{
    free();
}

VarjoSwapChain::VarjoSwapChain(VarjoSwapChain&& other) noexcept
    : session_owner_(std::move(other.session_owner_))
    , session_(std::exchange(other.session_, nullptr))
    , swap_chain_(std::exchange(other.swap_chain_, nullptr))
    , config_(std::exchange(other.config_, varjo_SwapChainConfig2{}))
    , acquired_(std::exchange(other.acquired_, false))
    , acquired_index_(std::exchange(other.acquired_index_, -1))
    , last_error_(std::move(other.last_error_))
{}

VarjoSwapChain& VarjoSwapChain::operator=(VarjoSwapChain&& other) noexcept
{
    if (this != &other) {
        free();
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        swap_chain_ = std::exchange(other.swap_chain_, nullptr);
        config_ = std::exchange(other.config_, varjo_SwapChainConfig2{});
        acquired_ = std::exchange(other.acquired_, false);
        acquired_index_ = std::exchange(other.acquired_index_, -1);
        last_error_ = std::move(other.last_error_);
    }
    return *this;
}

VarjoSwapChain VarjoSwapChain::createD3D11(varjo_Session* session, ID3D11Device* device, const varjo_SwapChainConfig2& config)
{
    auto mutableConfig = config;
    varjo_SwapChain* chain = nullptr;
    if (session && device) {
        chain = varjo_D3D11CreateSwapChain(session, device, &mutableConfig);
    }
    VarjoSwapChain out(nullptr, session, chain, mutableConfig);
    if (!chain) {
        out.setLastError("failed to create D3D11 Varjo swap chain");
    }
    return out;
}

VarjoSwapChain VarjoSwapChain::createD3D11(std::shared_ptr<varjo_Session> session, ID3D11Device* device, const varjo_SwapChainConfig2& config)
{
    auto mutableConfig = config;
    varjo_SwapChain* chain = nullptr;
    if (session && device) {
        chain = varjo_D3D11CreateSwapChain(session.get(), device, &mutableConfig);
    }
    VarjoSwapChain out(session, session.get(), chain, mutableConfig);
    if (!chain) {
        out.setLastError("failed to create D3D11 Varjo swap chain");
    }
    return out;
}

VarjoSwapChain VarjoSwapChain::createD3D12(varjo_Session* session, ID3D12CommandQueue* queue, const varjo_SwapChainConfig2& config)
{
    auto mutableConfig = config;
    varjo_SwapChain* chain = nullptr;
    if (session && queue) {
        chain = varjo_D3D12CreateSwapChain(session, queue, &mutableConfig);
    }
    VarjoSwapChain out(nullptr, session, chain, mutableConfig);
    if (!chain) {
        out.setLastError("failed to create D3D12 Varjo swap chain");
    }
    return out;
}

VarjoSwapChain VarjoSwapChain::createD3D12(std::shared_ptr<varjo_Session> session, ID3D12CommandQueue* queue, const varjo_SwapChainConfig2& config)
{
    auto mutableConfig = config;
    varjo_SwapChain* chain = nullptr;
    if (session && queue) {
        chain = varjo_D3D12CreateSwapChain(session.get(), queue, &mutableConfig);
    }
    VarjoSwapChain out(session, session.get(), chain, mutableConfig);
    if (!chain) {
        out.setLastError("failed to create D3D12 Varjo swap chain");
    }
    return out;
}

bool VarjoSwapChain::valid() const
{
    return swap_chain_ != nullptr;
}

varjo_Session* VarjoSwapChain::session() const
{
    return session_;
}

varjo_SwapChain* VarjoSwapChain::get() const
{
    return swap_chain_;
}

const varjo_SwapChainConfig2& VarjoSwapChain::config() const
{
    return config_;
}

varjo_Texture VarjoSwapChain::image(int32_t index) const
{
    if (!swap_chain_) {
        return varjo_Texture{};
    }
    return varjo_GetSwapChainImage(swap_chain_, index);
}

bool VarjoSwapChain::acquire(int32_t& outIndex)
{
    outIndex = -1;
    if (!swap_chain_) {
        setLastError("swap chain is null");
        return false;
    }
    if (acquired_) {
        outIndex = acquired_index_;
        return true;
    }
    varjo_AcquireSwapChainImage(swap_chain_, &outIndex);
    acquired_ = (outIndex >= 0);
    acquired_index_ = outIndex;
    if (!acquired_) {
        setLastError("failed to acquire Varjo swap chain image");
    } else {
        last_error_.clear();
    }
    return acquired_;
}

void VarjoSwapChain::release()
{
    if (swap_chain_ && acquired_) {
        varjo_ReleaseSwapChainImage(swap_chain_);
    }
    acquired_ = false;
    acquired_index_ = -1;
}

bool VarjoSwapChain::acquired() const
{
    return acquired_;
}

int32_t VarjoSwapChain::acquiredIndex() const
{
    return acquired_index_;
}

varjo_SwapChainViewport VarjoSwapChain::fullViewport(int32_t arrayIndex) const
{
    varjo_SwapChainViewport viewport{};
    viewport.swapChain = swap_chain_;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = config_.textureWidth;
    viewport.height = config_.textureHeight;
    viewport.arrayIndex = arrayIndex;
    return viewport;
}

const std::string& VarjoSwapChain::lastError() const
{
    return last_error_;
}

varjo_SwapChainConfig2 VarjoSwapChain::makeConfig(varjo_TextureFormat format, int32_t width, int32_t height, int32_t textureCount, int32_t arraySize)
{
    varjo_SwapChainConfig2 config{};
    config.textureFormat = format;
    config.numberOfTextures = textureCount;
    config.textureWidth = width;
    config.textureHeight = height;
    config.textureArraySize = arraySize;
    return config;
}

VarjoSwapChain::VarjoSwapChain(std::shared_ptr<varjo_Session> owner, varjo_Session* session, varjo_SwapChain* swapChain, const varjo_SwapChainConfig2& config)
    : session_owner_(std::move(owner))
    , session_(session)
    , swap_chain_(swapChain)
    , config_(config)
{}

void VarjoSwapChain::free()
{
    release();
    if (swap_chain_) {
        varjo_FreeSwapChain(swap_chain_);
        swap_chain_ = nullptr;
    }
}

void VarjoSwapChain::setLastError(std::string message) const
{
    last_error_ = std::move(message);
}
