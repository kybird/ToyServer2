@echo off
echo Building VampireSurvivorServer only...
cmake --build build --target VampireSurvivorServer --config Debug --parallel
if %ERRORLEVEL% NEQ 0 (
    echo Build FAILED!
    exit /b %ERRORLEVEL%
)
echo Build SUCCESS!
