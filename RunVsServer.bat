@echo off
echo Starting VampireSurvivor Server...

:: Ensure config files exist in the build directory
if not exist "build\Debug\simple_game_config.json" copy "simple_game_config.json" "build\Debug\"
if not exist "build\Debug\MonsterData.json" copy "MonsterData.json" "build\Debug\"
if not exist "build\Debug\WaveData.json" copy "WaveData.json" "build\Debug\"

pushd build\Debug
:: "start" opens it in a new window so you can also run the client from the same terminal if you want.
:: Remove "start" if you want it to run in this terminal.
VampireSurvivorServer.exe
popd
