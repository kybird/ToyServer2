@echo off
setlocal

:: User provided path for clang-tidy
set "CLANG_TIDY=C:\Users\kybir\.vscode\extensions\ms-vscode.cpptools-1.29.3-win32-x64\LLVM\bin\clang-tidy.exe"

:: Check if clang-tidy exists
if not exist "%CLANG_TIDY%" (
    echo [ERROR] clang-tidy.exe not found at:
    echo %CLANG_TIDY%
    echo Please verify the path.
    exit /b 1
)

:: Check if compile_commands.json exists
if not exist "compile_commands.json" (
    echo [WARNING] compile_commands.json not found in current directory.
    echo Clang-tidy might not report accurate results without compilation database.
    echo Ensure you ran: cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ...
)

echo [INFO] Using clang-tidy from: %CLANG_TIDY%
echo [INFO] Starting linting process for .cpp files in src/...

:: Iterate recursively over all .cpp files in src directory
for /r "src" %%f in (*.cpp) do (
    echo [LINT] %%f
    "%CLANG_TIDY%" -p . --quiet --extra-arg=/Y- "%%f"
)

echo [INFO] Linting complete.
endlocal
