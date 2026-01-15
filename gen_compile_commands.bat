@echo off
REM compile_commands.json 생성 스크립트
REM Ninja를 사용하여 clangd용 compile_commands.json을 생성합니다.
REM 실제 빌드는 Visual Studio로 계속 진행하면 됩니다.

echo ======================================
echo  compile_commands.json 생성
echo ======================================
echo.

REM Ninja 설치 확인
where ninja >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Ninja가 설치되어 있지 않습니다.
    echo setup_ninja.bat 을 먼저 실행해 주세요.
    exit /b 1
)

REM Visual Studio 환경 설정 (MSVC 컴파일러 사용시 필요)
if not defined VCINSTALLDIR (
    echo Visual Studio 환경을 로드합니다...
    REM VS2022 Pro/Enterprise
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
        echo Visual Studio를 찾을 수 없습니다.
        exit /b 1
    )
)

REM build_clangd 폴더 생성 및 CMake 실행
if not exist build_clangd mkdir build_clangd

echo Ninja로 CMake 프로젝트 구성 중...
cmake -B build_clangd -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
if %ERRORLEVEL% NEQ 0 (
    echo CMake 구성 실패!
    exit /b %ERRORLEVEL%
)

REM compile_commands.json을 프로젝트 루트로 복사
if exist build_clangd\compile_commands.json (
    copy /Y build_clangd\compile_commands.json compile_commands.json
    echo.
    echo ======================================
    echo  compile_commands.json 생성 완료!
    echo ======================================
    echo 파일 위치: %CD%\compile_commands.json
    echo clangd를 재시작하세요: Ctrl+Shift+P -^> clangd: Restart language server
) else (
    echo compile_commands.json 생성 실패!
    exit /b 1
)
