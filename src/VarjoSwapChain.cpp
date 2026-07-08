#include <VarjoToolkit/Rendering/VarjoSwapChain.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>

#include <utility>

namespace {

void logSwapChainConfig(const char* label, const varjo_SwapChainConfig2& config)
{
    VTK_SD_LOG(label
        << " format=" << static_cast<int64_t>(config.textureFormat)
        << " width=" << config.textureWidth
        << " height=" << config.textureHeight
        << " textures=" << config.numberOfTextures
        << " arraySize=" << config.textureArraySize);
}

} // namespace

VarjoSwapChain::~VarjoSwapChain()
{
    VTK_SD_LOG("VarjoSwapChain destructor swap_chain=" << swap_chain_ << " acquired=" << (acquired_ ? "true" : "false"));
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
{
    VTK_SD_LOG("VarjoSwapChain move constructor swap_chain=" << swap_chain_ << " acquired=" << (acquired_ ? "true" : "false") << " acquiredIndex=" << acquired_index_);
}

VarjoSwapChain& VarjoSwapChain::operator=(VarjoSwapChain&& other) noexcept
{
    if (this != &other) {
        VTK_SD_LOG("VarjoSwapChain move assignment releasing current swap_chain=" << swap_chain_);
        free();
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        swap_chain_ = std::exchange(other.swap_chain_, nullptr);
        config_ = std::exchange(other.config_, varjo_SwapChainConfig2{});
        acquired_ = std::exchange(other.acquired_, false);
        acquired_index_ = std::exchange(other.acquired_index_, -1);
        last_error_ = std::move(other.last_error_);
        VTK_SD_LOG("VarjoSwapChain move assignment new swap_chain=" << swap_chain_ << " acquired=" << (acquired_ ? "true" : "false") << " acquiredIndex=" << acquired_index_);
    }
    return *this;
}

VarjoSwapChain VarjoSwapChain::createD3D11(varjo_Session* session, ID3D11Device* device, const varjo_SwapChainConfig2& config)
{
    VTK_SD_SCOPE("VarjoSwapChain::createD3D11 raw");
    logSwapChainConfig("D3D11 swapchain requested", config);
    auto mutableConfig = config;
    varjo_SwapChain* chain = nullptr;
    if (session && device) {
        chain = varjo_D3D11CreateSwapChain(session, device, &mutableConfig);
    } else {
        VTK_SD_ERROR("D3D11 swapchain null input session=" << session << " device=" << device);
    }
    VarjoSwapChain out(nullptr, session, chain, mutableConfig);
    if (!chain) {
        out.setLastError("failed to create D3D11 Varjo swap chain");
    } else {
        VTK_SD_LOG("D3D11 swapchain created swap_chain=" << chain);
    }
    return out;
}

VarjoSwapChain VarjoSwapChain::createD3D11(std::shared_ptr<varjo_Session> session, ID3D11Device* device, const varjo_SwapChainConfig2& config)
{
    VTK_SD_SCOPE("VarjoSwapChain::createD3D11 shared");
    logSwapChainConfig("D3D11 swapchain requested", config);
    auto mutableConfig = config;
    varjo_SwapChain* chain = nullptr;
    if (session && device) {
        chain = varjo_D3D11CreateSwapChain(session.get(), device, &mutableConfig);
    } else {
        VTK_SD_ERROR("D3D11 swapchain null input session=" << session.get() << " device=" << device);
    }
    VarjoSwapChain out(session, session.get(), chain, mutableConfig);
    if (!chain) {
        out.setLastError("failed to create D3D11 Varjo swap chain");
    } else {
        VTK_SD_LOG("D3D11 swapchain created swap_chain=" << chain);
    }
    return out;
}

VarjoSwapChain VarjoSwapChain::createD3D12(varjo_Session* session, ID3D12CommandQueue* queue, const varjo_SwapChainConfig2& config)
{
    VTK_SD_SCOPE("VarjoSwapChain::createD3D12 raw");
    logSwapChainConfig("D3D12 swapchain requested", config);
    auto mutableConfig = config;
    varjo_SwapChain* chain = nullptr;
    if (session && queue) {
        chain = varjo_D3D12CreateSwapChain(session, queue, &mutableConfig);
    } else {
        VTK_SD_ERROR("D3D12 swapchain null input session=" << session << " queue=" << queue);
    }
    VarjoSwapChain out(nullptr, session, chain, mutableConfig);
    if (!chain) {
        out.setLastError("failed to create D3D12 Varjo swap chain");
    } else {
        VTK_SD_LOG("D3D12 swapchain created swap_chain=" << chain);
    }
    return out;
}

VarjoSwapChain VarjoSwapChain::createD3D12(std::shared_ptr<varjo_Session> session, ID3D12CommandQueue* queue, const varjo_SwapChainConfig2& config)
{
    VTK_SD_SCOPE("VarjoSwapChain::createD3D12 shared");
    logSwapChainConfig("D3D12 swapchain requested", config);
    auto mutableConfig = config;
    varjo_SwapChain* chain = nullptr;
    if (session && queue) {
        chain = varjo_D3D12CreateSwapChain(session.get(), queue, &mutableConfig);
    } else {
        VTK_SD_ERROR("D3D12 swapchain null input session=" << session.get() << " queue=" << queue);
    }
    VarjoSwapChain out(session, session.get(), chain, mutableConfig);
    if (!chain) {
        out.setLastError("failed to create D3D12 Varjo swap chain");
    } else {
        VTK_SD_LOG("D3D12 swapchain created swap_chain=" << chain);
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
        VTK_SD_ERROR("image requested with null swap_chain index=" << index);
        return varjo_Texture{};
    }
    VTK_SD_TRACE("varjo_GetSwapChainImage index=" << index);
    return varjo_GetSwapChainImage(swap_chain_, index);
}

bool VarjoSwapChain::acquire(int32_t& outIndex)
{
    VTK_SD_SCOPE("VarjoSwapChain::acquire");
    outIndex = -1;
    if (!swap_chain_) {
        setLastError("swap chain is null");
        return false;
    }
    if (acquired_) {
        outIndex = acquired_index_;
        VTK_SD_WARN("acquire called while already acquired acquiredIndex=" << acquired_index_);
        return true;
    }
    varjo_AcquireSwapChainImage(swap_chain_, &outIndex);
    acquired_ = (outIndex >= 0);
    acquired_index_ = outIndex;
    if (!acquired_) {
        setLastError("failed to acquire Varjo swap chain image");
    } else {
        last_error_.clear();
        VTK_SD_LOG("swapchain acquired index=" << acquired_index_);
    }
    return acquired_;
}

void VarjoSwapChain::release()
{
    if (swap_chain_ && acquired_) {
        VTK_SD_LOG("varjo_ReleaseSwapChainImage acquiredIndex=" << acquired_index_);
        varjo_ReleaseSwapChainImage(swap_chain_);
    } else if (swap_chain_ && !acquired_) {
        VTK_SD_TRACE("release called with no acquired image");
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
    VTK_SD_TRACE("fullViewport arrayIndex=" << arrayIndex << " width=" << viewport.width << " height=" << viewport.height);
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
    logSwapChainConfig("makeConfig", config);
    return config;
}

VarjoSwapChain::VarjoSwapChain(std::shared_ptr<varjo_Session> owner, varjo_Session* session, varjo_SwapChain* swapChain, const varjo_SwapChainConfig2& config)
    : session_owner_(std::move(owner))
    , session_(session)
    , swap_chain_(swapChain)
    , config_(config)
{
    VTK_SD_LOG("VarjoSwapChain constructed session=" << session_ << " swap_chain=" << swap_chain_);
}

void VarjoSwapChain::free()
{
    release();
    if (swap_chain_) {
        VTK_SD_LOG("varjo_FreeSwapChain swap_chain=" << swap_chain_);
        varjo_FreeSwapChain(swap_chain_);
        swap_chain_ = nullptr;
    }
}

void VarjoSwapChain::setLastError(std::string message) const
{
    last_error_ = std::move(message);
    VTK_SD_ERROR(last_error_);
}
