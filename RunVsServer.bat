@echo off
echo Starting VampireSurvivor Server...

:: Ensure config files exist in the build directory
:: Ensure config files exist in the build directory
if not exist "build\Debug\data" mkdir "build\Debug\data"
xcopy /Y /S "data\*.*" "build\Debug\data\"

pushd build\Debug
:: "start" opens it in a new window so you can also run the client from the same terminal if you want.
:: Remove "start" if you want it to run in this terminal.
VampireSurvivorServer.exe
popd
