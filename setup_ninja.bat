@echo off
REM Ninja 설치 스크립트 - clangd용 compile_commands.json 생성을 위해 필요
REM 이 스크립트는 Windows에서 Ninja 빌드 시스템을 설치합니다.

echo ======================================
echo  Ninja Build System 설치
echo ======================================
echo.

REM 이미 설치되어 있는지 확인
where ninja >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Ninja가 이미 설치되어 있습니다.
    ninja --version
    exit /b 0
)

echo Ninja가 설치되어 있지 않습니다. 설치를 시작합니다...
echo.

REM winget으로 설치 시도
where winget >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo winget을 사용하여 Ninja를 설치합니다...
    winget install Ninja-build.Ninja --accept-source-agreements --accept-package-agreements
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo Ninja 설치 완료!
        echo 터미널을 재시작하여 PATH를 업데이트하세요.
        exit /b 0
    )
)

REM chocolatey로 설치 시도
where choco >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo chocolatey를 사용하여 Ninja를 설치합니다...
    choco install ninja -y
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo Ninja 설치 완료!
        exit /b 0
    )
)

echo.
echo 자동 설치에 실패했습니다.
echo 수동으로 설치해 주세요:
echo   1. https://github.com/ninja-build/ninja/releases 에서 다운로드
echo   2. ninja.exe를 PATH에 추가
echo.
exit /b 1
