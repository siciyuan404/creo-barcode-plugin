# Test configuration
include(FetchContent)

# Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# RapidCheck for property-based testing
FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG master
)
set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rapidcheck)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Unit tests
add_executable(unit_tests
    test_barcode_generator.cpp
    test_config_manager.cpp
    test_batch_processor.cpp
    test_version_check.cpp
    test_settings_dialog.cpp
    test_data_sync_checker.cpp
)

target_link_libraries(unit_tests PRIVATE
    barcode_core
    GTest::gtest
    GTest::gtest_main
)

# Property-based tests
add_executable(property_tests
    property_tests.cpp
)

target_link_libraries(property_tests PRIVATE
    barcode_core
    GTest::gtest
    GTest::gtest_main
    rapidcheck
    rapidcheck_gtest
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(unit_tests)
gtest_discover_tests(property_tests)
