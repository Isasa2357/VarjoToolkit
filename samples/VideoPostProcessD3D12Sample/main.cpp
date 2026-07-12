#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/MR/VarjoVideoPostProcessShader.hpp>

#include <Varjo_d3d11.h>
#include <Varjo_mr.h>

#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

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
    int seconds = 30;
    float radius = 0.28f;
    float dimStrength = 0.55f;
    bool animate = true;
    bool help = false;
};

struct PostProcessConstants {
    float dimStrength = 0.55f;
    float radius = 0.28f;
    float pulse = 1.0f;
    float feather = 0.06f;
    float centerX = 0.5f;
    float centerY = 0.5f;
    float aspectScale = 1.0f;
    float padding = 0.0f;
};

static_assert(sizeof(PostProcessConstants) % 16 == 0, "Varjo shader constant buffer size must be divisible by 16");

void printUsage()
{
    std::cout
        << "VarjoVideoPostProcessD3D12Sample\n"
        << "\n"
        << "Usage:\n"
        << "  VarjoVideoPostProcessD3D12Sample.exe [options]\n"
        << "\n"
        << "Options:\n"
        << "  --seconds <n>       Duration in seconds. 0 means until Ctrl+C. Default: 30\n"
        << "  --radius <v>        Highlight radius in normalized short-axis units. Default: 0.28\n"
        << "  --dim <v>           Outside-circle dim strength [0..1]. Default: 0.55\n"
        << "  --no-animate        Disable pulse animation\n"
        << "  --help              Show this message\n";
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

bool parseFloat(const char* text, float& value)
{
    try {
        size_t consumed = 0;
        const float parsed = std::stof(text, &consumed);
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
        if (arg == "--seconds") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --seconds\n";
                return false;
            }
            int seconds = 0;
            if (!parseInt(argv[++i], seconds) || seconds < 0) {
                std::cerr << "Invalid --seconds value\n";
                return false;
            }
            options.seconds = seconds;
            continue;
        }
        if (arg == "--radius") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --radius\n";
                return false;
            }
            float radius = 0.0f;
            if (!parseFloat(argv[++i], radius) || radius <= 0.0f) {
                std::cerr << "Invalid --radius value\n";
                return false;
            }
            options.radius = radius;
            continue;
        }
        if (arg == "--dim") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --dim\n";
                return false;
            }
            float dim = 0.0f;
            if (!parseFloat(argv[++i], dim) || dim < 0.0f || dim > 1.0f) {
                std::cerr << "Invalid --dim value\n";
                return false;
            }
            options.dimStrength = dim;
            continue;
        }
        if (arg == "--no-animate") {
            options.animate = false;
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

ComPtr<ID3D12CommandQueue> createD3D12CommandQueueForVarjo(varjo_Session* session)
{
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer();
    }
#endif

    auto adapter = findVarjoAdapter(session);
    ComPtr<ID3D12Device> device;
    throwIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "D3D12CreateDevice");

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    ComPtr<ID3D12CommandQueue> queue;
    throwIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
    return queue;
}

ComPtr<ID3DBlob> compileShader(const char* source)
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
        "VarjoVideoPostProcessD3D12Sample.hlsl",
        nullptr,
        nullptr,
        "main",
        "cs_5_0",
        flags,
        0,
        &bytecode,
        &errors);

    if (FAILED(hr)) {
        if (errors) {
            std::cerr << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
        }
        throwIfFailed(hr, "D3DCompile cs_5_0");
    }
    return bytecode;
}

const char* kVideoPostProcessShader = R"hlsl(
#define BLOCK_SIZE 8
#define VIEW_FOCUS_L 2
#define VIEW_FOCUS_R 3
#define FLIP_FOCUS_Y 1

Texture2D<float4> inputTex : register(t0);
RWTexture2D<float4> outputTex : register(u0);
SamplerState SamplerLinearClamp : register(s0);
SamplerState SamplerLinearWrap : register(s1);

cbuffer VarjoVideoPostProcessConstants : register(b0)
{
    int2 sourceSize;
    float sourceTime;
    int viewIndex;
    int4 destRect;
    float4x4 projection;
    float4x4 inverseProjection;
    float4x4 view;
    float4x4 inverseView;
    int4 sourceFocusRect;
    int2 sourceContextSize;
    int2 varjoPadding;
};

cbuffer UserConstants : register(b1)
{
    float dimStrength;
    float radius;
    float pulse;
    float feather;
    float centerX;
    float centerY;
    float aspectScale;
    float userPadding;
};

bool isFocusView()
{
    return viewIndex == VIEW_FOCUS_L || viewIndex == VIEW_FOCUS_R;
}

// sourceFocusRect.xy is the top-left and sourceFocusRect.zw is the
// bottom-right of the focus view in absolute context-view pixels.
float2 sourcePixelToContextPixel(float2 sourcePixelCenter)
{
    if (!isFocusView()) {
        return sourcePixelCenter;
    }

    const float2 sourceExtent = max(float2(sourceSize), float2(1.0f, 1.0f));
    float2 focusUv = saturate(sourcePixelCenter / sourceExtent);
#if FLIP_FOCUS_Y
    focusUv.y = 1.0f - focusUv.y;
#endif
    const float2 focusTopLeft = float2(sourceFocusRect.xy);
    const float2 focusBottomRight = float2(sourceFocusRect.zw);
    return lerp(focusTopLeft, focusBottomRight, focusUv);
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const int2 pixel = int2(dispatchThreadID.xy) + destRect.xy;
    if (pixel.x < destRect.x || pixel.y < destRect.y || pixel.x >= destRect.x + destRect.z || pixel.y >= destRect.y + destRect.w) {
        return;
    }

    const float4 original = inputTex.Load(int3(pixel, 0));
    float4 color = original;

    const float2 contextSize = max(float2(sourceContextSize), float2(1.0f, 1.0f));
    const float shortAxis = max(min(contextSize.x, contextSize.y), 1.0f);
    const float2 contextPixel = sourcePixelToContextPixel(float2(pixel) + 0.5f);
    const float2 contextCenter = contextSize * 0.5f + (float2(centerX, centerY) - 0.5f) * shortAxis;

    float2 delta = (contextPixel - contextCenter) / shortAxis;
    delta.x *= aspectScale;

    const float d = length(delta);
    const float outside = smoothstep(radius, radius + max(feather, 0.001f), d);
    const float animatedDim = saturate(dimStrength * lerp(0.85f, 1.15f, saturate(pulse)));
    color.rgb *= lerp(1.0f, 1.0f - animatedDim, outside);

    outputTex[pixel] = float4(color.rgb, original.a);
}
)hlsl";

PostProcessConstants makeConstants(const Options& options, double elapsedSeconds)
{
    PostProcessConstants constants{};
    constants.dimStrength = options.dimStrength;
    constants.radius = options.radius;
    constants.feather = 0.06f;
    constants.centerX = 0.5f;
    constants.centerY = 0.5f;
    constants.aspectScale = 1.0f;
    constants.pulse = options.animate ? (0.5f + 0.5f * std::sin(static_cast<float>(elapsedSeconds) * 3.14159265f * 2.0f * 0.8f)) : 1.0f;
    return constants;
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

        std::cout << "Creating D3D12 command queue for Varjo adapter...\n";
        ComPtr<ID3D12CommandQueue> commandQueue = createD3D12CommandQueueForVarjo(session.get());

        std::cout << "Compiling video post process compute shader...\n";
        ComPtr<ID3DBlob> shaderBlob = compileShader(kVideoPostProcessShader);

        VarjoVideoPostProcessShader postProcess(session.shared());
        if (!postProcess) {
            std::cerr << "Failed to lock Varjo video post process shader: " << postProcess.lastError() << "\n";
            return 1;
        }

        auto config = VarjoVideoPostProcessShader::makeVideoPostProcessConfig(
            static_cast<int64_t>(sizeof(PostProcessConstants)),
            8,
            0,
            varjo_ShaderFlag_VideoPostProcess_None);

        if (!postProcess.configureD3D12(
                commandQueue.Get(),
                config,
                shaderBlob->GetBufferPointer(),
                static_cast<int32_t>(shaderBlob->GetBufferSize()))) {
            std::cerr << "configureD3D12 failed: " << postProcess.lastError() << "\n";
            return 1;
        }

        const auto start = std::chrono::steady_clock::now();
        PostProcessConstants constants = makeConstants(options, 0.0);
        if (!postProcess.submitConstantBuffer(constants)) {
            std::cerr << "submitConstantBuffer failed: " << postProcess.lastError() << "\n";
            return 1;
        }

        varjo_MRSetVideoRender(session.get(), varjo_True);
        if (!postProcess.setEnabled(true)) {
            std::cerr << "setEnabled(true) failed: " << postProcess.lastError() << "\n";
            return 1;
        }

        std::cout << "D3D12 video post process enabled. Press Ctrl+C to stop.\n";
        int lastPrintedSecond = -1;
        while (!g_stopRequested.load()) {
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - start).count();
            if (options.seconds > 0 && elapsed >= static_cast<double>(options.seconds)) {
                break;
            }

            constants = makeConstants(options, elapsed);
            if (!postProcess.submitConstantBuffer(constants)) {
                std::cerr << "submitConstantBuffer failed: " << postProcess.lastError() << "\n";
                break;
            }

            const int elapsedInt = static_cast<int>(elapsed);
            if (elapsedInt != lastPrintedSecond) {
                lastPrintedSecond = elapsedInt;
                std::cout << "elapsed=" << elapsedInt
                          << "s dim=" << constants.dimStrength
                          << " radius=" << constants.radius
                          << " pulse=" << constants.pulse << "\n";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

        std::cout << "Disabling video post process shader...\n";
        postProcess.setEnabled(false);
        std::cout << "Done.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
