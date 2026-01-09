@echo off
setlocal

REM Use the provided CMake path
set CMAKE_PATH=C:\Program Files\CMake\bin
echo Using CMake at: %CMAKE_PATH%
set PATH=%CMAKE_PATH%;%PATH%

cd /d %~dp0

REM Use a fresh build directory
set BUILD_DIR=build_unit
echo Using build directory: %BUILD_DIR%

REM Create build directory if it doesn't exist
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

echo Building unit_tests...
cmake --build . --target unit_tests --config Release
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo Running unit tests...
.\bin\Release\unit_tests.exe --gtest_filter=BarcodeGeneratorTest*

endlocal
