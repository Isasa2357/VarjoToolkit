#pragma once

#include "HmdD3DTestCommon.hpp"

#include <d3dcompiler.h>

#include <cstring>

struct HmdVideoPostProcessConstants {
    float scale = 0.85f;
    float padding0 = 0.0f;
    float padding1 = 0.0f;
    float padding2 = 0.0f;
};

static_assert(sizeof(HmdVideoPostProcessConstants) % 16 == 0, "constant buffer size must be divisible by 16");

inline const char* hmdVideoPostProcessShaderSource()
{
    return R"hlsl(
#define BLOCK_SIZE 8

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
    float scale;
    float padding0;
    float padding1;
    float padding2;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const int2 pixel = int2(dispatchThreadID.xy) + destRect.xy;
    if (pixel.x < destRect.x || pixel.y < destRect.y || pixel.x >= destRect.x + destRect.z || pixel.y >= destRect.y + destRect.w) {
        return;
    }

    const float4 original = inputTex.Load(int3(pixel, 0));
    outputTex[pixel] = float4(original.rgb * scale, original.a);
}
)hlsl";
}

inline ComPtr<ID3DBlob> hmdCompileVideoPostProcessShader()
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const char* source = hmdVideoPostProcessShaderSource();
    const HRESULT hr = D3DCompile(
        source,
        std::strlen(source),
        "HmdVideoPostProcessSmokeTest.hlsl",
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
        hmdThrowIfFailed(hr, "D3DCompile cs_5_0");
    }
    return bytecode;
}
