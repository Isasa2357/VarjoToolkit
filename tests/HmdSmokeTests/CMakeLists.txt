function(varjotoolkit_add_hmd_smoke_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target}
        PRIVATE
            VarjoToolkit::VarjoToolkit
            ${ARGN}
    )
    target_include_directories(${target}
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}
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
    set_tests_properties(${target} PROPERTIES LABELS "hmd")
endfunction()

varjotoolkit_add_hmd_smoke_test(VarjoToolkitHmdOcclusionMeshSmokeTest HmdOcclusionMeshSmokeTest.cpp)
varjotoolkit_add_hmd_smoke_test(VarjoToolkitHmdEventQueueSmokeTest HmdEventQueueSmokeTest.cpp)
varjotoolkit_add_hmd_smoke_test(VarjoToolkitHmdWorldMarkerSmokeTest HmdWorldMarkerSmokeTest.cpp)
varjotoolkit_add_hmd_smoke_test(VarjoToolkitHmdCameraPropertiesSmokeTest HmdCameraPropertiesSmokeTest.cpp)
varjotoolkit_add_hmd_smoke_test(VarjoToolkitHmdDataStreamConfigSmokeTest HmdDataStreamConfigSmokeTest.cpp)
varjotoolkit_add_hmd_smoke_test(VarjoToolkitHmdD3D11SwapChainSmokeTest HmdD3D11SwapChainSmokeTest.cpp d3d11 dxgi)
varjotoolkit_add_hmd_smoke_test(VarjoToolkitHmdD3D12SwapChainSmokeTest HmdD3D12SwapChainSmokeTest.cpp d3d12 d3d11 dxgi)

set_tests_properties(VarjoToolkitHmdCameraPropertiesSmokeTest PROPERTIES LABELS "hmd;mr")
set_tests_properties(VarjoToolkitHmdDataStreamConfigSmokeTest PROPERTIES LABELS "hmd;datastream")
set_tests_properties(VarjoToolkitHmdD3D11SwapChainSmokeTest PROPERTIES LABELS "hmd;d3d11;rendering")
set_tests_properties(VarjoToolkitHmdD3D12SwapChainSmokeTest PROPERTIES LABELS "hmd;d3d12;rendering")

if (VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS)
    varjotoolkit_add_hmd_smoke_test(VarjoToolkitHmdVideoPostProcessD3D11SmokeTest HmdVideoPostProcessD3D11SmokeTest.cpp d3d11 dxgi d3dcompiler)
    varjotoolkit_add_hmd_smoke_test(VarjoToolkitHmdVideoPostProcessD3D12SmokeTest HmdVideoPostProcessD3D12SmokeTest.cpp d3d12 d3d11 dxgi d3dcompiler)
    set_tests_properties(VarjoToolkitHmdVideoPostProcessD3D11SmokeTest PROPERTIES LABELS "hmd;mr;experimental;vpp;d3d11")
    set_tests_properties(VarjoToolkitHmdVideoPostProcessD3D12SmokeTest PROPERTIES LABELS "hmd;mr;experimental;vpp;d3d12")
endif()
