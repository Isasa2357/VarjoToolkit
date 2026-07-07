#pragma once

#include <Varjo.h>
#include <Varjo_mr.h>
#include <Varjo_mr_experimental.h>
#include <Varjo_types_mr_experimental.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <VarjoToolkit/Core/VarjoSession.hpp>

struct ID3D11Device;
struct ID3D12CommandQueue;

struct VarjoShaderBytecodeView {
    const void* data = nullptr;
    int32_t size = 0;

    bool valid() const { return data != nullptr && size > 0; }
};

struct VarjoShaderConstantBufferView {
    const void* data = nullptr;
    int32_t size = 0;

    bool empty() const { return data == nullptr || size <= 0; }
};

class VarjoVideoPostProcessShader;

class VarjoShaderTextureLock {
public:
    VarjoShaderTextureLock() = default;
    VarjoShaderTextureLock(std::shared_ptr<varjo_Session> session, varjo_ShaderType shaderType, int32_t textureIndex);
    VarjoShaderTextureLock(varjo_Session* session, varjo_ShaderType shaderType, int32_t textureIndex);
    ~VarjoShaderTextureLock();

    VarjoShaderTextureLock(const VarjoShaderTextureLock&) = delete;
    VarjoShaderTextureLock& operator=(const VarjoShaderTextureLock&) = delete;
    VarjoShaderTextureLock(VarjoShaderTextureLock&& other) noexcept;
    VarjoShaderTextureLock& operator=(VarjoShaderTextureLock&& other) noexcept;

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Texture texture() const;
    int32_t textureIndex() const;
    varjo_ShaderType shaderType() const;
    void release();

    const std::string& lastError() const;

private:
    void acquire();
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    varjo_ShaderType shader_type_ = varjo_ShaderType_VideoPostProcess;
    int32_t texture_index_ = -1;
    varjo_Texture texture_{};
    bool acquired_ = false;
    mutable std::string last_error_;
};

class VarjoVideoPostProcessShader {
public:
    explicit VarjoVideoPostProcessShader(varjo_Session* session, bool lockNow = true);
    explicit VarjoVideoPostProcessShader(std::shared_ptr<varjo_Session> session, bool lockNow = true);
    explicit VarjoVideoPostProcessShader(const VarjoSession& session, bool lockNow = true);
    ~VarjoVideoPostProcessShader();

    VarjoVideoPostProcessShader(const VarjoVideoPostProcessShader&) = delete;
    VarjoVideoPostProcessShader& operator=(const VarjoVideoPostProcessShader&) = delete;
    VarjoVideoPostProcessShader(VarjoVideoPostProcessShader&& other) noexcept;
    VarjoVideoPostProcessShader& operator=(VarjoVideoPostProcessShader&& other) noexcept;

    bool valid() const;
    explicit operator bool() const { return valid(); }

    varjo_Session* session() const;
    bool locked() const;
    bool lock();
    void unlock();

    bool setEnabled(bool enabled);

    bool configureD3D11(ID3D11Device* device, const varjo_ShaderConfig& config, VarjoShaderBytecodeView bytecode);
    bool configureD3D11(ID3D11Device* device, const varjo_ShaderConfig& config, const void* shaderData, int32_t shaderSize);

    bool configureD3D12(ID3D12CommandQueue* commandQueue, const varjo_ShaderConfig& config, VarjoShaderBytecodeView bytecode);
    bool configureD3D12(ID3D12CommandQueue* commandQueue, const varjo_ShaderConfig& config, const void* shaderData, int32_t shaderSize);

    VarjoShaderTextureLock acquireTexture(int32_t textureIndex);

    bool submitInputs(
        const int32_t* textureIndices,
        int32_t textureIndexCount,
        VarjoShaderConstantBufferView constantBuffer = {});
    bool submitInputs(
        const std::vector<int32_t>& textureIndices,
        VarjoShaderConstantBufferView constantBuffer = {});
    bool submitConstantBuffer(VarjoShaderConstantBufferView constantBuffer);

    template <typename T>
    bool submitInputs(const std::vector<int32_t>& textureIndices, const T& constants)
    {
        static_assert(std::is_trivially_copyable<T>::value, "constant buffer structs must be trivially copyable");
        return submitInputs(textureIndices, VarjoShaderConstantBufferView{&constants, static_cast<int32_t>(sizeof(T))});
    }

    template <typename T>
    bool submitConstantBuffer(const T& constants)
    {
        static_assert(std::is_trivially_copyable<T>::value, "constant buffer structs must be trivially copyable");
        return submitConstantBuffer(VarjoShaderConstantBufferView{&constants, static_cast<int32_t>(sizeof(T))});
    }

    std::vector<varjo_TextureFormat> supportedTextureFormats(varjo_RenderAPI renderAPI) const;
    varjo_DXGITextureFormat toDXGIFormat(varjo_TextureFormat format) const;

    const std::string& lastError() const;

    static varjo_ShaderConfig makeVideoPostProcessConfig(
        int64_t constantBufferSize = 0,
        int64_t computeBlockSize = 16,
        int64_t samplingMargin = 0,
        varjo_ShaderFlags_VideoPostProcess inputFlags = varjo_ShaderFlag_VideoPostProcess_None);
    static bool setTextureConfig(varjo_ShaderConfig& config, int32_t textureIndex, varjo_TextureFormat format, uint64_t width, uint64_t height);
    static VarjoShaderBytecodeView bytecodeView(const void* data, size_t size);

private:
    bool checkNativeError(const char* operation) const;
    void clearNativeError() const;
    void setLastError(std::string message) const;

private:
    std::shared_ptr<varjo_Session> session_owner_;
    varjo_Session* session_ = nullptr;
    bool locked_ = false;
    mutable std::string last_error_;
};
