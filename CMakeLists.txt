cmake_minimum_required(VERSION 3.24)

project(Olam VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OLAM_BUILD_ENGINE "Build SDL-dependent targets (engine, viewer, executable)" ON)
option(OLAM_BUILD_TESTS "Build Olam unit tests" ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
if(OLAM_BUILD_ENGINE)
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
endif()

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
add_library(olam_warnings INTERFACE)
if(MSVC)
    target_compile_options(olam_warnings INTERFACE /W4 /permissive- /utf-8 /MP)
else()
    target_compile_options(olam_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Bit-reproducible floating point for deterministic world generation.
add_library(olam_determinism INTERFACE)
if(MSVC)
    target_compile_options(olam_determinism INTERFACE /fp:precise)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(olam_determinism INTERFACE /clang:-ffp-contract=off)
    endif()
else()
    target_compile_options(olam_determinism INTERFACE -ffp-contract=off -fno-fast-math)
endif()

# ---------------------------------------------------------------------------
# olam_core: generic infrastructure, no SDL, no world knowledge.
# ---------------------------------------------------------------------------
add_library(olam_core STATIC
    src/core/logging/Log.cpp
    src/core/time/FrameTimer.cpp
    src/core/time/SimulationClock.cpp
    src/core/input/Input.cpp
    src/core/files/FileSystem.cpp
    src/core/debug/Assert.cpp
    src/core/hash/XxHash64.cpp
    src/core/noise/Noise.cpp
    src/core/random/Pcg32.cpp
    src/core/random/Seed.cpp
    src/render/camera/Camera2D.cpp
)
target_include_directories(olam_core PUBLIC src)
target_link_libraries(olam_core
    PUBLIC olam_determinism
    PRIVATE olam_warnings
)

# ---------------------------------------------------------------------------
# olam_world: strategic world data model.
# ---------------------------------------------------------------------------
add_library(olam_world STATIC
    src/world/WorldConfig.cpp
    src/world/World.cpp
    src/world/TileView.cpp
    src/world/WorldLayers.cpp
    src/world/WorldHash.cpp
    src/world/WorldStats.cpp
    src/world/WorldTypes.cpp
    src/world/WorldIO.cpp
    src/world/queries/TerrainQueries.cpp
    src/world/queries/HydrologyQueries.cpp
    src/world/queries/AgricultureQueries.cpp
    src/world/queries/ResourceQueries.cpp
)
target_link_libraries(olam_world
    PUBLIC olam_core
    PRIVATE olam_warnings
)

# ---------------------------------------------------------------------------
# olam_worldgen: generation pipeline.
# ---------------------------------------------------------------------------
add_library(olam_worldgen STATIC
    src/worldgen/WorldGenContext.cpp
    src/worldgen/WorldGenerator.cpp
    src/worldgen/DefaultPipeline.cpp
    src/worldgen/util/Blur.cpp
    src/worldgen/util/DistanceTransform.cpp
    src/worldgen/passes/TectonicsPass.cpp
    src/worldgen/passes/ElevationPass.cpp
    src/worldgen/passes/OceanPass.cpp
    src/worldgen/passes/TemperaturePass.cpp
    src/worldgen/passes/RainfallPass.cpp
    src/worldgen/passes/HydrologyPass.cpp
    src/worldgen/passes/SoilPass.cpp
    src/worldgen/passes/BiomePass.cpp
    src/worldgen/passes/FertilityPass.cpp
    src/worldgen/passes/VegetationPass.cpp
    src/worldgen/passes/ResourcePass.cpp
)
target_link_libraries(olam_worldgen
    PUBLIC olam_world
    PRIVATE olam_warnings
)

# Headless world generator (timings, stats, saving); no SDL.
add_executable(olam_generate src/tools/generate/main.cpp)
target_link_libraries(olam_generate PRIVATE olam_worldgen olam_warnings)

# ---------------------------------------------------------------------------
# SDL-dependent targets
# ---------------------------------------------------------------------------
if(OLAM_BUILD_ENGINE)
    add_library(olam_engine STATIC
        src/platform/sdl/SdlPlatform.cpp
        src/render/renderer/Renderer.cpp
        src/render/texture/Texture.cpp
        src/render/debug_render/TextPanel.cpp
    )
    target_link_libraries(olam_engine
        PUBLIC olam_core SDL3::SDL3-static
        PRIVATE olam_warnings
    )

    add_library(olam_world_viewer STATIC
        src/tools/world_viewer/WorldDebugRenderer.cpp
        src/tools/world_viewer/WorldViews.cpp
        src/tools/world_viewer/ColorRamp.cpp
        src/tools/world_viewer/TileInspector.cpp
    )
    target_link_libraries(olam_world_viewer
        PUBLIC olam_engine olam_world
        PRIVATE olam_warnings
    )

    add_executable(Olam
        src/app/main.cpp
        src/app/Application.cpp
        src/app/CommandLine.cpp
    )
    target_link_libraries(Olam PRIVATE olam_world_viewer olam_worldgen olam_warnings)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Olam)
endif()

if(OLAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
