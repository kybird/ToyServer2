@echo off
setlocal enabledelayedexpansion

:: 1. Configuration
set BUILD_DIR=build\Release
set LOG_DIR=logs\StressTest
set SERVER_EXE=%BUILD_DIR%\VampireSurvivorServer.exe
:: Actual path from build output:
set CLIENT_EXE=build\src\Tools\StressTest\Release\StressTestClient.exe
set TIMESTAMP=%date:~0,4%-%date:~5,2%-%date:~8,2%_%time:~0,2%%time:~3,2%
set TIMESTAMP=%TIMESTAMP: =0%

echo ========================================
echo  Stress Test Suite v1.0
echo ========================================

:: 2. Check Build
if not exist "%SERVER_EXE%" (
    echo [ERROR] Server executable not found at %SERVER_EXE%
    echo Please build the project in Release mode first.
    pause
    exit /b 1
)
if not exist "%CLIENT_EXE%" (
    echo [ERROR] Client executable not found at %CLIENT_EXE%
    echo Please build the project in Release mode first.
    pause
    exit /b 1
)

:: 3. Prepare Logs
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
set LOG_PERF=%LOG_DIR%\%TIMESTAMP%_perf.csv
set LOG_CLIENT=%LOG_DIR%\%TIMESTAMP%_client.log

:: 4. Swap Wave Data (Disable Monsters)
echo [1/4] Swapping WaveData.json (Heavy Monsters 10x)...
if exist "data\WaveData.json" (
    copy /Y "data\WaveData.json" "data\WaveData.json.bak" >nul
)
copy /Y "tools\StressTest\heavy_wave.json" "data\WaveData.json" >nul

:: 5. Start Server
echo [2/4] Starting Game Server...
:: Check if running
tasklist /FI "IMAGENAME eq VampireSurvivorServer.exe" 2>NUL | find /I /N "VampireSurvivorServer.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo [WARNING] Server is already running! WaveData swap might not take effect if it's hot-loaded.
    echo Please restart the server for empty waves to apply.
) else (
    start "GameServer" powershell -NoProfile -Command "chcp 65001 >$null; & '%SERVER_EXE%' | Tee-Object -FilePath '%LOG_DIR%\server_output.log'"
    timeout /t 10 >nul
)

:: 6. Start Monitor
echo [3/4] Starting Performance Monitor...
start /b powershell -ExecutionPolicy Bypass -File tools\Performance\monitor_server.ps1 -ProcessName VampireSurvivorServer -OutFile "%LOG_PERF%"

:: 7. Start Stress Client
echo [4/4] Starting Stress Test Client (8,000 CCU)...
echo Logs will be saved to %LOG_CLIENT%
"%CLIENT_EXE%" 8000 120 > "%LOG_CLIENT%" 2>&1

:: 8. Finish & Restore
echo.
echo ========================================
echo  Test Finished!
echo ========================================
echo Restoring WaveData.json...
if exist "data\WaveData.json.bak" (
    copy /Y "data\WaveData.json.bak" "data\WaveData.json" >nul
    del "data\WaveData.json.bak"
)
echo Results:
echo  - Perf Log: %LOG_PERF%
echo  - Client Log: %LOG_CLIENT%
echo.
echo Note: The 'GameServer' and 'PowerShell Monitor' are still running.
echo Close them manually when done analyzing.
pause
