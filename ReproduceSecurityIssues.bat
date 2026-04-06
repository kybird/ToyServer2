@echo off
setlocal
echo ========================================================
echo  ToyServer2 Security Review Reproduction Test Suite
echo ========================================================
echo.

echo [Step 1] Building UnitTests Target...
cmake --build build --config Debug --target UnitTests --parallel
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build FAILED! Please check the compile errors.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [Step 2] Running Security Reproduction Tests...
echo.

pushd build\Debug
UnitTests.exe --gtest_filter=SecurityReproduction.*
popd

echo.
echo ========================================================
echo  Reproduction attempt complete.
echo  Check the output above for race condition/ABA detections.
echo ========================================================
echo.
pause
