add_executable(VarjoOcclusionMeshDumpSample
    main.cpp
)

target_link_libraries(VarjoOcclusionMeshDumpSample
    PRIVATE
        VarjoToolkit::VarjoToolkit
)

target_compile_definitions(VarjoOcclusionMeshDumpSample
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(VarjoOcclusionMeshDumpSample PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(VarjoOcclusionMeshDumpSample PRIVATE -Wall -Wextra -Wpedantic)
endif()
