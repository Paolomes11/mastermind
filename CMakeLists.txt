cmake_minimum_required(VERSION 3.16)
project(mastermind VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Per-config compile options
add_compile_options(
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
add_link_options(
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# Core static library
add_library(mastermind_core STATIC
    src/code.cpp
    src/feedback.cpp
    src/strategy.cpp
    src/strategy_random.cpp
    src/strategy_entropy.cpp
    src/strategy_minimax.cpp
    src/solver.cpp
)
target_include_directories(mastermind_core PUBLIC include)

# Main executable
add_executable(mastermind main.cpp)
target_link_libraries(mastermind PRIVATE mastermind_core)

# Dedicated benchmark executable
add_executable(mastermind_bench benchmarks/benchmark.cpp)
target_link_libraries(mastermind_bench PRIVATE mastermind_core)

# Link-time optimisation for Release builds
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok OUTPUT _ipo_out)
if(ipo_ok)
    foreach(tgt mastermind_core mastermind mastermind_bench)
        set_property(TARGET ${tgt} PROPERTY
                     INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endforeach()
endif()

# GUI (requires SDL2 + OpenGL — install libsdl2-dev)
option(BUILD_GUI "Build Dear ImGui interactive GUI" ON)
if(BUILD_GUI)
    add_subdirectory(gui)
endif()
