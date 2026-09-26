cmake_minimum_required(VERSION 3.15)
project(Shobdomala VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(NOMINMAX)
endif()

# Header include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Core engine sources
set(CORE_SOURCES
    src/core/BanglaText.cpp
    src/core/Morphology.cpp
    src/core/NgramModel.cpp
    src/core/UserDictionary.cpp
    src/core/CandidateResolver.cpp
    src/core/ContextAnalyzer.cpp
    src/core/ExceptionDictionary.cpp
    src/core/FixedLayoutEngine.cpp
    src/core/InputBuffer.cpp
    src/core/PhoneticEngine.cpp
    src/core/SpecialCharPicker.cpp
    src/core/SuggestionPolicy.cpp
    src/core/SymbolTable.cpp
    src/core/TokenContext.cpp
    src/core/Tokenizer.cpp
    src/core/TokenTrie.cpp
    src/core/UnicodeComposer.cpp
    src/core/WordDictionary.cpp
)

# Native Win32 hook and injection sources
set(NATIVE_SOURCES
    src/native/ConsoleHost.cpp
    src/native/Settings.cpp
    src/native/InputInjector.cpp
    src/native/KeyboardHook.cpp
    src/native/KeyboardState.cpp
)

# Win32 GUI layer: tray icon, composition overlay, on-screen keyboard.
set(UI_SOURCES
    src/ui/UiTheme.cpp
    src/ui/CandidateWindow.cpp
    src/ui/OnScreenKeyboard.cpp
    src/ui/TrayIcon.cpp
)

# Main executable. The native layer is Win32-only, so it is only defined on Windows;
# this keeps the engine and its tests buildable on Linux/macOS for CI.
if(WIN32)
    add_executable(shobdomala WIN32
        src/main.cpp
        ${CORE_SOURCES}
        ${NATIVE_SOURCES}
        ${UI_SOURCES}
    )

    # user32: hooks and SendInput. gdi32: custom-drawn UI. shell32: tray icon.
    target_link_libraries(shobdomala PRIVATE user32 gdi32 shell32)

    if (MSVC)
        set_target_properties(shobdomala PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    endif()

    # Copy config directory to build output directory
    add_custom_command(TARGET shobdomala POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/config
            $<TARGET_FILE_DIR:shobdomala>/config
    )
endif()

# Unit test executable: pure core engine, no Windows dependency.
add_executable(shobdomala_tests
    tests/test_main.cpp
    ${CORE_SOURCES}
)

target_include_directories(shobdomala_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

add_custom_command(TARGET shobdomala_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:shobdomala_tests>/config
)

enable_testing()
add_test(NAME engine_tests
         COMMAND shobdomala_tests
         WORKING_DIRECTORY $<TARGET_FILE_DIR:shobdomala_tests>)
