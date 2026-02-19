@echo off
REM chcp 65001 >nul
echo Starting VampireSurvivor Server...

cd build\Debug
:: Ensure config files exist in the build directory
if not exist "data" mkdir "data"
copy ..\..\data\*.* data\ >nul
dir data
:: "start" opens it in a new window so you can also run the client from the same terminal if you want.
:: Remove "start" if you want it to run in this terminal.
VampireSurvivorServer.exe
popd
