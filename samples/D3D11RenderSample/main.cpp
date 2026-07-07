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
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
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

struct Options {
    int frames = 300;
    bool help = false;
};

void printUsage()
{
    std::cout
        << "VarjoD3D11RenderSample\n"
        << "\n"
        << "Usage:\n"
        << "  VarjoD3D11RenderSample.exe [options]\n"
        << "\n"
        << "Options:\n"
        << "  --frames <n>    Number of frames to render. 0 means until Ctrl+C. Default: 300\n"
        << "  --help          Show this message\n";
}

bool parseInt(const char* text, int& value)
{
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(text, &consumed, 10);
        if (consumed == std::string(text).size()) {
            value = parsed;
            return true;
        }
    } catch (...) {
    }
    return false;
}

bool parseArguments(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            options.help = true;
            return true;
        }
        if (arg == "--frames") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --frames\n";
                return false;
            }
            int frames = 0;
            if (!parseInt(argv[++i], frames) || frames < 0) {
                std::cerr << "Invalid --frames value\n";
                return false;
            }
            options.frames = frames;
            continue;
        }
        std::cerr << "Unknown option: " << arg << "\n";
        return false;
    }
    return true;
}

void throwIfFailed(HRESULT hr, const char* what)
{
    if (FAILED(hr)) {
        throw std::runtime_error(std::string(what) + " failed with HRESULT=0x" + std::to_string(static_cast<unsigned long>(hr)));
    }
}

varjo_Matrix copyMatrix(const double* values)
{
    varjo_Matrix out{};
    std::copy(values, values + 16, out.value);
    return out;
}

bool sameLuid(const DXGI_ADAPTER_DESC1& desc, const varjo_Luid& luid)
{
    return desc.AdapterLuid.HighPart == luid.high && static_cast<uint32_t>(desc.AdapterLuid.LowPart) == luid.low;
}

ComPtr<IDXGIAdapter1> findVarjoAdapter(varjo_Session* session)
{
    const varjo_Luid luid = varjo_D3D11GetLuid(session);

    ComPtr<IDXGIFactory1> factory;
    throwIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");

    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc)) && sameLuid(desc, luid)) {
            std::wcout << L"Using adapter: " << desc.Description << L"\n";
            return adapter;
        }
    }

    throw std::runtime_error("Could not find DXGI adapter matching Varjo runtime LUID");
}

ComPtr<ID3DBlob> compileShader(const char* source, const char* entry, const char* target)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompile(
        source,
        std::strlen(source),
        nullptr,
        nullptr,
        nullptr,
        entry,
        target,
        flags,
        0,
        &bytecode,
        &errors);

    if (FAILED(hr)) {
        if (errors) {
            std::cerr << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
        }
        throwIfFailed(hr, target);
    }
    return bytecode;
}

DXGI_FORMAT rtvFormatFor(DXGI_FORMAT resourceFormat)
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

const char* kVertexShader = R"hlsl(
struct VSOut {
    float4 position : SV_POSITION;
    float3 color : COLOR0;
};

VSOut main(uint vertexId : SV_VertexID)
{
    float2 positions[3] = {
        float2( 0.0f,  0.65f),
        float2( 0.65f, -0.55f),
        float2(-0.65f, -0.55f)
    };
    float3 colors[3] = {
        float3(1.0f, 0.2f, 0.2f),
        float3(0.2f, 1.0f, 0.2f),
        float3(0.2f, 0.4f, 1.0f)
    };

    VSOut output;
    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    output.color = colors[vertexId];
    return output;
}
)hlsl";

const char* kPixelShader = R"hlsl(
float4 main(float4 position : SV_POSITION, float3 color : COLOR0) : SV_TARGET
{
    return float4(color, 1.0f);
}
)hlsl";

class D3D11TriangleRenderer {
public:
    void initialize(varjo_Session* session)
    {
        auto adapter = findVarjoAdapter(session);

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        const UINT featureLevelCount = static_cast<UINT>(sizeof(featureLevels) / sizeof(featureLevels[0]));
        D3D_FEATURE_LEVEL createdLevel{};
        throwIfFailed(
            D3D11CreateDevice(
                adapter.Get(),
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                flags,
                featureLevels,
                featureLevelCount,
                D3D11_SDK_VERSION,
                &device_,
                &createdLevel,
                &context_),
            "D3D11CreateDevice");

        auto vs = compileShader(kVertexShader, "main", "vs_5_0");
        auto ps = compileShader(kPixelShader, "main", "ps_5_0");
        throwIfFailed(device_->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertex_shader_), "CreateVertexShader");
        throwIfFailed(device_->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixel_shader_), "CreatePixelShader");
    }

    ID3D11Device* device() const { return device_.Get(); }

    void createRenderTargets(const VarjoSwapChain& swapChain)
    {
        render_targets_.clear();
        const auto& config = swapChain.config();
        render_targets_.resize(static_cast<size_t>(config.numberOfTextures) * static_cast<size_t>(config.textureArraySize));

        for (int32_t imageIndex = 0; imageIndex < config.numberOfTextures; ++imageIndex) {
            const varjo_Texture texture = swapChain.image(imageIndex);
            ID3D11Texture2D* d3dTexture = varjo_ToD3D11Texture(texture);
            if (!d3dTexture) {
                throw std::runtime_error("varjo_ToD3D11Texture returned null");
            }

            D3D11_TEXTURE2D_DESC textureDesc{};
            d3dTexture->GetDesc(&textureDesc);

            for (int32_t arrayIndex = 0; arrayIndex < config.textureArraySize; ++arrayIndex) {
                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
                rtvDesc.Format = rtvFormatFor(textureDesc.Format);
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = 0;
                rtvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(arrayIndex);
                rtvDesc.Texture2DArray.ArraySize = 1;

                ComPtr<ID3D11RenderTargetView> rtv;
                throwIfFailed(device_->CreateRenderTargetView(d3dTexture, &rtvDesc, &rtv), "CreateRenderTargetView");
                render_targets_[targetIndex(imageIndex, arrayIndex, config.textureArraySize)] = rtv;
            }
        }
    }

    void render(int32_t imageIndex, int32_t viewCount, int32_t width, int32_t height)
    {
        if (!context_) {
            throw std::runtime_error("D3D11 context is null");
        }

        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vertex_shader_.Get(), nullptr, 0);
        context_->PSSetShader(pixel_shader_.Get(), nullptr, 0);

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &viewport);

        for (int32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
            auto rtv = render_targets_.at(targetIndex(imageIndex, viewIndex, viewCount)).Get();
            const float clearColor[4] = {
                0.02f + 0.04f * static_cast<float>(viewIndex),
                0.04f,
                0.08f + 0.03f * static_cast<float>(viewIndex),
                1.0f};
            context_->OMSetRenderTargets(1, &rtv, nullptr);
            context_->ClearRenderTargetView(rtv, clearColor);
            context_->Draw(3, 0);
        }

        ID3D11RenderTargetView* nullRtv = nullptr;
        context_->OMSetRenderTargets(1, &nullRtv, nullptr);
        context_->Flush();
    }

private:
    static size_t targetIndex(int32_t imageIndex, int32_t arrayIndex, int32_t arraySize)
    {
        return static_cast<size_t>(imageIndex) * static_cast<size_t>(arraySize) + static_cast<size_t>(arrayIndex);
    }

private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11VertexShader> vertex_shader_;
    ComPtr<ID3D11PixelShader> pixel_shader_;
    std::vector<ComPtr<ID3D11RenderTargetView>> render_targets_;
};

void updateLayerViews(VarjoMultiProjLayer& layer, const VarjoFrameInfo& frameInfo, const VarjoSwapChain& swapChain)
{
    for (int32_t viewIndex = 0; viewIndex < frameInfo.viewCount(); ++viewIndex) {
        const auto& viewInfo = frameInfo.view(viewIndex);
        layer.setView(
            static_cast<size_t>(viewIndex),
            copyMatrix(viewInfo.projectionMatrix),
            copyMatrix(viewInfo.viewMatrix),
            swapChain.fullViewport(viewIndex));
    }
}

int recommendedWidth(const VarjoFrameInfo& frameInfo)
{
    int width = 1;
    for (int32_t i = 0; i < frameInfo.viewCount(); ++i) {
        width = std::max(width, frameInfo.view(i).preferredWidth);
    }
    return width;
}

int recommendedHeight(const VarjoFrameInfo& frameInfo)
{
    int height = 1;
    for (int32_t i = 0; i < frameInfo.viewCount(); ++i) {
        height = std::max(height, frameInfo.view(i).preferredHeight);
    }
    return height;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseArguments(argc, argv, options)) {
        printUsage();
        return 1;
    }
    if (options.help) {
        printUsage();
        return 0;
    }

    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    try {
        std::cout << "Initializing Varjo session...\n";
        VarjoSession session;
        if (!session) {
            std::cerr << "Varjo session initialization failed: " << session.lastError() << "\n";
            return 1;
        }

        VarjoFrameInfo frameInfo(session);
        if (!frameInfo || !frameInfo.waitSync()) {
            std::cerr << "Failed to create or wait Varjo frame info.\n";
            return 1;
        }

        const int32_t viewCount = frameInfo.viewCount();
        if (viewCount <= 0) {
            std::cerr << "Varjo runtime returned no views.\n";
            return 1;
        }

        const int width = recommendedWidth(frameInfo);
        const int height = recommendedHeight(frameInfo);
        std::cout << "Views=" << viewCount << " renderSize=" << width << "x" << height << "\n";

        D3D11TriangleRenderer renderer;
        renderer.initialize(session.get());

        auto config = VarjoSwapChain::makeConfig(varjo_TextureFormat_R8G8B8A8_SRGB, width, height, 3, viewCount);
        VarjoSwapChain swapChain = VarjoSwapChain::createD3D11(session.shared(), renderer.device(), config);
        if (!swapChain) {
            std::cerr << "Failed to create D3D11 Varjo swap chain: " << swapChain.lastError() << "\n";
            return 1;
        }
        renderer.createRenderTargets(swapChain);

        VarjoLayerFrame layerFrame(session.shared());
        VarjoMultiProjLayer layer(viewCount, varjo_LayerFlagNone, varjo_SpaceLocal);

        std::cout << "Rendering D3D11 frames. Press Ctrl+C to stop.\n";
        int renderedFrames = 0;
        while (!g_stopRequested.load() && (options.frames == 0 || renderedFrames < options.frames)) {
            if (!frameInfo.waitSync()) {
                std::cerr << "waitSync failed.\n";
                break;
            }
            if (!layerFrame.begin()) {
                std::cerr << "begin frame failed: " << layerFrame.lastError() << "\n";
                break;
            }

            int32_t imageIndex = -1;
            if (!swapChain.acquire(imageIndex)) {
                std::cerr << "swap chain acquire failed: " << swapChain.lastError() << "\n";
                layerFrame.endEmpty(frameInfo.frameNumber());
                break;
            }

            renderer.render(imageIndex, viewCount, width, height);
            updateLayerViews(layer, frameInfo, swapChain);
            swapChain.release();

            if (!layerFrame.end(layer, frameInfo.frameNumber())) {
                std::cerr << "end frame failed: " << layerFrame.lastError() << "\n";
                break;
            }

            ++renderedFrames;
            if ((renderedFrames % 60) == 0) {
                std::cout << "renderedFrames=" << renderedFrames << "\n";
            }
        }

        std::cout << "Done. renderedFrames=" << renderedFrames << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
