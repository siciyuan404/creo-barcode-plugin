@echo off
setlocal

set BUILD_DIR=build_dll
set CREO_TOOLKIT=C:\Program Files\PTC\Creo 8.0.9.0\Common Files\protoolkit

echo Creating build directory...
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo Configuring CMake...
cmake -S . -B %BUILD_DIR% -DCREO_TOOLKIT_DIR="%CREO_TOOLKIT%" -DBUILD_TESTS=OFF -G "Visual Studio 17 2022" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    exit /b 1
)

echo Building Release configuration...
cmake --build %BUILD_DIR% --config Release --target creo_barcode_plugin

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)

echo.
echo Build successful!
echo DLL location: %BUILD_DIR%\bin\Release\creo_barcode.dll
echo.

endlocal
