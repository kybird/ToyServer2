@echo off
setlocal

:: Switch to script directory
cd /d "%~dp0"

:: Paths (Now relative to current dir)
set PROTOC=..\..\..\..\vcpkg_installed\x64-windows\tools\protobuf\protoc.exe
set PROTO_FILE=game.proto
set CPP_OUT_DIR=.
set CSHARP_OUT_DIR=..\ToySurvival\Assets\_Project\Scripts\Protocol
set CSHARP_OUT_DIR_INTERNAL=..\Client\Assets\Scripts\Protocol

:: Check Protoc
if not exist "%PROTOC%" (
    echo [ERROR] protoc.exe not found at %PROTOC%
    echo Please build the server project first to restore vcpkg packages.
    pause
    exit /b 1
)

:: Generate C++ (Optional - CMake handles this, but useful for manual regeneration)
echo [INFO] Generating C++ from %PROTO_FILE%...
"%PROTOC%" --proto_path=. --cpp_out="%CPP_OUT_DIR%" "%PROTO_FILE%"

if %errorlevel% neq 0 (
    echo [ERROR] C++ Protobuf generation failed.
    pause
    exit /b 1
)

echo [SUCCESS] C++ files generated in %CPP_OUT_DIR%

:: Generate C# (for Unity Client)
if not exist "%CSHARP_OUT_DIR%" mkdir "%CSHARP_OUT_DIR%"
echo [INFO] Generating C# from %PROTO_FILE%...
"%PROTOC%" --proto_path=. --csharp_out="%CSHARP_OUT_DIR%" "%PROTO_FILE%"

if %errorlevel% neq 0 (
    echo [ERROR] C# Protobuf generation failed.
    pause
    exit /b 1
)

echo [SUCCESS] C# files generated in %CSHARP_OUT_DIR%

:: Generate C# (for Internal Client)
if not exist "%CSHARP_OUT_DIR_INTERNAL%" mkdir "%CSHARP_OUT_DIR_INTERNAL%"
echo [INFO] Generating C# for internal client...
"%PROTOC%" --proto_path=. --csharp_out="%CSHARP_OUT_DIR_INTERNAL%" "%PROTO_FILE%"

if %errorlevel% neq 0 (
    echo [ERROR] C# Internal Protobuf generation failed.
    pause
    exit /b 1
)

:: Rename Game.cs to Protocol.cs (User Request)
move /Y "%CSHARP_OUT_DIR_INTERNAL%\Game.cs" "%CSHARP_OUT_DIR_INTERNAL%\Protocol.cs"

echo [SUCCESS] C# files generated in %CSHARP_OUT_DIR_INTERNAL%

:: Generate GamePackets.h (Custom Wrapper)
echo [INFO] Generating GamePackets.h...
python generate_packets.py

if %errorlevel% neq 0 (
    echo [WARNING] GamePackets.h generation failed. Ensure Python is installed.
)

pause



