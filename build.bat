@echo off
echo Building all targets...
cmake --build build --config Debug --parallel
if %ERRORLEVEL% NEQ 0 (
    echo Build FAILED!
    exit /b %ERRORLEVEL%
)
echo Build SUCCESS!
