#include "HmdVideoPostProcessCommon.hpp"

#include <VarjoToolkit/MR/VarjoVideoPostProcessShader.hpp>

#include <Varjo_mr.h>

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    std::cout << "VarjoToolkit HMD D3D11 video post process smoke test\n";
    if (!requireRuntimeAvailable()) {
        return 1;
    }

    try {
        VarjoSession session;
        if (!requireSession(session)) {
            return 1;
        }

        ComPtr<ID3D11Device> device = hmdCreateD3D11Device(session.get());
        ComPtr<ID3DBlob> shaderBlob = hmdCompileVideoPostProcessShader();

        VarjoVideoPostProcessShader shader(session.shared());
        if (!shader.valid()) {
            return hmdFail(std::string("failed to lock video post process shader: ") + shader.lastError());
        }

        auto config = VarjoVideoPostProcessShader::makeVideoPostProcessConfig(
            static_cast<int64_t>(sizeof(HmdVideoPostProcessConstants)),
            8,
            0,
            varjo_ShaderFlag_VideoPostProcess_None);

        if (!shader.configureD3D11(
                device.Get(),
                config,
                shaderBlob->GetBufferPointer(),
                static_cast<int32_t>(shaderBlob->GetBufferSize()))) {
            return hmdFail(std::string("configureD3D11 failed: ") + shader.lastError());
        }

        HmdVideoPostProcessConstants constants{};
        constants.scale = 0.85f;
        if (!shader.submitConstantBuffer(constants)) {
            return hmdFail(std::string("submitConstantBuffer failed: ") + shader.lastError());
        }

        varjo_MRSetVideoRender(session.get(), varjo_True);
        if (!shader.setEnabled(true)) {
            return hmdFail(std::string("setEnabled(true) failed: ") + shader.lastError());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        constants.scale = 0.95f;
        if (!shader.submitConstantBuffer(constants)) {
            shader.setEnabled(false);
            return hmdFail(std::string("second submitConstantBuffer failed: ") + shader.lastError());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!shader.setEnabled(false)) {
            return hmdFail(std::string("setEnabled(false) failed: ") + shader.lastError());
        }

        std::cout << "[PASS] HMD D3D11 video post process smoke test passed\n";
        return 0;
    } catch (const std::exception& e) {
        return hmdFail(std::string("fatal exception: ") + e.what());
    }
}
