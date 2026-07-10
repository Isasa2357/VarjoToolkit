cmake_minimum_required(VERSION 3.20)

project(VarjoToolkit VERSION 0.4.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(VARJOTOOLKIT_BUILD_SAMPLES "Build VarjoToolkit sample applications" ON)
option(VARJOTOOLKIT_BUILD_TESTS "Build VarjoToolkit HMD-independent tests" OFF)
option(VARJOTOOLKIT_BUILD_HMD_TESTS "Build tests that require Varjo Base and a connected Varjo HMD" OFF)
option(VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS "Build experimental Varjo video post process shader wrapper" ON)
option(VARJOTOOLKIT_ENABLE_SUPERDEBUG "Enable verbose console SuperDebug diagnostics in Debug builds" OFF)

# Backward compatibility for the previous option name.
if (DEFINED VARJOSERVICES_BUILD_SAMPLES)
    set(VARJOTOOLKIT_BUILD_SAMPLES ${VARJOSERVICES_BUILD_SAMPLES})
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VARJO_SDK_ROOT "" CACHE PATH "Path to the Varjo Native SDK root directory")

set(_VARJO_INCLUDE_HINTS)
set(_VARJO_EXPERIMENTAL_INCLUDE_HINTS)
set(_VARJO_LIBRARY_HINTS)
if (VARJO_SDK_ROOT)
    list(APPEND _VARJO_INCLUDE_HINTS
        "${VARJO_SDK_ROOT}/include"
        "${VARJO_SDK_ROOT}"
    )
    list(APPEND _VARJO_EXPERIMENTAL_INCLUDE_HINTS
        "${VARJO_SDK_ROOT}/include_experimental"
    )
    list(APPEND _VARJO_LIBRARY_HINTS
        "${VARJO_SDK_ROOT}/lib"
        "${VARJO_SDK_ROOT}/lib/x64"
        "${VARJO_SDK_ROOT}/lib/win64"
        "${VARJO_SDK_ROOT}/lib/Win64"
        "${VARJO_SDK_ROOT}/Lib"
        "${VARJO_SDK_ROOT}/Lib/x64"
        "${VARJO_SDK_ROOT}/Lib/win64"
        "${VARJO_SDK_ROOT}/Lib/Win64"
    )
endif()
if (DEFINED ENV{VARJO_SDK_ROOT})
    list(APPEND _VARJO_INCLUDE_HINTS
        "$ENV{VARJO_SDK_ROOT}/include"
        "$ENV{VARJO_SDK_ROOT}"
    )
    list(APPEND _VARJO_EXPERIMENTAL_INCLUDE_HINTS
        "$ENV{VARJO_SDK_ROOT}/include_experimental"
    )
    list(APPEND _VARJO_LIBRARY_HINTS
        "$ENV{VARJO_SDK_ROOT}/lib"
        "$ENV{VARJO_SDK_ROOT}/lib/x64"
        "$ENV{VARJO_SDK_ROOT}/lib/win64"
        "$ENV{VARJO_SDK_ROOT}/lib/Win64"
        "$ENV{VARJO_SDK_ROOT}/Lib"
        "$ENV{VARJO_SDK_ROOT}/Lib/x64"
        "$ENV{VARJO_SDK_ROOT}/Lib/win64"
        "$ENV{VARJO_SDK_ROOT}/Lib/Win64"
    )
endif()

find_path(VARJO_INCLUDE_DIR
    NAMES Varjo.h Varjo_datastream.h Varjo_math.h
    HINTS ${_VARJO_INCLUDE_HINTS}
)

if (VARJO_INCLUDE_DIR)
    get_filename_component(_VARJO_INCLUDE_PARENT "${VARJO_INCLUDE_DIR}" DIRECTORY)
    list(APPEND _VARJO_EXPERIMENTAL_INCLUDE_HINTS
        "${_VARJO_INCLUDE_PARENT}/include_experimental"
    )
endif()

find_path(VARJO_EXPERIMENTAL_INCLUDE_DIR
    NAMES Varjo_mr_experimental.h Varjo_types_mr_experimental.h
    HINTS ${_VARJO_EXPERIMENTAL_INCLUDE_HINTS}
)

find_library(VARJO_LIBRARY
    NAMES VarjoLib varjo
    HINTS ${_VARJO_LIBRARY_HINTS}
)

if (NOT VARJO_INCLUDE_DIR)
    message(FATAL_ERROR "Varjo SDK include directory was not found. Set -DVARJO_SDK_ROOT=... or VARJO_INCLUDE_DIR.")
endif()
if (NOT VARJO_LIBRARY)
    message(FATAL_ERROR "Varjo SDK library was not found. Set -DVARJO_SDK_ROOT=... or VARJO_LIBRARY.")
endif()
if (VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS AND NOT VARJO_EXPERIMENTAL_INCLUDE_DIR)
    message(FATAL_ERROR "Varjo experimental include directory was not found. Set -DVARJO_SDK_ROOT=... or VARJO_EXPERIMENTAL_INCLUDE_DIR=..., or disable -DVARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=OFF.")
endif()

message(STATUS "Using Varjo include dir: ${VARJO_INCLUDE_DIR}")
message(STATUS "Using Varjo library: ${VARJO_LIBRARY}")
if (VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS)
    message(STATUS "Using Varjo experimental include dir: ${VARJO_EXPERIMENTAL_INCLUDE_DIR}")
endif()
if (VARJOTOOLKIT_ENABLE_SUPERDEBUG)
    message(STATUS "VarjoToolkit SuperDebug is enabled for Debug configuration")
endif()

set(VARJOTOOLKIT_SOURCES
    src/VarjoSession.cpp
    src/VarjoFrameInfo.cpp
    src/VarjoScopedLock.cpp
    src/VarjoEventQueue.cpp
    src/VarjoCsv.cpp
    src/VarjoDataStream.cpp
    src/VarjoDataStreamBufferLock.cpp
    src/VarjoCameraProperties.cpp
    src/VarjoChromaKey.cpp
    src/VarjoWorld.cpp
    src/VarjoMarkerTracker.cpp
    src/VarjoOcclusionMesh.cpp
    src/VarjoSwapChain.cpp
    src/VarjoLayerFrame.cpp
    src/VarjoEventService.cpp
    src/VarjoMarkerTrackingService.cpp
    src/VarjoEyeTrackingService.cpp
    src/VarjoIMUService.cpp
    src/VarjoVSTService.cpp
    src/VarjoEyeCameraService.cpp
    src/VarjoEnvironmentCubemapService.cpp
    src/VarjoTimestampMapping.cpp
)

if (VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS)
    list(APPEND VARJOTOOLKIT_SOURCES
        src/VarjoVideoPostProcessShader.cpp
    )
endif()

add_library(VarjoToolkit STATIC
    ${VARJOTOOLKIT_SOURCES}
)

add_library(VarjoToolkit::VarjoToolkit ALIAS VarjoToolkit)

target_include_directories(VarjoToolkit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${VARJO_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${VARJO_EXPERIMENTAL_INCLUDE_DIR}
)

target_link_libraries(VarjoToolkit
    PUBLIC
        ${VARJO_LIBRARY}
)

target_compile_definitions(VarjoToolkit
    PUBLIC
        NOMINMAX
        $<$<BOOL:${VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS}>:VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=1>
        VARJOTOOLKIT_SUPERDEBUG=$<IF:$<AND:$<CONFIG:Debug>,$<BOOL:${VARJOTOOLKIT_ENABLE_SUPERDEBUG}>>,1,0>
)

if (WIN32)
    target_link_libraries(VarjoToolkit
        PUBLIC
            dxgi
            d3d11
            d3d12
            dxguid
    )
endif()

set_target_properties(VarjoToolkit PROPERTIES
    EXPORT_NAME VarjoToolkit
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS VarjoToolkit
    EXPORT VarjoToolkitTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT VarjoToolkitTargets
    NAMESPACE VarjoToolkit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VarjoToolkit
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VarjoToolkitConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/VarjoToolkitConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VarjoToolkit
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/VarjoToolkitConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/VarjoToolkitConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/VarjoToolkitConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VarjoToolkit
)

if (VARJOTOOLKIT_BUILD_TESTS OR VARJOTOOLKIT_BUILD_HMD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (VARJOTOOLKIT_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()
