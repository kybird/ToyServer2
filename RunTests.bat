@echo off
echo Running Unit Tests...
pushd build\Debug
UnitTests.exe
popd
echo.
pause
