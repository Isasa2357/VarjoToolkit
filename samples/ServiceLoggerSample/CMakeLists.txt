add_executable(VarjoServiceLoggerSample
    main.cpp
    WrapperExamples.cpp
)

target_link_libraries(VarjoServiceLoggerSample
    PRIVATE
        VarjoToolkit::VarjoToolkit
)

target_compile_definitions(VarjoServiceLoggerSample
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(VarjoServiceLoggerSample PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(VarjoServiceLoggerSample PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../D3D11RenderSample"
    "${CMAKE_BINARY_DIR}/samples/D3D11RenderSample"
)

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../D3D12RenderSample"
    "${CMAKE_BINARY_DIR}/samples/D3D12RenderSample"
)
