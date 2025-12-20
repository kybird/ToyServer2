@echo off
echo Starting VampireSurvivor Server...
pushd build\Debug
:: "start" opens it in a new window so you can also run the client from the same terminal if you want.
:: Remove "start" if you want it to run in this terminal.
VampireSurvivorServer.exe
popd
