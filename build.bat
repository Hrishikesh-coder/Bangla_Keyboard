@echo off
setlocal

echo [BUILD] Building Shobdomala using g++...

if not exist bin mkdir bin
if not exist bin\config mkdir bin\config
copy /Y config\phonetic_rules.json bin\config\phonetic_rules.json >nul

set CXX_FLAGS=-std=c++17 -O2 -Iinclude -Ithird_party -luser32
set CORE_SRCS=src\core\CandidateResolver.cpp src\core\InputBuffer.cpp src\core\PhoneticEngine.cpp src\core\SpecialCharPicker.cpp src\core\SymbolTable.cpp src\core\Tokenizer.cpp src\core\UnicodeComposer.cpp
set NATIVE_SRCS=src\native\InputInjector.cpp src\native\KeyboardHook.cpp src\native\KeyboardState.cpp

echo [BUILD] Compiling shobdomala.exe...
g++ %CXX_FLAGS% -o bin\shobdomala.exe src\main.cpp %CORE_SRCS% %NATIVE_SRCS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile shobdomala.exe
    exit /b %ERRORLEVEL%
)

echo [BUILD] Compiling shobdomala_tests.exe...
g++ %CXX_FLAGS% -o bin\shobdomala_tests.exe tests\test_main.cpp %CORE_SRCS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile shobdomala_tests.exe
    exit /b %ERRORLEVEL%
)

echo [BUILD] Build completed successfully in .\bin\
echo [RUN] Running unit tests...
bin\shobdomala_tests.exe
