@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "PRESET=msvc-x64-debug"
set "CONFIG=Debug"
set "BUILD_DIR=build\msvc-x64-debug"

:parse
if "%~1"=="" goto run
if /I "%~1"=="release" (
    set "PRESET=msvc-x64-release"
    set "CONFIG=Release"
    set "BUILD_DIR=build\msvc-x64-release"
    shift
    goto parse
)
if /I "%~1"=="clean" (
    set "CLEAN=1"
    shift
    goto parse
)
if /I "%~1"=="help" (
    echo Usage: build.bat [release] [clean]
    exit /b 0
)
echo Unknown option: %~1
exit /b 1

:run
if defined CLEAN (
    echo Removing %BUILD_DIR%
    if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo [error] CMake was not found in PATH.
    exit /b 1
)

echo [1/3] Configuring: %PRESET%
cmake --preset "%PRESET%" || exit /b 1

echo [2/3] Building: %CONFIG%
cmake --build --preset "%PRESET%" --config "%CONFIG%" || exit /b 1

echo [3/3] Running tests
ctest --preset "%PRESET%" || exit /b 1

echo Build completed successfully.
endlocal