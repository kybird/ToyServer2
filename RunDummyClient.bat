@echo off
setlocal

:: Default values
set CLIENT_COUNT=100
set DURATION=60

:: Override defaults if arguments are provided
if not "%~1"=="" set CLIENT_COUNT=%~1
if not "%~2"=="" set DURATION=%~2

echo ========================================
echo  Building DummyClient...
echo ========================================

:: Ensure build directory exists
if not exist build (
    mkdir build
)

:: Run CMake configuration if build folder is freshly created or missing cache
if not exist build\CMakeCache.txt (
    cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR=%cd%/vcpkg_installed
)

:: Build DummyClient only
cmake --build build --target DummyClient --config Debug

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo  Running DummyClient...
echo  Clients: %CLIENT_COUNT%, Duration: %DURATION%s
echo ========================================

:: Execute the built binary
.\build\Debug\DummyClient.exe %CLIENT_COUNT% %DURATION%

endlocal
