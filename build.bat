@echo off
setlocal

echo [BUILD] Building Shobdomala using g++...

if not exist bin mkdir bin
if not exist bin\config mkdir bin\config
copy /Y config\phonetic_rules.json bin\config\phonetic_rules.json >nul
copy /Y config\exceptions.json bin\config\exceptions.json >nul
copy /Y config\layout_probhat.json bin\config\layout_probhat.json >nul
copy /Y config\words_bangla.json bin\config\words_bangla.json >nul

set CXX_FLAGS=-std=c++17 -Wall -Wextra -O2 -Iinclude -Ithird_party
set CORE_SRCS=src\core\CandidateResolver.cpp src\core\ContextAnalyzer.cpp src\core\ExceptionDictionary.cpp src\core\FixedLayoutEngine.cpp src\core\InputBuffer.cpp src\core\PhoneticEngine.cpp src\core\SpecialCharPicker.cpp src\core\SymbolTable.cpp src\core\TokenContext.cpp src\core\Tokenizer.cpp src\core\TokenTrie.cpp src\core\UnicodeComposer.cpp src\core\WordDictionary.cpp
set NATIVE_SRCS=src\native\ConsoleHost.cpp src\native\Settings.cpp src\native\InputInjector.cpp src\native\KeyboardHook.cpp src\native\KeyboardState.cpp
set UI_SRCS=src\ui\UiTheme.cpp src\ui\CandidateWindow.cpp src\ui\OnScreenKeyboard.cpp src\ui\TrayIcon.cpp

echo [BUILD] Compiling shobdomala.exe...
g++ %CXX_FLAGS% -o bin\shobdomala.exe src\main.cpp %CORE_SRCS% %NATIVE_SRCS% %UI_SRCS% -mwindows -luser32 -lgdi32 -lshell32
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile shobdomala.exe
    exit /b %ERRORLEVEL%
)

echo [BUILD] Compiling shobdomala_tests.exe...
g++ %CXX_FLAGS% -Itests -o bin\shobdomala_tests.exe tests\test_main.cpp %CORE_SRCS%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile shobdomala_tests.exe
    exit /b %ERRORLEVEL%
)

echo [BUILD] Build completed successfully in .\bin\
echo [RUN] Running unit tests...
pushd bin
shobdomala_tests.exe
set TEST_RESULT=%ERRORLEVEL%
popd
exit /b %TEST_RESULT%
