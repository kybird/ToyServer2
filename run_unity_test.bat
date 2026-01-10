@echo off
set UNITY_PATH="C:\Program Files\Unity\Hub\Editor\6000.3.2f1\Editor\Unity.exe"
set PROJECT_PATH=%~dp0src\Examples\VampireSurvivor\ToySurvival
set LOG_FILE=%~dp0test_results.xml

echo Running Unity Tests...
echo Unity Path: %UNITY_PATH%
echo Project Path: %PROJECT_PATH%

%UNITY_PATH% -runTests -batchmode -projectPath "%PROJECT_PATH%" -testResults "%LOG_FILE%" -testPlatform PlayMode -logFile "%~dp0build.log"

if %ERRORLEVEL% EQU 0 (
    echo Tests Passed! Results saved to %LOG_FILE%
) else (
    echo Tests Failed! Check %LOG_FILE% for details.
)
