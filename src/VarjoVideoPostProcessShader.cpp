#include <VarjoToolkit/MR/VarjoVideoPostProcessShader.hpp>
#include <VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace {

constexpr int32_t kMaxShaderTextureSlots = 16;

const char* safeErrorDesc(varjo_Error error)
{
    const char* desc = varjo_GetErrorDesc(error);
    return desc ? desc : "unknown Varjo error";
}

void logShaderConfig(const char* label, const varjo_ShaderConfig& config)
{
    VTK_SD_LOG(label
        << " format=" << static_cast<int64_t>(config.format)
        << " inputLayout=" << static_cast<int64_t>(config.inputLayout)
        << " inputFlags=" << static_cast<int64_t>(config.params.videoPostProcess.inputFlags)
        << " computeBlockSize=" << config.params.videoPostProcess.computeBlockSize
        << " samplingMargin=" << config.params.videoPostProcess.samplingMargin
        << " constantBufferSize=" << config.params.videoPostProcess.constantBufferSize);
}

} // namespace

VarjoShaderTextureLock::VarjoShaderTextureLock(std::shared_ptr<varjo_Session> session, varjo_ShaderType shaderType, int32_t textureIndex)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
    , shader_type_(shaderType)
    , texture_index_(textureIndex)
{
    VTK_SD_LOG("VarjoShaderTextureLock shared constructor session=" << session_ << " shaderType=" << static_cast<int64_t>(shader_type_) << " textureIndex=" << texture_index_);
    acquire();
}

VarjoShaderTextureLock::VarjoShaderTextureLock(varjo_Session* session, varjo_ShaderType shaderType, int32_t textureIndex)
    : session_(session)
    , shader_type_(shaderType)
    , texture_index_(textureIndex)
{
    VTK_SD_LOG("VarjoShaderTextureLock raw constructor session=" << session_ << " shaderType=" << static_cast<int64_t>(shader_type_) << " textureIndex=" << texture_index_);
    acquire();
}

VarjoShaderTextureLock::~VarjoShaderTextureLock()
{
    VTK_SD_LOG("VarjoShaderTextureLock destructor acquired=" << (acquired_ ? "true" : "false") << " textureIndex=" << texture_index_);
    release();
}

VarjoShaderTextureLock::VarjoShaderTextureLock(VarjoShaderTextureLock&& other) noexcept
    : session_owner_(std::move(other.session_owner_))
    , session_(std::exchange(other.session_, nullptr))
    , shader_type_(std::exchange(other.shader_type_, varjo_ShaderType_VideoPostProcess))
    , texture_index_(std::exchange(other.texture_index_, -1))
    , texture_(std::exchange(other.texture_, varjo_Texture{}))
    , acquired_(std::exchange(other.acquired_, false))
    , last_error_(std::move(other.last_error_))
{
    VTK_SD_LOG("VarjoShaderTextureLock move constructor acquired=" << (acquired_ ? "true" : "false") << " textureIndex=" << texture_index_);
}

VarjoShaderTextureLock& VarjoShaderTextureLock::operator=(VarjoShaderTextureLock&& other) noexcept
{
    if (this != &other) {
        VTK_SD_LOG("VarjoShaderTextureLock move assignment releasing current acquired=" << (acquired_ ? "true" : "false"));
        release();
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        shader_type_ = std::exchange(other.shader_type_, varjo_ShaderType_VideoPostProcess);
        texture_index_ = std::exchange(other.texture_index_, -1);
        texture_ = std::exchange(other.texture_, varjo_Texture{});
        acquired_ = std::exchange(other.acquired_, false);
        last_error_ = std::move(other.last_error_);
        VTK_SD_LOG("VarjoShaderTextureLock move assignment new acquired=" << (acquired_ ? "true" : "false") << " textureIndex=" << texture_index_);
    }
    return *this;
}

bool VarjoShaderTextureLock::valid() const
{
    return acquired_;
}

varjo_Texture VarjoShaderTextureLock::texture() const
{
    return texture_;
}

int32_t VarjoShaderTextureLock::textureIndex() const
{
    return texture_index_;
}

varjo_ShaderType VarjoShaderTextureLock::shaderType() const
{
    return shader_type_;
}

void VarjoShaderTextureLock::release()
{
    if (acquired_ && session_) {
        VTK_SD_LOG("varjo_MRReleaseShaderTexture shaderType=" << static_cast<int64_t>(shader_type_) << " textureIndex=" << texture_index_);
        varjo_GetError(session_);
        varjo_MRReleaseShaderTexture(session_, shader_type_, texture_index_);
        const varjo_Error error = varjo_GetError(session_);
        if (error != varjo_NoError) {
            setLastError(std::string("failed to release shader texture: ") + safeErrorDesc(error));
        }
    }
    texture_ = varjo_Texture{};
    acquired_ = false;
}

const std::string& VarjoShaderTextureLock::lastError() const
{
    return last_error_;
}

void VarjoShaderTextureLock::acquire()
{
    VTK_SD_SCOPE("VarjoShaderTextureLock::acquire");
    if (!session_) {
        setLastError("session is null");
        return;
    }
    if (texture_index_ < 0) {
        setLastError("texture index is invalid");
        return;
    }

    varjo_GetError(session_);
    texture_ = varjo_MRAcquireShaderTexture(session_, shader_type_, texture_index_);
    const varjo_Error error = varjo_GetError(session_);
    if (error != varjo_NoError) {
        setLastError(std::string("failed to acquire shader texture: ") + safeErrorDesc(error));
        acquired_ = false;
        texture_ = varjo_Texture{};
        return;
    }

    acquired_ = true;
    last_error_.clear();
    VTK_SD_LOG("shader texture acquired shaderType=" << static_cast<int64_t>(shader_type_) << " textureIndex=" << texture_index_);
}

void VarjoShaderTextureLock::setLastError(std::string message) const
{
    last_error_ = std::move(message);
    VTK_SD_ERROR(last_error_);
}

VarjoVideoPostProcessShader::VarjoVideoPostProcessShader(varjo_Session* session, bool lockNow)
    : session_(session)
{
    VTK_SD_LOG("VarjoVideoPostProcessShader raw constructor session=" << session_ << " lockNow=" << (lockNow ? "true" : "false"));
    if (lockNow) {
        lock();
    }
}

VarjoVideoPostProcessShader::VarjoVideoPostProcessShader(std::shared_ptr<varjo_Session> session, bool lockNow)
    : session_owner_(std::move(session))
    , session_(session_owner_.get())
{
    VTK_SD_LOG("VarjoVideoPostProcessShader shared constructor session=" << session_ << " lockNow=" << (lockNow ? "true" : "false"));
    if (lockNow) {
        lock();
    }
}

VarjoVideoPostProcessShader::VarjoVideoPostProcessShader(const VarjoSession& session, bool lockNow)
    : VarjoVideoPostProcessShader(session.shared(), lockNow)
{}

VarjoVideoPostProcessShader::~VarjoVideoPostProcessShader()
{
    VTK_SD_LOG("VarjoVideoPostProcessShader destructor locked=" << (locked_ ? "true" : "false"));
    unlock();
}

VarjoVideoPostProcessShader::VarjoVideoPostProcessShader(VarjoVideoPostProcessShader&& other) noexcept
    : session_owner_(std::move(other.session_owner_))
    , session_(std::exchange(other.session_, nullptr))
    , locked_(std::exchange(other.locked_, false))
    , last_error_(std::move(other.last_error_))
{
    VTK_SD_LOG("VarjoVideoPostProcessShader move constructor session=" << session_ << " locked=" << (locked_ ? "true" : "false"));
}

VarjoVideoPostProcessShader& VarjoVideoPostProcessShader::operator=(VarjoVideoPostProcessShader&& other) noexcept
{
    if (this != &other) {
        VTK_SD_LOG("VarjoVideoPostProcessShader move assignment releasing current locked=" << (locked_ ? "true" : "false"));
        unlock();
        session_owner_ = std::move(other.session_owner_);
        session_ = std::exchange(other.session_, nullptr);
        locked_ = std::exchange(other.locked_, false);
        last_error_ = std::move(other.last_error_);
        VTK_SD_LOG("VarjoVideoPostProcessShader move assignment new session=" << session_ << " locked=" << (locked_ ? "true" : "false"));
    }
    return *this;
}

bool VarjoVideoPostProcessShader::valid() const
{
    return session_ != nullptr && locked_;
}

varjo_Session* VarjoVideoPostProcessShader::session() const
{
    return session_;
}

bool VarjoVideoPostProcessShader::locked() const
{
    return locked_;
}

bool VarjoVideoPostProcessShader::lock()
{
    VTK_SD_SCOPE("VarjoVideoPostProcessShader::lock");
    if (locked_) {
        VTK_SD_LOG("video post process shader already locked");
        return true;
    }
    if (!session_) {
        setLastError("session is null");
        return false;
    }

    locked_ = (varjo_Lock(session_, varjo_LockType_VideoPostProcessShader) == varjo_True);
    if (!locked_) {
        setLastError("failed to acquire video post process shader lock");
        return false;
    }

    last_error_.clear();
    VTK_SD_LOG("video post process shader lock acquired");
    return true;
}

void VarjoVideoPostProcessShader::unlock()
{
    if (locked_ && session_) {
        VTK_SD_LOG("varjo_Unlock VideoPostProcessShader");
        varjo_Unlock(session_, varjo_LockType_VideoPostProcessShader);
    }
    locked_ = false;
}

bool VarjoVideoPostProcessShader::setEnabled(bool enabled)
{
    VTK_SD_LOG("set video post process shader enabled=" << (enabled ? "true" : "false"));
    if (!valid()) {
        setLastError("video post process shader is not locked");
        return false;
    }

    clearNativeError();
    varjo_MRSetShader(session_, varjo_ShaderType_VideoPostProcess, enabled ? varjo_True : varjo_False);
    return checkNativeError("set video post process shader enabled state");
}

bool VarjoVideoPostProcessShader::configureD3D11(ID3D11Device* device, const varjo_ShaderConfig& config, VarjoShaderBytecodeView bytecode)
{
    return configureD3D11(device, config, bytecode.data, bytecode.size);
}

bool VarjoVideoPostProcessShader::configureD3D11(ID3D11Device* device, const varjo_ShaderConfig& config, const void* shaderData, int32_t shaderSize)
{
    VTK_SD_SCOPE("VarjoVideoPostProcessShader::configureD3D11");
    logShaderConfig("configureD3D11 config", config);
    VTK_SD_LOG("D3D11 device=" << device << " shaderData=" << shaderData << " shaderSize=" << shaderSize);
    if (!valid()) {
        setLastError("video post process shader is not locked");
        return false;
    }
    if (!device) {
        setLastError("D3D11 device is null");
        return false;
    }
    if (!shaderData || shaderSize <= 0) {
        setLastError("shader bytecode is empty");
        return false;
    }

    clearNativeError();
    varjo_MRD3D11ConfigureShader(
        session_,
        device,
        varjo_ShaderType_VideoPostProcess,
        &config,
        reinterpret_cast<const char*>(shaderData),
        shaderSize);
    return checkNativeError("configure D3D11 video post process shader");
}

bool VarjoVideoPostProcessShader::configureD3D12(ID3D12CommandQueue* commandQueue, const varjo_ShaderConfig& config, VarjoShaderBytecodeView bytecode)
{
    return configureD3D12(commandQueue, config, bytecode.data, bytecode.size);
}

bool VarjoVideoPostProcessShader::configureD3D12(ID3D12CommandQueue* commandQueue, const varjo_ShaderConfig& config, const void* shaderData, int32_t shaderSize)
{
    VTK_SD_SCOPE("VarjoVideoPostProcessShader::configureD3D12");
    logShaderConfig("configureD3D12 config", config);
    VTK_SD_LOG("D3D12 commandQueue=" << commandQueue << " shaderData=" << shaderData << " shaderSize=" << shaderSize);
    if (!valid()) {
        setLastError("video post process shader is not locked");
        return false;
    }
    if (!commandQueue) {
        setLastError("D3D12 command queue is null");
        return false;
    }
    if (!shaderData || shaderSize <= 0) {
        setLastError("shader bytecode is empty");
        return false;
    }

    clearNativeError();
    varjo_MRD3D12ConfigureShader(
        session_,
        commandQueue,
        varjo_ShaderType_VideoPostProcess,
        &config,
        reinterpret_cast<const char*>(shaderData),
        shaderSize);
    return checkNativeError("configure D3D12 video post process shader");
}

VarjoShaderTextureLock VarjoVideoPostProcessShader::acquireTexture(int32_t textureIndex)
{
    VTK_SD_LOG("acquire video post process shader texture index=" << textureIndex);
    if (!valid()) {
        VarjoShaderTextureLock lock;
        setLastError("video post process shader is not locked");
        return lock;
    }
    if (session_owner_) {
        return VarjoShaderTextureLock(session_owner_, varjo_ShaderType_VideoPostProcess, textureIndex);
    }
    return VarjoShaderTextureLock(session_, varjo_ShaderType_VideoPostProcess, textureIndex);
}

bool VarjoVideoPostProcessShader::submitInputs(
    const int32_t* textureIndices,
    int32_t textureIndexCount,
    VarjoShaderConstantBufferView constantBuffer)
{
    VTK_SD_LOG("submit shader inputs textureIndexCount=" << textureIndexCount << " constantBufferSize=" << constantBuffer.size << " constantBufferData=" << constantBuffer.data);
    if (!valid()) {
        setLastError("video post process shader is not locked");
        return false;
    }
    if (textureIndexCount < 0) {
        setLastError("texture index count is invalid");
        return false;
    }
    if (textureIndexCount > 0 && !textureIndices) {
        setLastError("texture index list is null");
        return false;
    }
    if (constantBuffer.size < 0) {
        setLastError("constant buffer size is invalid");
        return false;
    }
    if (constantBuffer.size > 0 && !constantBuffer.data) {
        setLastError("constant buffer data is null");
        return false;
    }

    clearNativeError();
    varjo_MRSubmitShaderInputs(
        session_,
        varjo_ShaderType_VideoPostProcess,
        textureIndexCount > 0 ? textureIndices : nullptr,
        textureIndexCount,
        constantBuffer.size > 0 ? reinterpret_cast<const char*>(constantBuffer.data) : nullptr,
        constantBuffer.size > 0 ? constantBuffer.size : 0);
    return checkNativeError("submit video post process shader inputs");
}

bool VarjoVideoPostProcessShader::submitInputs(
    const std::vector<int32_t>& textureIndices,
    VarjoShaderConstantBufferView constantBuffer)
{
    return submitInputs(
        textureIndices.empty() ? nullptr : textureIndices.data(),
        static_cast<int32_t>(textureIndices.size()),
        constantBuffer);
}

bool VarjoVideoPostProcessShader::submitConstantBuffer(VarjoShaderConstantBufferView constantBuffer)
{
    return submitInputs(nullptr, 0, constantBuffer);
}

std::vector<varjo_TextureFormat> VarjoVideoPostProcessShader::supportedTextureFormats(varjo_RenderAPI renderAPI) const
{
    VTK_SD_LOG("query supported shader texture formats renderAPI=" << static_cast<int64_t>(renderAPI));
    std::vector<varjo_TextureFormat> formats;
    if (!session_) {
        VTK_SD_WARN("supportedTextureFormats called with null session");
        return formats;
    }

    varjo_GetError(session_);
    const int32_t count = varjo_MRGetSupportedShaderTextureFormatCount(session_, renderAPI, varjo_ShaderType_VideoPostProcess);
    const varjo_Error countError = varjo_GetError(session_);
    if (countError != varjo_NoError || count <= 0) {
        VTK_SD_WARN("supportedTextureFormats count failed or empty count=" << count << " error=" << safeErrorDesc(countError));
        return formats;
    }

    formats.resize(static_cast<size_t>(count));
    varjo_GetError(session_);
    varjo_MRGetSupportedShaderTextureFormats(session_, renderAPI, varjo_ShaderType_VideoPostProcess, formats.data(), count);
    const varjo_Error formatsError = varjo_GetError(session_);
    if (formatsError != varjo_NoError) {
        VTK_SD_ERROR("supportedTextureFormats fetch failed: " << safeErrorDesc(formatsError));
        formats.clear();
    }
    VTK_SD_LOG("supported shader texture format count=" << formats.size());
    return formats;
}

varjo_DXGITextureFormat VarjoVideoPostProcessShader::toDXGIFormat(varjo_TextureFormat format) const
{
    if (!session_) {
        VTK_SD_WARN("toDXGIFormat called with null session");
        return 0;
    }
    varjo_GetError(session_);
    const auto dxgi = varjo_ToDXGIFormat(session_, format);
    const varjo_Error error = varjo_GetError(session_);
    if (error != varjo_NoError) {
        VTK_SD_ERROR("toDXGIFormat failed format=" << static_cast<int64_t>(format) << " error=" << safeErrorDesc(error));
        return 0;
    }
    VTK_SD_LOG("toDXGIFormat format=" << static_cast<int64_t>(format) << " dxgi=" << dxgi);
    return dxgi;
}

const std::string& VarjoVideoPostProcessShader::lastError() const
{
    return last_error_;
}

varjo_ShaderConfig VarjoVideoPostProcessShader::makeVideoPostProcessConfig(
    int64_t constantBufferSize,
    int64_t computeBlockSize,
    int64_t samplingMargin,
    varjo_ShaderFlags_VideoPostProcess inputFlags)
{
    varjo_ShaderConfig config{};
    config.format = varjo_ShaderFormat_DxComputeBlob;
    config.inputLayout = varjo_ShaderInputLayout_VideoPostProcess_V2;
    config.params.videoPostProcess.inputFlags = inputFlags;
    config.params.videoPostProcess.computeBlockSize = computeBlockSize;
    config.params.videoPostProcess.samplingMargin = samplingMargin;
    config.params.videoPostProcess.constantBufferSize = constantBufferSize;
    logShaderConfig("makeVideoPostProcessConfig", config);
    return config;
}

bool VarjoVideoPostProcessShader::setTextureConfig(
    varjo_ShaderConfig& config,
    int32_t textureIndex,
    varjo_TextureFormat format,
    uint64_t width,
    uint64_t height)
{
    if (textureIndex < 0 || textureIndex >= kMaxShaderTextureSlots) {
        VTK_SD_ERROR("setTextureConfig invalid textureIndex=" << textureIndex);
        return false;
    }
    config.params.videoPostProcess.textures[textureIndex].format = format;
    config.params.videoPostProcess.textures[textureIndex].width = width;
    config.params.videoPostProcess.textures[textureIndex].height = height;
    VTK_SD_LOG("setTextureConfig textureIndex=" << textureIndex << " format=" << static_cast<int64_t>(format) << " size=" << width << 'x' << height);
    return true;
}

VarjoShaderBytecodeView VarjoVideoPostProcessShader::bytecodeView(const void* data, size_t size)
{
    if (!data || size == 0 || size > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        VTK_SD_WARN("bytecodeView invalid data=" << data << " size=" << size);
        return {};
    }
    VTK_SD_LOG("bytecodeView size=" << size);
    return VarjoShaderBytecodeView{data, static_cast<int32_t>(size)};
}

bool VarjoVideoPostProcessShader::checkNativeError(const char* operation) const
{
    const varjo_Error error = varjo_GetError(session_);
    if (error == varjo_NoError) {
        last_error_.clear();
        VTK_SD_LOG((operation ? operation : "operation") << " succeeded");
        return true;
    }

    setLastError(std::string(operation) + " failed: " + safeErrorDesc(error));
    return false;
}

void VarjoVideoPostProcessShader::clearNativeError() const
{
    if (session_) {
        varjo_GetError(session_);
    }
}

void VarjoVideoPostProcessShader::setLastError(std::string message) const
{
    last_error_ = std::move(message);
    VTK_SD_ERROR(last_error_);
}
