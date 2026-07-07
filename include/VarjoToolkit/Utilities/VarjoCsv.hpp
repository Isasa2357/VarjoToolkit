#pragma once

#include <Varjo.h>
#include <Varjo_datastream.h>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Services/EyeTracking/VarjoEyeTrackingService.hpp>

namespace VarjoToolkit::Csv {

// Generic helpers ------------------------------------------------------------

std::string number(double value);
std::string integer(long long value);
std::string boolean(bool value);

std::string join(std::initializer_list<std::string> fields);
std::string join(const std::vector<std::string>& fields);
std::string emptyFields(size_t count);

std::string makeHeader(const std::string& name, std::initializer_list<std::string> fields);
std::string makeHeader(const std::string& name, const std::vector<std::string>& fields);
std::string makeIndexedHeader(const std::string& name, const std::string& itemName, size_t count);

std::string values(const double* values, size_t count);
std::string values(const float* values, size_t count);
std::string values(const int* values, size_t count);

// Varjo Native SDK basic math types ------------------------------------------

std::string toCsv(const varjo_Vector2Df& value);
std::string headerForVector2Df(const std::string& name);

std::string toCsv(const varjo_Vector3D& value);
std::string headerForVector3D(const std::string& name);

std::string toCsv(const varjo_Matrix& value);
std::string headerForMatrix(const std::string& name);

std::string toCsv(const varjo_Matrix3x3& value);
std::string headerForMatrix3x3(const std::string& name);

std::string toCsv(const varjo_Ray& value);
std::string headerForRay(const std::string& name);

// Varjo Native SDK data structs ----------------------------------------------

std::string toCsv(const varjo_ViewInfo& value);
std::string headerForViewInfo(const std::string& name);

std::string toCsv(const varjo_BufferMetadata& value);
std::string headerForBufferMetadata(const std::string& name);

std::string toCsv(const varjo_CameraIntrinsics2& value);
std::string headerForCameraIntrinsics2(const std::string& name);

std::string toCsv(const varjo_Gaze& value);
std::string headerForGaze(const std::string& name);

std::string toCsv(const varjo_EyeMeasurements& value);
std::string headerForEyeMeasurements(const std::string& name);

// VarjoToolkit types ---------------------------------------------------------

std::string toCsv(const VarjoProjectedGazePosition& value);
std::string headerForProjectedGazePosition(const std::string& name);

std::string toCsv(const VarjoFrameInfoSnapshot& value);
std::string toCsv(const VarjoFrameInfoSnapshot& value, size_t fixedViewCount);
std::string headerForFrameInfoSnapshot(const std::string& name, size_t viewCount);

// Typed header helper. This lets callers write:
//   Csv::header<varjo_Vector3D>("coord") -> "coord.x,coord.y,coord.z"
template <typename T>
std::string header(const std::string& name);

template <>
inline std::string header<varjo_Vector2Df>(const std::string& name) { return headerForVector2Df(name); }

template <>
inline std::string header<varjo_Vector3D>(const std::string& name) { return headerForVector3D(name); }

template <>
inline std::string header<varjo_Matrix>(const std::string& name) { return headerForMatrix(name); }

template <>
inline std::string header<varjo_Matrix3x3>(const std::string& name) { return headerForMatrix3x3(name); }

template <>
inline std::string header<varjo_Ray>(const std::string& name) { return headerForRay(name); }

template <>
inline std::string header<varjo_ViewInfo>(const std::string& name) { return headerForViewInfo(name); }

template <>
inline std::string header<varjo_BufferMetadata>(const std::string& name) { return headerForBufferMetadata(name); }

template <>
inline std::string header<varjo_CameraIntrinsics2>(const std::string& name) { return headerForCameraIntrinsics2(name); }

template <>
inline std::string header<varjo_Gaze>(const std::string& name) { return headerForGaze(name); }

template <>
inline std::string header<varjo_EyeMeasurements>(const std::string& name) { return headerForEyeMeasurements(name); }

template <>
inline std::string header<VarjoProjectedGazePosition>(const std::string& name) { return headerForProjectedGazePosition(name); }

} // namespace VarjoToolkit::Csv
