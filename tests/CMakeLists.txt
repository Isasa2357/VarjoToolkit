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
endif()
