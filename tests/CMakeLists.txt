function(olam_add_test_executable name)
    add_executable(${name} TestMain.cpp ${ARGN})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE olam_warnings)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

olam_add_test_executable(olam_core_tests
    core/LogTests.cpp
    core/TimeTests.cpp
    core/InputTests.cpp
    core/FilesTests.cpp
    core/LayerTests.cpp
    core/HashTests.cpp
    core/RandomTests.cpp
    core/NoiseTests.cpp
    core/SerializationTests.cpp
    render/CameraTests.cpp
)
target_link_libraries(olam_core_tests PRIVATE olam_core)

olam_add_test_executable(olam_world_tests
    world/WorldConfigTests.cpp
    world/WorldTests.cpp
)
target_link_libraries(olam_world_tests PRIVATE olam_world)

olam_add_test_executable(olam_worldgen_tests
    worldgen/WorldGeneratorTests.cpp
)
target_link_libraries(olam_worldgen_tests PRIVATE olam_worldgen)
