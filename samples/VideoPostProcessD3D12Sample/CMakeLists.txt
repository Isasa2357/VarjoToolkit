add_executable(VarjoVideoPostProcessD3D12Sample
    main.cpp
)

target_link_libraries(VarjoVideoPostProcessD3D12Sample
    PRIVATE
        VarjoToolkit::VarjoToolkit
        d3d12
        d3d11
        dxgi
        d3dcompiler
)

target_compile_definitions(VarjoVideoPostProcessD3D12Sample
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(VarjoVideoPostProcessD3D12Sample PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(VarjoVideoPostProcessD3D12Sample PRIVATE -Wall -Wextra -Wpedantic)
endif()
