CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude -Ithird_party
LDFLAGS = -luser32

CORE_SRCS = src/core/CandidateResolver.cpp \
            src/core/InputBuffer.cpp \
            src/core/PhoneticEngine.cpp \
            src/core/SpecialCharPicker.cpp \
            src/core/SymbolTable.cpp \
            src/core/Tokenizer.cpp \
            src/core/UnicodeComposer.cpp

NATIVE_SRCS = src/native/InputInjector.cpp \
              src/native/KeyboardHook.cpp \
              src/native/KeyboardState.cpp

APP_SRCS = src/main.cpp $(CORE_SRCS) $(NATIVE_SRCS)
TEST_SRCS = tests/test_main.cpp $(CORE_SRCS)

all: bin/shobdomala.exe bin/shobdomala_tests.exe copy_config

bin/shobdomala.exe: $(APP_SRCS)
	@if not exist bin mkdir bin
	$(CXX) $(CXXFLAGS) -o $@ $(APP_SRCS) $(LDFLAGS)

bin/shobdomala_tests.exe: $(TEST_SRCS)
	@if not exist bin mkdir bin
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_SRCS) $(LDFLAGS)

copy_config:
	@if not exist bin\config mkdir bin\config
	@copy /Y config\phonetic_rules.json bin\config\phonetic_rules.json >nul

test: bin/shobdomala_tests.exe copy_config
	bin\shobdomala_tests.exe

clean:
	@if exist bin rmdir /S /Q bin

.PHONY: all clean test copy_config
