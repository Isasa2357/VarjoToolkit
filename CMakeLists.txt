cmake_minimum_required(VERSION 3.20)

project(VarjoToolkit LANGUAGES CXX)

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

find_path(BOOST_INCLUDE_DIR
    NAMES boost/circular_buffer.hpp
)

if (NOT VARJO_INCLUDE_DIR)
    message(FATAL_ERROR "Varjo SDK include directory was not found. Set -DVARJO_SDK_ROOT=... or VARJO_INCLUDE_DIR.")
endif()
if (NOT VARJO_LIBRARY)
    message(FATAL_ERROR "Varjo SDK library was not found. Set -DVARJO_SDK_ROOT=... or VARJO_LIBRARY.")
endif()
if (NOT BOOST_INCLUDE_DIR)
    message(FATAL_ERROR "Boost headers were not found. VarjoEyeTrackingService uses boost/circular_buffer.hpp.")
endif()

add_library(VarjoToolkit STATIC
    src/VarjoSession.cpp
    src/VarjoFrameInfo.cpp
    src/VarjoCsv.cpp
    src/VarjoDataStream.cpp
    src/VarjoDataStreamBufferLock.cpp
    src/VarjoCameraProperties.cpp
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
        "${BOOST_INCLUDE_DIR}"
)

target_link_libraries(VarjoToolkit
    PUBLIC
        "${VARJO_LIBRARY}"
)

target_compile_definitions(VarjoToolkit
    PUBLIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN
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

    add_executable(VarjoToolkitCoreSmokeTest
        tests/CoreWrappersSmokeTest.cpp
    )

    target_link_libraries(VarjoToolkitCoreSmokeTest
        PRIVATE
            VarjoToolkit::VarjoToolkit
    )

    add_executable(VarjoToolkitCsvUtilityTest
        tests/CsvUtilityTest.cpp
    )

    target_link_libraries(VarjoToolkitCsvUtilityTest
        PRIVATE
            VarjoToolkit::VarjoToolkit
    )

    add_executable(VarjoToolkitDataStreamWrappersTest
        tests/DataStreamWrappersTest.cpp
    )

    target_link_libraries(VarjoToolkitDataStreamWrappersTest
        PRIVATE
            VarjoToolkit::VarjoToolkit
    )

    add_executable(VarjoToolkitCameraPropertiesTest
        tests/CameraPropertiesTest.cpp
    )

    target_link_libraries(VarjoToolkitCameraPropertiesTest
        PRIVATE
            VarjoToolkit::VarjoToolkit
    )

    foreach(_varjotoolkit_test_target IN ITEMS VarjoToolkitCoreSmokeTest VarjoToolkitCsvUtilityTest VarjoToolkitDataStreamWrappersTest VarjoToolkitCameraPropertiesTest)
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
    endforeach()

    add_test(NAME VarjoToolkitCoreSmokeTest COMMAND VarjoToolkitCoreSmokeTest)
    add_test(NAME VarjoToolkitCsvUtilityTest COMMAND VarjoToolkitCsvUtilityTest)
    add_test(NAME VarjoToolkitDataStreamWrappersTest COMMAND VarjoToolkitDataStreamWrappersTest)
    add_test(NAME VarjoToolkitCameraPropertiesTest COMMAND VarjoToolkitCameraPropertiesTest)
endif()
