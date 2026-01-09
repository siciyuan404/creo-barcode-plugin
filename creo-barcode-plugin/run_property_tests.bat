@echo off
setlocal

REM Use the provided CMake path
set CMAKE_PATH=C:\Program Files\CMake\bin
echo Using CMake at: %CMAKE_PATH%
set PATH=%CMAKE_PATH%;%PATH%

pushd %~dp0

REM Use the existing build directory
set BUILD_DIR=build_unit
echo Using build directory: %BUILD_DIR%

echo Building property_tests...
cmake --build %BUILD_DIR% --target property_tests --config Release
if errorlevel 1 (
    echo Build failed!
    popd
    exit /b 1
)

echo Running property tests for Property 4 (Grid Position Calculation)...
.\%BUILD_DIR%\bin\Release\property_tests.exe --gtest_filter=*Property4_COM_Grid*

popd
endlocal
