@echo off
setlocal

:: 1. Setup vcpkg path
if defined VCPKG_ROOT (
    set "VCPKG_CMAKE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
) else (
    if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" (
        set "VCPKG_CMAKE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
    ) else (
        echo [ERROR] VCPKG_ROOT is not defined and C:\vcpkg was not found.
        exit /b 1
    )
)

echo [INFO] vcpkg toolchain: %VCPKG_CMAKE%

:: 2. Clean build folder
if exist build (
    echo [INFO] Removing existing build folder...
    rmdir /s /q build
)

:: 3. CMake Configure
echo [INFO] Configuring CMake...
cmake -B build -S . ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_CMAKE%" ^
    -DVCPKG_INSTALLED_DIR="%~dp0vcpkg_installed" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake Configuration Failed
    exit /b %ERRORLEVEL%
)

:: 4. CMake Build
echo [INFO] Starting build...
cmake --build build --config Debug

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build Failed
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Rebuild completed successfully.
