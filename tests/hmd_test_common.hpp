#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>
#include <VarjoToolkit/Rendering/VarjoSwapChain.hpp>

#include <Varjo_d3d11.h>

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vtk_hmd_test {

using Microsoft::WRL::ComPtr;

inline void require(bool condition, const char* expression, const char* file, int line)
{
    if (!condition) {
        std::ostringstream oss;
        oss << file << '(' << line << "): requirement failed: " << expression;
        throw std::runtime_error(oss.str());
    }
}

inline void requireMessage(bool condition, const std::string& message, const char* file, int line)
{
    if (!condition) {
        std::ostringstream oss;
        oss << file << '(' << line << "): requirement failed: " << message;
        throw std::runtime_error(oss.str());
    }
}

inline void throwIfFailed(HRESULT hr, const char* what)
{
    if (FAILED(hr)) {
        std::ostringstream oss;
        oss << what << " failed with HRESULT=0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
        throw std::runtime_error(oss.str());
    }
}

template <typename Fn>
int runTest(const char* testName, Fn&& fn)
{
    try {
        std::cout << "[ RUN      ] " << testName << '\n';
        fn();
        std::cout << "[       OK ] " << testName << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[  FAILED  ] " << testName << ": " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "[  FAILED  ] " << testName << ": unknown exception\n";
        return 1;
    }
}

inline varjo_Matrix copyMatrix(const double* values)
{
    varjo_Matrix out{};
    std::copy(values, values + 16, out.value);
    return out;
}

inline bool isFiniteMatrix(const double* values)
{
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(values[i])) {
            return false;
        }
    }
    return true;
}

inline bool sameLuid(const DXGI_ADAPTER_DESC1& desc, const varjo_Luid& luid)
{
    return desc.AdapterLuid.HighPart == luid.high && static_cast<std::uint32_t>(desc.AdapterLuid.LowPart) == luid.low;
}

inline ComPtr<IDXGIAdapter1> findVarjoAdapter(varjo_Session* session)
{
    require(session != nullptr, "session != nullptr", __FILE__, __LINE__);

    const varjo_Luid luid = varjo_D3D11GetLuid(session);
    std::cout << "Varjo D3D LUID high=" << luid.high << " low=" << luid.low << '\n';

    ComPtr<IDXGIFactory1> factory;
    throwIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");

    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc)) && sameLuid(desc, luid)) {
            std::wcout << L"Using Varjo adapter: " << desc.Description << L"\n";
            return adapter;
        }
    }

    throw std::runtime_error("Could not find a DXGI adapter matching the Varjo runtime LUID");
}

inline ComPtr<ID3D11Device> createD3D11DeviceForVarjo(varjo_Session* session, ComPtr<ID3D11DeviceContext>& outContext)
{
    auto adapter = findVarjoAdapter(session);

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL createdLevel{};
    ComPtr<ID3D11Device> device;
    throwIfFailed(
        D3D11CreateDevice(
            adapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            flags,
            featureLevels,
            static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION,
            &device,
            &createdLevel,
            &outContext),
        "D3D11CreateDevice");

    std::cout << "Created D3D11 device. featureLevel=0x" << std::hex << createdLevel << std::dec << '\n';
    return device;
}

inline DXGI_FORMAT rtvFormatFor(DXGI_FORMAT resourceFormat)
{
    switch (resourceFormat) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    default:
        return resourceFormat;
    }
}

inline int32_t recommendedWidth(const VarjoFrameInfo& frameInfo)
{
    int32_t width = 1;
    for (int32_t i = 0; i < frameInfo.viewCount(); ++i) {
        width = std::max(width, frameInfo.view(i).preferredWidth);
    }
    return width;
}

inline int32_t recommendedHeight(const VarjoFrameInfo& frameInfo)
{
    int32_t height = 1;
    for (int32_t i = 0; i < frameInfo.viewCount(); ++i) {
        height = std::max(height, frameInfo.view(i).preferredHeight);
    }
    return height;
}

inline int32_t chooseSmokeDimension(int32_t preferred, int32_t minimum, int32_t maximum)
{
    int32_t value = std::max<int32_t>(1, std::min<int32_t>(preferred, 1024));
    if (minimum > 0) {
        value = std::max(value, minimum);
    }
    if (maximum > 0) {
        value = std::min(value, maximum);
    }
    return std::max<int32_t>(1, value);
}

inline int32_t chooseSmokeTextureCount(const varjo_SwapChainLimits& limits)
{
    int32_t value = std::max<int32_t>(3, limits.minimumNumberOfTextures);
    if (limits.maximumNumberOfTextures > 0) {
        value = std::min(value, limits.maximumNumberOfTextures);
    }
    return std::max<int32_t>(1, value);
}

inline varjo_SwapChainConfig2 makeSmokeColorSwapChainConfig(varjo_Session* session, const VarjoFrameInfo& frameInfo)
{
    require(session != nullptr, "session != nullptr", __FILE__, __LINE__);
    require(frameInfo.viewCount() > 0, "frameInfo.viewCount() > 0", __FILE__, __LINE__);

    const varjo_SwapChainLimits limits = varjo_GetSwapChainLimits(session);
    const int32_t width = chooseSmokeDimension(recommendedWidth(frameInfo), limits.minimumTextureWidth, limits.maximumTextureWidth);
    const int32_t height = chooseSmokeDimension(recommendedHeight(frameInfo), limits.minimumTextureHeight, limits.maximumTextureHeight);
    const int32_t textureCount = chooseSmokeTextureCount(limits);
    const int32_t arraySize = frameInfo.viewCount();

    std::cout
        << "SwapChainLimits textures=[" << limits.minimumNumberOfTextures << ',' << limits.maximumNumberOfTextures << "]"
        << " width=[" << limits.minimumTextureWidth << ',' << limits.maximumTextureWidth << "]"
        << " height=[" << limits.minimumTextureHeight << ',' << limits.maximumTextureHeight << "]\n"
        << "Using smoke swapchain config width=" << width
        << " height=" << height
        << " textures=" << textureCount
        << " arraySize=" << arraySize << '\n';

    return VarjoSwapChain::makeConfig(varjo_TextureFormat_R8G8B8A8_SRGB, width, height, textureCount, arraySize);
}

inline void updateLayerViews(VarjoMultiProjLayer& layer, const VarjoFrameInfo& frameInfo, const VarjoSwapChain& swapChain)
{
    layer.resize(frameInfo.viewCount());
    for (int32_t viewIndex = 0; viewIndex < frameInfo.viewCount(); ++viewIndex) {
        const auto& viewInfo = frameInfo.view(viewIndex);
        layer.setView(
            static_cast<size_t>(viewIndex),
            copyMatrix(viewInfo.projectionMatrix),
            copyMatrix(viewInfo.viewMatrix),
            swapChain.fullViewport(viewIndex));
    }
}

} // namespace vtk_hmd_test

#define VTK_HMD_TEST_REQUIRE(expression) \
    ::vtk_hmd_test::require(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

#define VTK_HMD_TEST_REQUIRE_MESSAGE(expression, message) \
    ::vtk_hmd_test::requireMessage(static_cast<bool>(expression), (message), __FILE__, __LINE__)
