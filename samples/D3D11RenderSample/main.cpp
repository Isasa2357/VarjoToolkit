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
#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

std::atomic_bool g_stopRequested{false};

BOOL WINAPI consoleCtrlHandler(DWORD eventType)
{
    switch (eventType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        g_stopRequested.store(true);
        return TRUE;
    default:
        return FALSE;
    }
}

void throwIfFailed(HRESULT result, const char* operation)
{
    if (FAILED(result)) {
        throw std::runtime_error(std::string(operation) + " failed");
    }
}

bool sameLuid(const DXGI_ADAPTER_DESC1& description, const varjo_Luid& luid)
{
    return description.AdapterLuid.HighPart == luid.high &&
        static_cast<uint32_t>(description.AdapterLuid.LowPart) == luid.low;
}

ComPtr<IDXGIAdapter1> findVarjoAdapter(varjo_Session* session)
{
    const varjo_Luid luid = varjo_D3D11GetLuid(session);
    ComPtr<IDXGIFactory1> factory;
    throwIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");

    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 description{};
        if (SUCCEEDED(adapter->GetDesc1(&description)) &&
            sameLuid(description, luid)) {
            return adapter;
        }
    }
    throw std::runtime_error("Varjo DXGI adapter was not found");
}

varjo_Matrix copyMatrix(const double values[16])
{
    varjo_Matrix output{};
    std::copy(values, values + 16, output.value);
    return output;
}

DXGI_FORMAT rtvFormatFor(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    default:
        return format;
    }
}

int recommendedWidth(const VarjoFrameInfo& frameInfo)
{
    int output = 1;
    for (int32_t index = 0; index < frameInfo.viewCount(); ++index) {
        output = std::max(output, frameInfo.view(index).preferredWidth);
    }
    return output;
}

int recommendedHeight(const VarjoFrameInfo& frameInfo)
{
    int output = 1;
    for (int32_t index = 0; index < frameInfo.viewCount(); ++index) {
        output = std::max(output, frameInfo.view(index).preferredHeight);
    }
    return output;
}

void updateLayerViews(
    VarjoMultiProjLayer& layer,
    const VarjoFrameInfo& frameInfo,
    const VarjoSwapChain& swapChain)
{
    for (int32_t index = 0; index < frameInfo.viewCount(); ++index) {
        const auto& view = frameInfo.view(index);
        layer.setView(
            static_cast<size_t>(index),
            copyMatrix(view.projectionMatrix),
            copyMatrix(view.viewMatrix),
            swapChain.fullViewport(index));
    }
}

} // namespace

int main(int argc, char** argv)
{
    int frameLimit = 300;
    if (argc == 3 && std::string(argv[1]) == "--frames") {
        frameLimit = std::max(0, std::stoi(argv[2]));
    }

    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    try {
        VarjoSession session;
        if (!session.valid()) {
            std::cerr << session.lastError() << '\n';
            return 1;
        }

        VarjoFrameInfo frameInfo(session);
        if (!frameInfo.valid()) return 1;
        varjo_WaitSync(session.get(), frameInfo.get());

        const int32_t viewCount = frameInfo.viewCount();
        const int width = recommendedWidth(frameInfo);
        const int height = recommendedHeight(frameInfo);

        auto adapter = findVarjoAdapter(session.get());
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL featureLevel{};
        throwIfFailed(
            D3D11CreateDevice(
                adapter.Get(),
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                nullptr,
                0,
                D3D11_SDK_VERSION,
                &device,
                &featureLevel,
                &context),
            "D3D11CreateDevice");

        auto config = VarjoSwapChain::makeConfig(
            varjo_TextureFormat_R8G8B8A8_SRGB,
            width,
            height,
            3,
            viewCount);
        auto swapChain = VarjoSwapChain::createD3D11(
            session.shared(),
            device.Get(),
            config);
        if (!swapChain.valid()) {
            std::cerr << swapChain.lastError() << '\n';
            return 1;
        }

        std::vector<ComPtr<ID3D11RenderTargetView>> renderTargets(
            static_cast<size_t>(config.numberOfTextures) *
            static_cast<size_t>(config.textureArraySize));
        for (int32_t imageIndex = 0;
             imageIndex < config.numberOfTextures;
             ++imageIndex) {
            ID3D11Texture2D* texture = varjo_ToD3D11Texture(
                swapChain.image(imageIndex));
            D3D11_TEXTURE2D_DESC textureDescription{};
            texture->GetDesc(&textureDescription);
            for (int32_t arrayIndex = 0;
                 arrayIndex < config.textureArraySize;
                 ++arrayIndex) {
                D3D11_RENDER_TARGET_VIEW_DESC rtvDescription{};
                rtvDescription.Format = rtvFormatFor(textureDescription.Format);
                rtvDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDescription.Texture2DArray.MipSlice = 0;
                rtvDescription.Texture2DArray.FirstArraySlice = arrayIndex;
                rtvDescription.Texture2DArray.ArraySize = 1;
                throwIfFailed(
                    device->CreateRenderTargetView(
                        texture,
                        &rtvDescription,
                        &renderTargets[
                            static_cast<size_t>(imageIndex) * viewCount +
                            static_cast<size_t>(arrayIndex)]),
                    "CreateRenderTargetView");
            }
        }

        VarjoLayerFrame layerFrame(session.shared());
        VarjoMultiProjLayer layer(
            viewCount,
            varjo_LayerFlagNone,
            varjo_SpaceLocal);

        int renderedFrames = 0;
        while (!g_stopRequested.load() &&
               (frameLimit == 0 || renderedFrames < frameLimit)) {
            // The sample application is the sole synchronization owner.
            varjo_WaitSync(session.get(), frameInfo.get());
            if (!layerFrame.begin()) break;

            int32_t imageIndex = -1;
            if (!swapChain.acquire(imageIndex)) {
                layerFrame.endEmpty(frameInfo.frameNumber());
                break;
            }

            for (int32_t viewIndex = 0;
                 viewIndex < viewCount;
                 ++viewIndex) {
                ID3D11RenderTargetView* target = renderTargets[
                    static_cast<size_t>(imageIndex) * viewCount +
                    static_cast<size_t>(viewIndex)].Get();
                const float clear[4] = {
                    0.04f + 0.03f * static_cast<float>(viewIndex),
                    0.08f,
                    0.16f,
                    1.0f};
                context->OMSetRenderTargets(1, &target, nullptr);
                context->ClearRenderTargetView(target, clear);
            }
            context->Flush();

            updateLayerViews(layer, frameInfo, swapChain);
            swapChain.release();
            if (!layerFrame.end(layer, frameInfo.frameNumber())) break;
            ++renderedFrames;
        }

        std::cout << "Rendered frames: " << renderedFrames << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
