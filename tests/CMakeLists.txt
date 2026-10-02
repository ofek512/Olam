add_executable(olam_tests
    TestMain.cpp
    core/LogTests.cpp
    core/TimeTests.cpp
    core/InputTests.cpp
    core/FilesTests.cpp
    render/CameraTests.cpp
)
target_include_directories(olam_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(olam_tests PRIVATE olam_core olam_warnings)

add_test(NAME olam_tests COMMAND olam_tests)
