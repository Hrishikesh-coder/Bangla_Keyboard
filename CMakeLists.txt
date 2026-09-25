cmake_minimum_required(VERSION 3.15)
project(Shobdomala VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Core engine sources
set(CORE_SOURCES
    src/core/CandidateResolver.cpp
    src/core/InputBuffer.cpp
    src/core/PhoneticEngine.cpp
    src/core/SpecialCharPicker.cpp
    src/core/SymbolTable.cpp
    src/core/Tokenizer.cpp
    src/core/UnicodeComposer.cpp
)

# Native Win32 hook and injection sources
set(NATIVE_SOURCES
    src/native/InputInjector.cpp
    src/native/KeyboardHook.cpp
    src/native/KeyboardState.cpp
)

# Main executable
add_executable(shobdomala
    src/main.cpp
    ${CORE_SOURCES}
    ${NATIVE_SOURCES}
)

# Link User32 for Windows hooks and SendInput
target_link_libraries(shobdomala PRIVATE user32)

# Copy config directory to build output directory
add_custom_command(TARGET shobdomala POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:shobdomala>/config
)

# Unit test executable (does not require Windows hook)
add_executable(shobdomala_tests
    tests/test_main.cpp
    ${CORE_SOURCES}
)

target_link_libraries(shobdomala_tests PRIVATE user32)

add_custom_command(TARGET shobdomala_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:shobdomala_tests>/config
)
