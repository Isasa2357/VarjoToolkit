#pragma once

#include "HmdTestCommon.hpp"

#include <Varjo_d3d11.h>

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <cstdint>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

inline void hmdThrowIfFailed(HRESULT hr, const char* what)
{
    if (FAILED(hr)) {
        throw std::runtime_error(std::string(what) + " failed with HRESULT=0x" + std::to_string(static_cast<unsigned long>(hr)));
    }
}

inline bool hmdSameLuid(const DXGI_ADAPTER_DESC1& desc, const varjo_Luid& luid)
{
    return desc.AdapterLuid.HighPart == luid.high && static_cast<uint32_t>(desc.AdapterLuid.LowPart) == luid.low;
}

inline ComPtr<IDXGIAdapter1> hmdFindVarjoAdapter(varjo_Session* session)
{
    const varjo_Luid luid = varjo_D3D11GetLuid(session);

    ComPtr<IDXGIFactory1> factory;
    hmdThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");

    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc)) && hmdSameLuid(desc, luid)) {
            std::wcout << L"Using adapter: " << desc.Description << L"\n";
            return adapter;
        }
    }

    throw std::runtime_error("Could not find DXGI adapter matching Varjo runtime LUID");
}

inline ComPtr<ID3D11Device> hmdCreateD3D11Device(varjo_Session* session)
{
    auto adapter = hmdFindVarjoAdapter(session);

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

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL createdLevel{};
    hmdThrowIfFailed(
        D3D11CreateDevice(
            adapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            flags,
            featureLevels,
            featureLevelCount,
            D3D11_SDK_VERSION,
            &device,
            &createdLevel,
            &context),
        "D3D11CreateDevice");
    return device;
}

struct HmdD3D12Context {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
};

inline HmdD3D12Context hmdCreateD3D12Context(varjo_Session* session)
{
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer();
    }
#endif

    auto adapter = hmdFindVarjoAdapter(session);
    HmdD3D12Context context;
    hmdThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&context.device)), "D3D12CreateDevice");

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    hmdThrowIfFailed(context.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&context.queue)), "CreateCommandQueue");
    return context;
}
