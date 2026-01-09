cmake_minimum_required(VERSION 3.16)
project(CreoBarcodePlugin VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add UTF-8 support for MSVC to handle Unicode characters in source files
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Options
option(BUILD_TESTS "Build unit tests" ON)
option(CREO_TOOLKIT_DIR "Path to Creo Toolkit installation" "")

# Force Release runtime for all targets (required for Pro/TOOLKIT compatibility)
# This must be set BEFORE any targets are created (including FetchContent)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find Creo Toolkit (optional for development without Creo)
if(CREO_TOOLKIT_DIR)
    # CREO_TOOLKIT_DIR 应该直接指向 protoolkit 目录
    set(PROTOOLKIT_INCLUDE_DIR "${CREO_TOOLKIT_DIR}/includes")
    set(PROTOOLKIT_LIB_DIR "${CREO_TOOLKIT_DIR}/x86e_win64/obj")
    
    message(STATUS "Looking for Creo Toolkit at: ${CREO_TOOLKIT_DIR}")
    message(STATUS "Include dir: ${PROTOOLKIT_INCLUDE_DIR}")
    message(STATUS "Lib dir: ${PROTOOLKIT_LIB_DIR}")
    
    if(EXISTS ${PROTOOLKIT_INCLUDE_DIR})
        message(STATUS "Creo Toolkit found!")
        include_directories(${PROTOOLKIT_INCLUDE_DIR})
        link_directories(${PROTOOLKIT_LIB_DIR})
        add_definitions(-DHAS_CREO_TOOLKIT)
        set(HAS_CREO_TOOLKIT TRUE)
    else()
        message(WARNING "Creo Toolkit includes not found at ${PROTOOLKIT_INCLUDE_DIR}")
        set(HAS_CREO_TOOLKIT FALSE)
    endif()
else()
    set(HAS_CREO_TOOLKIT FALSE)
endif()

# Fetch dependencies
include(FetchContent)

# ZXing-cpp for barcode generation/decoding
FetchContent_Declare(
    zxing-cpp
    GIT_REPOSITORY https://github.com/zxing-cpp/zxing-cpp.git
    GIT_TAG v2.2.1
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zxing-cpp)

# nlohmann/json for configuration
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# stb for image writing (header-only)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)


# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

# Source files
set(PLUGIN_SOURCES
    src/barcode_generator.cpp
    src/config_manager.cpp
    src/batch_processor.cpp
    src/logger.cpp
    src/version_check.cpp
    src/menu_manager.cpp
    src/settings_dialog.cpp
    src/drawing_interface.cpp
    src/data_sync_checker.cpp
    src/creo_com_bridge.cpp
)

# Create static library for core functionality (testable without Creo)
add_library(barcode_core STATIC ${PLUGIN_SOURCES})
target_link_libraries(barcode_core PUBLIC ZXing::ZXing nlohmann_json::nlohmann_json)
target_include_directories(barcode_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Link Windows COM libraries on Windows platform
if(WIN32)
    target_link_libraries(barcode_core PUBLIC ole32 oleaut32)
endif()

# Main plugin DLL (requires Creo Toolkit)
if(HAS_CREO_TOOLKIT)
    message(STATUS "Building Creo plugin DLL (Pure C version)...")
    
    # Find Creo Toolkit libraries (Creo 8.0 uses _NU suffix)
    find_library(PROTOOLKIT_LIB 
        NAMES protk_dllmd_NU protk_dll_NU protoolkit_NU protoolkit protk_dll
        PATHS "${PROTOOLKIT_LIB_DIR}"
        NO_DEFAULT_PATH
    )
    find_library(UCORE_LIB 
        NAMES ucore
        PATHS "${PROTOOLKIT_LIB_DIR}"
        NO_DEFAULT_PATH
    )
    find_library(UDATA_LIB 
        NAMES udata
        PATHS "${PROTOOLKIT_LIB_DIR}"
        NO_DEFAULT_PATH
    )
    
    message(STATUS "PROTOOLKIT_LIB: ${PROTOOLKIT_LIB}")
    message(STATUS "UCORE_LIB: ${UCORE_LIB}")
    message(STATUS "UDATA_LIB: ${UDATA_LIB}")
    
    # Build plugin with pure C implementation (no C++ dependencies)
    add_library(creo_barcode_plugin SHARED
        src/main_pure_c.c
        src/barcode_pure_c.c
        src/creo_barcode.def
    )
    
    # No C++ libraries needed - pure C implementation
    # target_link_libraries(creo_barcode_plugin PRIVATE barcode_core)
    
    # Link with found libraries
    if(PROTOOLKIT_LIB)
        target_link_libraries(creo_barcode_plugin PRIVATE 
            ${PROTOOLKIT_LIB}
            # Windows libraries required by Pro/TOOLKIT
            ws2_32.lib
            mpr.lib
            netapi32.lib
        )
        if(UCORE_LIB)
            target_link_libraries(creo_barcode_plugin PRIVATE ${UCORE_LIB})
        endif()
        if(UDATA_LIB)
            target_link_libraries(creo_barcode_plugin PRIVATE ${UDATA_LIB})
        endif()
    else()
        message(WARNING "Creo Toolkit libraries not found")
    endif()
    
    set_target_properties(creo_barcode_plugin PROPERTIES
        OUTPUT_NAME "creo_barcode"
        SUFFIX ".dll"
    )
else()
    message(STATUS "Creo Toolkit not found - building standalone executable for testing")
    add_executable(creo_barcode_standalone
        src/main.cpp
    )
    target_link_libraries(creo_barcode_standalone PRIVATE barcode_core)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
install(TARGETS barcode_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY resources/ DESTINATION share/creo_barcode)
