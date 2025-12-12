
@echo off
set SERVER_EXE=build\Release\GameServer.exe

if not exist "%SERVER_EXE%" (
    echo DummyClient.exe not found! Please build first.
    pause
    exit /b
)


echo Server: %SERVER_EXE%
echo.

:: Run with 1000 Clients for 10 Seconds
"%SERVER_EXE%"

pause
