add_executable(VarjoVideoPostProcessD3D11Sample
    main.cpp
)

target_link_libraries(VarjoVideoPostProcessD3D11Sample
    PRIVATE
        VarjoToolkit::VarjoToolkit
        d3d11
        dxgi
        d3dcompiler
)

target_compile_definitions(VarjoVideoPostProcessD3D11Sample
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(VarjoVideoPostProcessD3D11Sample PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(VarjoVideoPostProcessD3D11Sample PRIVATE -Wall -Wextra -Wpedantic)
endif()
