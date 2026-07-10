if (NOT WIN32)
    message(STATUS "VarjoToolkit HMD smoke tests require Windows and Direct3D; skipping test targets.")
    return()
endif()

function(varjotoolkit_add_hmd_test target)
    add_executable(${target}
        ${ARGN}
    )

    target_include_directories(${target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${target}
        PRIVATE
            VarjoToolkit::VarjoToolkit
    )

    target_compile_definitions(${target}
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )

    if (MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME ${target} COMMAND ${target})
endfunction()

function(varjotoolkit_configure_service_test target labels)
    set_tests_properties(${target}
        PROPERTIES
            LABELS "${labels}"
            SKIP_RETURN_CODE 125
            TIMEOUT 30
    )
endfunction()

if (VARJOTOOLKIT_BUILD_HMD_TESTS)
    varjotoolkit_add_hmd_test(VarjoToolkitHmdCoreSmokeTest
        hmd_core_smoke.cpp
    )
    set_tests_properties(VarjoToolkitHmdCoreSmokeTest
        PROPERTIES
            LABELS "hmd;core"
    )

    varjotoolkit_add_hmd_test(VarjoToolkitHmdRenderingSmokeTest
        hmd_rendering_smoke.cpp
    )
    set_tests_properties(VarjoToolkitHmdRenderingSmokeTest
        PROPERTIES
            LABELS "hmd;rendering"
    )

    varjotoolkit_add_hmd_test(VarjoToolkitHmdD3D11SwapChainSmokeTest
        hmd_d3d11_swapchain_smoke.cpp
    )
    target_link_libraries(VarjoToolkitHmdD3D11SwapChainSmokeTest
        PRIVATE
            d3d11
            dxgi
    )
    set_tests_properties(VarjoToolkitHmdD3D11SwapChainSmokeTest
        PROPERTIES
            LABELS "hmd;rendering;d3d11"
    )

    varjotoolkit_add_hmd_test(VarjoToolkitHmdD3D12SwapChainSmokeTest
        hmd_d3d12_swapchain_smoke.cpp
    )
    target_link_libraries(VarjoToolkitHmdD3D12SwapChainSmokeTest
        PRIVATE
            d3d12
            d3d11
            dxgi
    )
    set_tests_properties(VarjoToolkitHmdD3D12SwapChainSmokeTest
        PROPERTIES
            LABELS "hmd;rendering;d3d12"
    )

    varjotoolkit_add_hmd_test(VarjoToolkitHmdVstServiceSmokeTest
        hmd_vst_service_smoke.cpp
    )
    varjotoolkit_configure_service_test(
        VarjoToolkitHmdVstServiceSmokeTest
        "hmd;service;camera;vst;performance"
    )

    varjotoolkit_add_hmd_test(VarjoToolkitHmdEyeCameraServiceSmokeTest
        hmd_eye_camera_service_smoke.cpp
    )
    varjotoolkit_configure_service_test(
        VarjoToolkitHmdEyeCameraServiceSmokeTest
        "hmd;service;camera;eyecamera;performance;optional"
    )

    varjotoolkit_add_hmd_test(VarjoToolkitHmdCubemapServiceSmokeTest
        hmd_cubemap_service_smoke.cpp
    )
    varjotoolkit_configure_service_test(
        VarjoToolkitHmdCubemapServiceSmokeTest
        "hmd;service;camera;cubemap;performance;optional"
    )

    varjotoolkit_add_hmd_test(VarjoToolkitHmdImuServiceSmokeTest
        hmd_imu_service_smoke.cpp
    )
    varjotoolkit_configure_service_test(
        VarjoToolkitHmdImuServiceSmokeTest
        "hmd;service;sensor;imu;performance"
    )

    varjotoolkit_add_hmd_test(VarjoToolkitHmdEyeTrackingServiceSmokeTest
        hmd_eye_tracking_service_smoke.cpp
    )
    varjotoolkit_configure_service_test(
        VarjoToolkitHmdEyeTrackingServiceSmokeTest
        "hmd;service;sensor;gaze;performance;optional"
    )
endif()
