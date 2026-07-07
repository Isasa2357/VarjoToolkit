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
#include <Varjo_d3d12.h>

#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
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
        << "VarjoD3D12RenderSample\n"
        << "\n"
        << "Usage:\n"
        << "  VarjoD3D12RenderSample.exe [options]\n"
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

D3D12_RESOURCE_BARRIER transitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
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

class D3D12TriangleRenderer {
public:
    ~D3D12TriangleRenderer()
    {
        if (queue_ && fence_) {
            waitForGpu();
        }
        if (fence_event_) {
            CloseHandle(fence_event_);
            fence_event_ = nullptr;
        }
    }

    void initialize(varjo_Session* session)
    {
#if defined(_DEBUG)
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
        }
#endif

        auto adapter = findVarjoAdapter(session);
        throwIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)), "D3D12CreateDevice");

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;
        throwIfFailed(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue_)), "CreateCommandQueue");
        throwIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)), "CreateCommandAllocator");
        throwIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&command_list_)), "CreateCommandList");
        throwIfFailed(command_list_->Close(), "Close initial command list");

        throwIfFailed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence");
        fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!fence_event_) {
            throw std::runtime_error("CreateEvent failed");
        }
    }

    ID3D12CommandQueue* commandQueue() const { return queue_.Get(); }

    void createRenderTargets(const VarjoSwapChain& swapChain)
    {
        const auto& config = swapChain.config();
        textures_.clear();
        textures_.resize(static_cast<size_t>(config.numberOfTextures));

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.NumDescriptors = static_cast<UINT>(config.numberOfTextures * config.textureArraySize);
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDesc.NodeMask = 0;
        throwIfFailed(device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtv_heap_)), "CreateDescriptorHeap");
        rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        DXGI_FORMAT psoFormat = DXGI_FORMAT_UNKNOWN;
        for (int32_t imageIndex = 0; imageIndex < config.numberOfTextures; ++imageIndex) {
            const varjo_Texture texture = swapChain.image(imageIndex);
            ID3D12Resource* resource = varjo_ToD3D12Texture(texture);
            if (!resource) {
                throw std::runtime_error("varjo_ToD3D12Texture returned null");
            }
            textures_[static_cast<size_t>(imageIndex)] = resource;
            const auto resourceDesc = resource->GetDesc();
            const DXGI_FORMAT rtvFormat = rtvFormatFor(resourceDesc.Format);
            if (psoFormat == DXGI_FORMAT_UNKNOWN) {
                psoFormat = rtvFormat;
            }

            for (int32_t arrayIndex = 0; arrayIndex < config.textureArraySize; ++arrayIndex) {
                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
                rtvDesc.Format = rtvFormat;
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = 0;
                rtvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(arrayIndex);
                rtvDesc.Texture2DArray.ArraySize = 1;
                rtvDesc.Texture2DArray.PlaneSlice = 0;
                device_->CreateRenderTargetView(resource, &rtvDesc, handleFor(imageIndex, arrayIndex, config.textureArraySize));
            }
        }

        createPipelineState(psoFormat == DXGI_FORMAT_UNKNOWN ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : psoFormat);
    }

    void render(int32_t imageIndex, int32_t viewCount, int32_t width, int32_t height)
    {
        auto* resource = textures_.at(static_cast<size_t>(imageIndex));
        if (!resource) {
            throw std::runtime_error("D3D12 swap chain resource is null");
        }

        throwIfFailed(allocator_->Reset(), "CommandAllocator Reset");
        throwIfFailed(command_list_->Reset(allocator_.Get(), pipeline_state_.Get()), "CommandList Reset");

        auto toRenderTarget = transitionBarrier(resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list_->ResourceBarrier(1, &toRenderTarget);

        command_list_->SetGraphicsRootSignature(root_signature_.Get());
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        D3D12_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        D3D12_RECT scissor{};
        scissor.left = 0;
        scissor.top = 0;
        scissor.right = width;
        scissor.bottom = height;
        command_list_->RSSetViewports(1, &viewport);
        command_list_->RSSetScissorRects(1, &scissor);

        for (int32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
            auto handle = handleFor(imageIndex, viewIndex, viewCount);
            const float clearColor[4] = {
                0.02f + 0.04f * static_cast<float>(viewIndex),
                0.04f,
                0.08f + 0.03f * static_cast<float>(viewIndex),
                1.0f};
            command_list_->OMSetRenderTargets(1, &handle, FALSE, nullptr);
            command_list_->ClearRenderTargetView(handle, clearColor, 0, nullptr);
            command_list_->DrawInstanced(3, 1, 0, 0);
        }

        auto toCommon = transitionBarrier(resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
        command_list_->ResourceBarrier(1, &toCommon);

        throwIfFailed(command_list_->Close(), "CommandList Close");
        ID3D12CommandList* lists[] = {command_list_.Get()};
        queue_->ExecuteCommandLists(1, lists);
        waitForGpu();
    }

private:
    void createPipelineState(DXGI_FORMAT rtvFormat)
    {
        auto vs = compileShader(kVertexShader, "main", "vs_5_0");
        auto ps = compileShader(kPixelShader, "main", "ps_5_0");

        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 0;
        rootDesc.pParameters = nullptr;
        rootDesc.NumStaticSamplers = 0;
        rootDesc.pStaticSamplers = nullptr;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> errors;
        const HRESULT serializeHr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
        if (FAILED(serializeHr)) {
            if (errors) {
                std::cerr << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
            }
            throwIfFailed(serializeHr, "D3D12SerializeRootSignature");
        }
        throwIfFailed(device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_)), "CreateRootSignature");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = root_signature_.Get();
        pso.VS = D3D12_SHADER_BYTECODE{vs->GetBufferPointer(), vs->GetBufferSize()};
        pso.PS = D3D12_SHADER_BYTECODE{ps->GetBufferPointer(), ps->GetBufferSize()};
        pso.BlendState.AlphaToCoverageEnable = FALSE;
        pso.BlendState.IndependentBlendEnable = FALSE;
        pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso.SampleMask = UINT_MAX;
        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.FrontCounterClockwise = FALSE;
        pso.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        pso.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        pso.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        pso.RasterizerState.DepthClipEnable = TRUE;
        pso.RasterizerState.MultisampleEnable = FALSE;
        pso.RasterizerState.AntialiasedLineEnable = FALSE;
        pso.RasterizerState.ForcedSampleCount = 0;
        pso.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        pso.DepthStencilState.DepthEnable = FALSE;
        pso.DepthStencilState.StencilEnable = FALSE;
        pso.InputLayout.pInputElementDescs = nullptr;
        pso.InputLayout.NumElements = 0;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = rtvFormat;
        pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
        pso.SampleDesc.Count = 1;
        pso.SampleDesc.Quality = 0;
        pso.NodeMask = 0;
        pso.CachedPSO.pCachedBlob = nullptr;
        pso.CachedPSO.CachedBlobSizeInBytes = 0;
        pso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        throwIfFailed(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipeline_state_)), "CreateGraphicsPipelineState");
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handleFor(int32_t imageIndex, int32_t arrayIndex, int32_t arraySize) const
    {
        const size_t index = static_cast<size_t>(imageIndex) * static_cast<size_t>(arraySize) + static_cast<size_t>(arrayIndex);
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * rtv_descriptor_size_;
        return handle;
    }

    void waitForGpu()
    {
        const uint64_t waitValue = fence_value_++;
        throwIfFailed(queue_->Signal(fence_.Get(), waitValue), "Queue Signal");
        if (fence_->GetCompletedValue() < waitValue) {
            throwIfFailed(fence_->SetEventOnCompletion(waitValue, fence_event_), "SetEventOnCompletion");
            WaitForSingleObject(fence_event_, INFINITE);
        }
    }

private:
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12CommandAllocator> allocator_;
    ComPtr<ID3D12GraphicsCommandList> command_list_;
    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12PipelineState> pipeline_state_;
    ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    UINT rtv_descriptor_size_ = 0;
    std::vector<ID3D12Resource*> textures_;
    ComPtr<ID3D12Fence> fence_;
    uint64_t fence_value_ = 1;
    HANDLE fence_event_ = nullptr;
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

        D3D12TriangleRenderer renderer;
        renderer.initialize(session.get());

        auto config = VarjoSwapChain::makeConfig(varjo_TextureFormat_R8G8B8A8_SRGB, width, height, 3, viewCount);
        VarjoSwapChain swapChain = VarjoSwapChain::createD3D12(session.shared(), renderer.commandQueue(), config);
        if (!swapChain) {
            std::cerr << "Failed to create D3D12 Varjo swap chain: " << swapChain.lastError() << "\n";
            return 1;
        }
        renderer.createRenderTargets(swapChain);

        VarjoLayerFrame layerFrame(session.shared());
        VarjoMultiProjLayer layer(viewCount, varjo_LayerFlagNone, varjo_SpaceLocal);

        std::cout << "Rendering D3D12 frames. Press Ctrl+C to stop.\n";
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
