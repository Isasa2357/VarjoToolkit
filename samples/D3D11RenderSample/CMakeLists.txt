add_executable(VarjoD3D11RenderSample
    main.cpp
)

target_link_libraries(VarjoD3D11RenderSample
    PRIVATE
        VarjoToolkit::VarjoToolkit
        d3d11
        dxgi
        d3dcompiler
)

target_compile_definitions(VarjoD3D11RenderSample
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(VarjoD3D11RenderSample PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(VarjoD3D11RenderSample PRIVATE -Wall -Wextra -Wpedantic)
endif()
