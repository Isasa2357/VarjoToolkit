add_executable(VarjoServiceLoggerSample
    main.cpp
)

target_link_libraries(VarjoServiceLoggerSample
    PRIVATE
        VarjoServices
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
