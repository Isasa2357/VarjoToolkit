add_executable(VarjoD3D12RenderSample
    main.cpp
)

target_link_libraries(VarjoD3D12RenderSample
    PRIVATE
        VarjoToolkit::VarjoToolkit
        d3d12
        d3d11
        dxgi
        d3dcompiler
)

# Varjo exposes the runtime adapter LUID through varjo_D3D11GetLuid for both
# Direct3D backends. Make its declaration available to the D3D12 sample.
target_precompile_headers(VarjoD3D12RenderSample
    PRIVATE
        <Varjo_d3d11.h>
)

target_compile_definitions(VarjoD3D12RenderSample
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(VarjoD3D12RenderSample PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(VarjoD3D12RenderSample PRIVATE -Wall -Wextra -Wpedantic)
endif()
