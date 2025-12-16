@echo off
echo ========================================
echo Building Tests with Google Test (VERIFICATION)
echo ========================================

if not exist build_gtest mkdir build_gtest
cd build_gtest

cmake .. -A x64
if %errorlevel% neq 0 exit /b %errorlevel%

cmake --build . --config Debug
if %errorlevel% neq 0 exit /b %errorlevel%

echo.
echo ========================================
echo Running Tests
echo ========================================
ctest -C Debug --output-on-failure
if %errorlevel% neq 0 exit /b %errorlevel%

cd ..
exit /b 0
