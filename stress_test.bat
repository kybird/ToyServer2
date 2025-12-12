@echo off
set CLIENT_EXE=build\Debug\DummyClient.exe

if not exist "%CLIENT_EXE%" (
    echo DummyClient.exe not found! Please build first.
    pause
    exit /b
)

echo Running Stress Test...
echo Client: %CLIENT_EXE%
echo.

:: Run with 1000 Clients for 10 Seconds
"%CLIENT_EXE%" 1000 10

pause
