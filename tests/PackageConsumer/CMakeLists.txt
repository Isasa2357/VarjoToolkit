cmake_minimum_required(VERSION 3.20)

project(VarjoToolkitPackageConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(VarjoToolkit CONFIG REQUIRED)

add_executable(VarjoToolkitPackageConsumer
    main.cpp
)

target_link_libraries(VarjoToolkitPackageConsumer
    PRIVATE
        VarjoToolkit::VarjoToolkit
)

target_compile_definitions(VarjoToolkitPackageConsumer
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(VarjoToolkitPackageConsumer PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(VarjoToolkitPackageConsumer PRIVATE -Wall -Wextra -Wpedantic)
endif()
