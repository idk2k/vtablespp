@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "PRESET=msvc-x64-debug"
set "CONFIG=Debug"
set "BUILD_DIR=build\msvc-x64-debug"
set "CMAKE_ARGS="

echo %* | findstr /I "\<release\>" >nul && (
    set "PRESET=msvc-x64-release"
    set "CONFIG=Release"
    set "BUILD_DIR=build\msvc-x64-release"
)
echo %* | findstr /I "\<clean\>" >nul && (
    set "CLEAN=1"
)
echo %* | findstr /I "\<help\>" >nul && (
    echo Usage: build.bat [release] [clean] [any cmake options...]
    exit /b 0
)

set "RAW_ARGS=%*"
if defined RAW_ARGS (
    set "RAW_ARGS=!RAW_ARGS:release=!"
    set "RAW_ARGS=!RAW_ARGS:clean=!"
    set "CMAKE_ARGS=!RAW_ARGS!"
)

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
cmake --preset "%PRESET%" %CMAKE_ARGS% || exit /b 1

echo [2/3] Building: %CONFIG%
cmake --build --preset "%PRESET%" --config "%CONFIG%" || exit /b 1

echo [3/3] Running tests
ctest --preset "%PRESET%" || exit /b 1

echo Build completed successfully.
endlocal
