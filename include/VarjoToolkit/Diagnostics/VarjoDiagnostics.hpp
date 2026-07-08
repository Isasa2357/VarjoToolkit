#pragma once

#ifndef VARJOTOOLKIT_SUPERDEBUG
#define VARJOTOOLKIT_SUPERDEBUG 0
#endif

#if VARJOTOOLKIT_SUPERDEBUG

#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace VarjoToolkit::Diagnostics {

inline std::mutex& logMutex()
{
    static std::mutex mutex;
    return mutex;
}

inline const char* baseName(const char* path)
{
    if (!path) {
        return "";
    }
    const char* last = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            last = p + 1;
        }
    }
    return last;
}

inline long long elapsedMilliseconds()
{
    static const auto start = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

inline void write(const char* level, const char* file, int line, const char* function, const std::string& message)
{
    std::lock_guard<std::mutex> lock(logMutex());
    std::clog << "[VarjoToolkit][SuperDebug]"
              << "[" << elapsedMilliseconds() << "ms]"
              << "[tid=" << std::this_thread::get_id() << "]"
              << "[" << (level ? level : "INFO") << "] "
              << baseName(file) << ':' << line << ' '
              << (function ? function : "") << " - "
              << message << std::endl;
}

class ScopeLog {
public:
    ScopeLog(const char* name, const char* file, int line, const char* function)
        : name_(name ? name : "scope")
        , file_(file)
        , line_(line)
        , function_(function)
    {
        write("TRACE", file_, line_, function_, std::string("enter ") + name_);
    }

    ~ScopeLog()
    {
        write("TRACE", file_, line_, function_, std::string("leave ") + name_);
    }

    ScopeLog(const ScopeLog&) = delete;
    ScopeLog& operator=(const ScopeLog&) = delete;

private:
    const char* name_ = nullptr;
    const char* file_ = nullptr;
    int line_ = 0;
    const char* function_ = nullptr;
};

} // namespace VarjoToolkit::Diagnostics

#define VTK_SD_CONCAT_IMPL(a, b) a##b
#define VTK_SD_CONCAT(a, b) VTK_SD_CONCAT_IMPL(a, b)

#define VTK_SD_LOG_LEVEL(level_literal, message_expr) \
    do { \
        std::ostringstream vtk_sd_oss__; \
        vtk_sd_oss__ << message_expr; \
        ::VarjoToolkit::Diagnostics::write(level_literal, __FILE__, __LINE__, __func__, vtk_sd_oss__.str()); \
    } while (false)

#define VTK_SD_LOG(message_expr) VTK_SD_LOG_LEVEL("INFO", message_expr)
#define VTK_SD_WARN(message_expr) VTK_SD_LOG_LEVEL("WARN", message_expr)
#define VTK_SD_ERROR(message_expr) VTK_SD_LOG_LEVEL("ERROR", message_expr)
#define VTK_SD_TRACE(message_expr) VTK_SD_LOG_LEVEL("TRACE", message_expr)
#define VTK_SD_SCOPE(name_expr) ::VarjoToolkit::Diagnostics::ScopeLog VTK_SD_CONCAT(vtk_sd_scope_, __LINE__)(name_expr, __FILE__, __LINE__, __func__)

#else

#define VTK_SD_LOG_LEVEL(level_literal, message_expr) do {} while (false)
#define VTK_SD_LOG(message_expr) do {} while (false)
#define VTK_SD_WARN(message_expr) do {} while (false)
#define VTK_SD_ERROR(message_expr) do {} while (false)
#define VTK_SD_TRACE(message_expr) do {} while (false)
#define VTK_SD_SCOPE(name_expr) do {} while (false)

#endif
