cmake_minimum_required(VERSION 3.20)

project(VarjoToolkit VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(VARJOTOOLKIT_BUILD_SAMPLES "Build VarjoToolkit sample applications" ON)
option(VARJOTOOLKIT_BUILD_TESTS "Build VarjoToolkit tests. Some tests require a connected Varjo HMD/runtime." OFF)
option(VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS "Build experimental Varjo video post process shader wrapper" ON)

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
# Compatibility alias for older sample/application CMakeLists.
add_library(VarjoServices ALIAS VarjoToolkit)

set_target_properties(VarjoToolkit PROPERTIES
    VERSION ${PROJECT_VERSION}
    EXPORT_NAME VarjoToolkit
)

target_include_directories(VarjoToolkit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        "${VARJO_INCLUDE_DIR}"
)

if (VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS)
    target_include_directories(VarjoToolkit
        PUBLIC
            "${VARJO_EXPERIMENTAL_INCLUDE_DIR}"
    )
endif()

target_link_libraries(VarjoToolkit
    PUBLIC
        "${VARJO_LIBRARY}"
)

target_compile_definitions(VarjoToolkit
    PUBLIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        VARJOTOOLKIT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        VARJOTOOLKIT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        VARJOTOOLKIT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        VARJOTOOLKIT_VERSION_STRING="${PROJECT_VERSION}"
)

if (VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS)
    target_compile_definitions(VarjoToolkit PUBLIC VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=1)
else()
    target_compile_definitions(VarjoToolkit PUBLIC VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=0)
endif()

if (MSVC)
    target_compile_options(VarjoToolkit PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(VarjoToolkit PRIVATE -Wall -Wextra -Wpedantic)
endif()

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
    FILE VarjoToolkitTargets.cmake
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

if (VARJOTOOLKIT_BUILD_SAMPLES)
    add_subdirectory(samples/ServiceLoggerSample)
    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT VarjoServiceLoggerSample)
endif()

if (VARJOTOOLKIT_BUILD_TESTS)
    enable_testing()

    set(_varjotoolkit_test_targets)

    add_executable(VarjoToolkitCoreSmokeTest
        tests/CoreWrappersSmokeTest.cpp
    )
    list(APPEND _varjotoolkit_test_targets VarjoToolkitCoreSmokeTest)

    add_executable(VarjoToolkitCsvUtilityTest
        tests/CsvUtilityTest.cpp
    )
    list(APPEND _varjotoolkit_test_targets VarjoToolkitCsvUtilityTest)

    add_executable(VarjoToolkitDataStreamWrappersTest
        tests/DataStreamWrappersTest.cpp
    )
    list(APPEND _varjotoolkit_test_targets VarjoToolkitDataStreamWrappersTest)

    add_executable(VarjoToolkitCameraPropertiesTest
        tests/CameraPropertiesTest.cpp
    )
    list(APPEND _varjotoolkit_test_targets VarjoToolkitCameraPropertiesTest)

    add_executable(VarjoToolkitScopedLockChromaKeyTest
        tests/ScopedLockChromaKeyTest.cpp
    )
    list(APPEND _varjotoolkit_test_targets VarjoToolkitScopedLockChromaKeyTest)

    add_executable(VarjoToolkitRenderingWorldWrappersTest
        tests/RenderingWorldWrappersTest.cpp
    )
    list(APPEND _varjotoolkit_test_targets VarjoToolkitRenderingWorldWrappersTest)

    add_executable(VarjoToolkitQueueTimestampUtilityTest
        tests/QueueTimestampUtilityTest.cpp
    )
    list(APPEND _varjotoolkit_test_targets VarjoToolkitQueueTimestampUtilityTest)

    add_executable(VarjoToolkitCameraChromaDetailedTest
        tests/CameraChromaDetailedTest.cpp
    )
    list(APPEND _varjotoolkit_test_targets VarjoToolkitCameraChromaDetailedTest)

    add_executable(VarjoToolkitRenderingWorldDetailedTest
        tests/RenderingWorldDetailedTest.cpp
    )
    list(APPEND _varjotoolkit_test_targets VarjoToolkitRenderingWorldDetailedTest)

    add_executable(VarjoToolkitEventMarkerServicesTest
        tests/EventMarkerServicesTest.cpp
    )
    list(APPEND _varjotoolkit_test_targets VarjoToolkitEventMarkerServicesTest)

    if (VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS)
        add_executable(VarjoToolkitVideoPostProcessShaderTest
            tests/VideoPostProcessShaderTest.cpp
        )
        list(APPEND _varjotoolkit_test_targets VarjoToolkitVideoPostProcessShaderTest)
    endif()

    foreach(_varjotoolkit_test_target IN LISTS _varjotoolkit_test_targets)
        target_link_libraries(${_varjotoolkit_test_target}
            PRIVATE
                VarjoToolkit::VarjoToolkit
        )

        target_compile_definitions(${_varjotoolkit_test_target}
            PRIVATE
                NOMINMAX
                WIN32_LEAN_AND_MEAN
        )

        if (MSVC)
            target_compile_options(${_varjotoolkit_test_target} PRIVATE /utf-8 /W4 /permissive-)
        else()
            target_compile_options(${_varjotoolkit_test_target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME ${_varjotoolkit_test_target} COMMAND ${_varjotoolkit_test_target})
    endforeach()
endif()
