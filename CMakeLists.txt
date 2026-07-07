cmake_minimum_required(VERSION 3.20)

project(VarjoToolkit VERSION 0.1.0 LANGUAGES CXX)

option(VARJOTOOLKIT_BUILD_SAMPLES "Build VarjoToolkit sample applications" ON)
option(VARJOTOOLKIT_BUILD_TESTS "Build VarjoToolkit tests. Some tests require a connected Varjo HMD/runtime." OFF)

# Backward compatibility for the previous option name.
if (DEFINED VARJOSERVICES_BUILD_SAMPLES)
    set(VARJOTOOLKIT_BUILD_SAMPLES ${VARJOSERVICES_BUILD_SAMPLES})
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VARJO_SDK_ROOT "" CACHE PATH "Path to the Varjo Native SDK root directory")

set(_VARJO_INCLUDE_HINTS)
set(_VARJO_LIBRARY_HINTS)
if (VARJO_SDK_ROOT)
    list(APPEND _VARJO_INCLUDE_HINTS
        "${VARJO_SDK_ROOT}/include"
        "${VARJO_SDK_ROOT}"
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

message(STATUS "Using Varjo include dir: ${VARJO_INCLUDE_DIR}")
message(STATUS "Using Varjo library: ${VARJO_LIBRARY}")

add_library(VarjoToolkit STATIC
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
    src/VarjoEyeTrackingService.cpp
    src/VarjoIMUService.cpp
    src/VarjoVSTService.cpp
    src/VarjoEyeCameraService.cpp
    src/VarjoEnvironmentCubemapService.cpp
    src/VarjoTimestampMapping.cpp
)

add_library(VarjoToolkit::VarjoToolkit ALIAS VarjoToolkit)
# Compatibility alias for older sample/application CMakeLists.
add_library(VarjoServices ALIAS VarjoToolkit)

target_include_directories(VarjoToolkit
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${VARJO_INCLUDE_DIR}"
)

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

if (MSVC)
    target_compile_options(VarjoToolkit PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(VarjoToolkit PRIVATE -Wall -Wextra -Wpedantic)
endif()

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
