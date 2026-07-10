#pragma once

#define VARJOTOOLKIT_VERSION_MAJOR 0
#define VARJOTOOLKIT_VERSION_MINOR 5
#define VARJOTOOLKIT_VERSION_PATCH 0
#define VARJOTOOLKIT_VERSION_STRING "0.5.0"

namespace VarjoToolkit {

inline constexpr int VersionMajor = VARJOTOOLKIT_VERSION_MAJOR;
inline constexpr int VersionMinor = VARJOTOOLKIT_VERSION_MINOR;
inline constexpr int VersionPatch = VARJOTOOLKIT_VERSION_PATCH;
inline constexpr const char* VersionString = VARJOTOOLKIT_VERSION_STRING;

} // namespace VarjoToolkit
