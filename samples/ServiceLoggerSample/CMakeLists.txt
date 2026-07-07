add_executable(VarjoServiceLoggerSample
    main.cpp
    WrapperExamples.cpp
)

target_link_libraries(VarjoServiceLoggerSample
    PRIVATE
        VarjoToolkit::VarjoToolkit
)

if (MSVC)
    target_compile_options(VarjoServiceLoggerSample PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(VarjoServiceLoggerSample PRIVATE -Wall -Wextra -Wpedantic)
endif()
