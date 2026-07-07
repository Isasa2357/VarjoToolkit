if (NOT DEFINED VARJOTOOLKIT_SOURCE_DIR OR VARJOTOOLKIT_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "VARJOTOOLKIT_SOURCE_DIR is required")
endif()
if (NOT DEFINED VARJOTOOLKIT_BINARY_DIR OR VARJOTOOLKIT_BINARY_DIR STREQUAL "")
    message(FATAL_ERROR "VARJOTOOLKIT_BINARY_DIR is required")
endif()
if (NOT DEFINED VARJOTOOLKIT_PACKAGE_TEST_DIR OR VARJOTOOLKIT_PACKAGE_TEST_DIR STREQUAL "")
    message(FATAL_ERROR "VARJOTOOLKIT_PACKAGE_TEST_DIR is required")
endif()
if (NOT DEFINED VARJOTOOLKIT_CONFIG OR VARJOTOOLKIT_CONFIG STREQUAL "")
    set(VARJOTOOLKIT_CONFIG Debug)
endif()

set(install_dir "${VARJOTOOLKIT_PACKAGE_TEST_DIR}/install")
set(consumer_build_dir "${VARJOTOOLKIT_PACKAGE_TEST_DIR}/build")
file(REMOVE_RECURSE "${install_dir}" "${consumer_build_dir}")
file(MAKE_DIRECTORY "${VARJOTOOLKIT_PACKAGE_TEST_DIR}")

message(STATUS "Installing VarjoToolkit to ${install_dir}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${VARJOTOOLKIT_BINARY_DIR}" --config "${VARJOTOOLKIT_CONFIG}" --prefix "${install_dir}"
    RESULT_VARIABLE install_result
)
if (NOT install_result EQUAL 0)
    message(FATAL_ERROR "VarjoToolkit install step failed with code ${install_result}")
endif()

set(configure_args
    "${CMAKE_COMMAND}"
    -S "${VARJOTOOLKIT_SOURCE_DIR}/tests/PackageConsumer"
    -B "${consumer_build_dir}"
    -DCMAKE_PREFIX_PATH=${install_dir}
)

if (DEFINED VARJOTOOLKIT_GENERATOR AND NOT VARJOTOOLKIT_GENERATOR STREQUAL "")
    list(APPEND configure_args -G "${VARJOTOOLKIT_GENERATOR}")
endif()
if (DEFINED VARJOTOOLKIT_GENERATOR_PLATFORM AND NOT VARJOTOOLKIT_GENERATOR_PLATFORM STREQUAL "")
    list(APPEND configure_args -A "${VARJOTOOLKIT_GENERATOR_PLATFORM}")
endif()
if (DEFINED VARJOTOOLKIT_GENERATOR_TOOLSET AND NOT VARJOTOOLKIT_GENERATOR_TOOLSET STREQUAL "")
    list(APPEND configure_args -T "${VARJOTOOLKIT_GENERATOR_TOOLSET}")
endif()

message(STATUS "Configuring VarjoToolkit package consumer")
execute_process(
    COMMAND ${configure_args}
    RESULT_VARIABLE configure_result
)
if (NOT configure_result EQUAL 0)
    message(FATAL_ERROR "VarjoToolkit package consumer configure failed with code ${configure_result}")
endif()

message(STATUS "Building VarjoToolkit package consumer")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_build_dir}" --config "${VARJOTOOLKIT_CONFIG}"
    RESULT_VARIABLE build_result
)
if (NOT build_result EQUAL 0)
    message(FATAL_ERROR "VarjoToolkit package consumer build failed with code ${build_result}")
endif()
