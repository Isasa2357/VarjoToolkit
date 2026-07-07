#include <VarjoToolkit/MR/VarjoVideoPostProcessShader.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct TestConstants {
    float radius = 0.25f;
    float gain = 1.5f;
    int mode = 2;
    int padding = 0;
};

int fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

bool expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit video post process shader wrapper test\n";

    const std::vector<std::uint8_t> bytecodeBytes{1, 2, 3, 4};
    const auto bytecode = VarjoVideoPostProcessShader::bytecodeView(bytecodeBytes.data(), bytecodeBytes.size());
    if (!expect(bytecode.valid(), "bytecode view should be valid for non-empty data")) return 1;
    if (!expect(bytecode.data == bytecodeBytes.data(), "bytecode view should keep data pointer")) return 1;
    if (!expect(bytecode.size == 4, "bytecode view should store byte size")) return 1;
    if (!expect(!VarjoVideoPostProcessShader::bytecodeView(nullptr, 4).valid(), "null bytecode data should be invalid")) return 1;
    if (!expect(!VarjoVideoPostProcessShader::bytecodeView(bytecodeBytes.data(), 0).valid(), "zero bytecode size should be invalid")) return 1;

    auto config = VarjoVideoPostProcessShader::makeVideoPostProcessConfig(sizeof(TestConstants), 16, 2);
    if (!expect(config.format == varjo_ShaderFormat_DxComputeBlob, "default shader format should be DxComputeBlob")) return 1;
    if (!expect(config.inputLayout == varjo_ShaderInputLayout_VideoPostProcess_V2, "default layout should be video post process V2")) return 1;
    if (!expect(config.params.videoPostProcess.constantBufferSize == sizeof(TestConstants), "constant buffer size should be stored")) return 1;
    if (!expect(config.params.videoPostProcess.computeBlockSize == 16, "compute block size should be stored")) return 1;
    if (!expect(config.params.videoPostProcess.samplingMargin == 2, "sampling margin should be stored")) return 1;

    if (!expect(VarjoVideoPostProcessShader::setTextureConfig(config, 0, varjo_TextureFormat_R8G8B8A8_UNORM, 640, 480), "texture config slot 0 should be accepted")) return 1;
    if (!expect(config.params.videoPostProcess.textures[0].format == varjo_TextureFormat_R8G8B8A8_UNORM, "texture format should be stored")) return 1;
    if (!expect(config.params.videoPostProcess.textures[0].width == 640, "texture width should be stored")) return 1;
    if (!expect(config.params.videoPostProcess.textures[0].height == 480, "texture height should be stored")) return 1;
    if (!expect(!VarjoVideoPostProcessShader::setTextureConfig(config, -1, varjo_TextureFormat_R8G8B8A8_UNORM, 1, 1), "negative texture slot should be rejected")) return 1;
    if (!expect(!VarjoVideoPostProcessShader::setTextureConfig(config, 16, varjo_TextureFormat_R8G8B8A8_UNORM, 1, 1), "slot 16 should be rejected")) return 1;

    VarjoShaderConstantBufferView emptyConstants{};
    if (!expect(emptyConstants.empty(), "default constant buffer view should be empty")) return 1;
    TestConstants constants{};
    VarjoShaderConstantBufferView constantView{&constants, static_cast<int32_t>(sizeof(constants))};
    if (!expect(!constantView.empty(), "non-empty constant buffer view should not be empty")) return 1;

    VarjoShaderTextureLock nullTextureLock(static_cast<varjo_Session*>(nullptr), varjo_ShaderType_VideoPostProcess, 0);
    if (nullTextureLock.valid()) return fail("null texture lock should be invalid");
    if (nullTextureLock.lastError().empty()) return fail("null texture lock should report error");

    VarjoVideoPostProcessShader shader(static_cast<varjo_Session*>(nullptr), false);
    if (shader.valid()) return fail("shader with null session should be invalid");
    if (shader.lock()) return fail("lock should fail for null session");
    if (shader.lastError().empty()) return fail("null shader lock should set lastError");
    if (shader.setEnabled(true)) return fail("setEnabled should fail while not locked");
    if (shader.configureD3D11(nullptr, config, bytecode)) return fail("configureD3D11 should fail while not locked");
    if (shader.configureD3D12(nullptr, config, bytecode)) return fail("configureD3D12 should fail while not locked");
    if (shader.submitInputs(std::vector<int32_t>{}, constantView)) return fail("submitInputs should fail while not locked");
    if (shader.submitConstantBuffer(constants)) return fail("templated submitConstantBuffer should fail while not locked but compile");
    if (!shader.supportedTextureFormats(varjo_RenderAPI_D3D11).empty()) return fail("null shader should not report texture formats");
    if (shader.toDXGIFormat(varjo_TextureFormat_R8G8B8A8_UNORM) != 0) return fail("null shader should not report DXGI format");

    std::cout << "[PASS] VarjoToolkit video post process shader wrapper test passed\n";
    return 0;
}
