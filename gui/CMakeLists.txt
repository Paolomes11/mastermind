cmake_minimum_required(VERSION 3.16)
include(FetchContent)

# ── Dear ImGui ────────────────────────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_options(imgui_lib PRIVATE -w)

# ── ImPlot ────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.17
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(implot)

add_library(implot_lib STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot_lib PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot_lib PUBLIC imgui_lib)
target_compile_options(implot_lib PRIVATE -w)

# ── SDL2 ──────────────────────────────────────────────────────────────────────
find_package(SDL2 REQUIRED)

# Guard: some distros export SDL2::SDL2, others only SDL2_LIBRARIES/SDL2_INCLUDE_DIRS
if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
    )
endif()

# ── OpenGL ────────────────────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)

# Link SDL2+OpenGL into imgui backends
target_link_libraries(imgui_lib PUBLIC SDL2::SDL2 OpenGL::GL)

# ── GUI executable ────────────────────────────────────────────────────────────
add_executable(mastermind_gui
    main_gui.cpp
    app_state.cpp
    render.cpp
)

target_include_directories(mastermind_gui PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mastermind_gui PRIVATE
    mastermind_core
    imgui_lib
    implot_lib
    SDL2::SDL2
    OpenGL::GL
)

# Pass the path to DroidSans.ttf (bundled with ImGui) so font can be re-rasterized at any scale
target_compile_definitions(mastermind_gui PRIVATE
    IMGUI_FONT_PATH="${imgui_SOURCE_DIR}/misc/fonts/DroidSans.ttf"
)

# GUI uses -O2 (not -march=native/-O3/-fsanitize from root flags)
# Override by setting per-target options after the global ones
target_compile_options(mastermind_gui PRIVATE
    $<$<CONFIG:Release>:-O2>
)
