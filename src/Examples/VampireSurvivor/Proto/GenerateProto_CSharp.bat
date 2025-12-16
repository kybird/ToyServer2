@echo off
setlocal

:: Switch to script directory
cd /d "%~dp0"

:: Paths (Now relative to current dir)
set PROTOC=..\..\..\..\vcpkg_installed\x64-windows\tools\protobuf\protoc.exe
set PROTO_FILE=Protocol.proto
set OUT_DIR=..\Client\Assets\Scripts\Protocol

:: Check Protoc
if not exist "%PROTOC%" (
    echo [ERROR] protoc.exe not found at %PROTOC%
    echo Please build the server project first to restore vcpkg packages.
    pause
    exit /b 1
)

:: Create Output Dir
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

:: Generate C#
echo [INFO] Generating C# from %PROTO_FILE%...
"%PROTOC%" --proto_path=. --csharp_out="%OUT_DIR%" "%PROTO_FILE%"

if %errorlevel% neq 0 (
    echo [ERROR] Protobuf generation failed.
    pause
    exit /b 1
)

echo [SUCCESS] C# files generated in %OUT_DIR%
pause
