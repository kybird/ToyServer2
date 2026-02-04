@echo off
setlocal

:: 1. vcpkg 경로 설정 (VCPKG_ROOT 우선, 그 다음 C:\vcpkg)
if defined VCPKG_ROOT (
    set "VCPKG_CMAKE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
) else (
    if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" (
        set "VCPKG_CMAKE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
    ) else (
        echo [ERROR] VCPKG_ROOT가 설정되지 않았고 C:\vcpkg를 찾을 수 없습니다.
        exit /b 1
    )
)

echo [INFO] vcpkg toolchain: %VCPKG_CMAKE%

:: 2. 기존 빌드 폴더 삭제 (Clean Build)
if exist build (
    echo [INFO] 기존 build 폴더를 삭제합니다...
    rmdir /s /q build
)

:: 3. CMake 구성 (Configure)
:: -DVCPKG_INSTALLED_DIR를 현재 디렉토리의 vcpkg_installed로 고정
:: -DVCPKG_TARGET_TRIPLET를 x64-windows로 설정
cmake -B build -S . ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_CMAKE%" ^
    -DVCPKG_INSTALLED_DIR="%~dp0vcpkg_installed" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake 구성 실패
    exit /b %ERRORLEVEL%
)

:: 4. CMake 빌드
echo [INFO] 빌드를 시작합니다...
cmake --build build --config Debug

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 빌드 실패
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] 모든 과정이 성공적으로 완료되었습니다.
