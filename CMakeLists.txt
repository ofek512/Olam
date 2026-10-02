cmake_minimum_required(VERSION 3.24)

project(Olam VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OLAM_BUILD_TESTS "Build Olam unit tests" ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.16
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# ---------------------------------------------------------------------------
# Warnings
# ---------------------------------------------------------------------------
add_library(olam_warnings INTERFACE)
if(MSVC)
    target_compile_options(olam_warnings INTERFACE /W4 /permissive- /utf-8 /MP)
else()
    target_compile_options(olam_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# olam_core: pure C++ (no SDL), unit-testable.
# ---------------------------------------------------------------------------
add_library(olam_core STATIC
    src/core/logging/Log.cpp
    src/core/time/FrameTimer.cpp
    src/core/time/SimulationClock.cpp
    src/core/input/Input.cpp
    src/core/files/FileSystem.cpp
    src/render/camera/Camera2D.cpp
)
target_include_directories(olam_core PUBLIC src)
target_link_libraries(olam_core PRIVATE olam_warnings)

# ---------------------------------------------------------------------------
# olam_engine: platform (SDL3), rendering and the application loop.
# ---------------------------------------------------------------------------
add_library(olam_engine STATIC
    src/platform/sdl/SdlPlatform.cpp
    src/render/renderer/Renderer.cpp
    src/render/debug_render/DebugOverlay.cpp
    src/render/debug_render/TestPattern.cpp
    src/core/application/Application.cpp
)
target_link_libraries(olam_engine
    PUBLIC olam_core SDL3::SDL3-static
    PRIVATE olam_warnings
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(Olam src/main.cpp)
target_link_libraries(Olam PRIVATE olam_engine olam_warnings)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Olam)

if(OLAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
