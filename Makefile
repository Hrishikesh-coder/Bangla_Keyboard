# Shobdomala build file.
#
# On Windows (MinGW) `make` builds the hook application and the tests.
# On Linux/macOS only the core engine and its tests build: the native layer is Win32-only.
# That matters because it lets the transliteration engine be tested in CI, and on a laptop,
# without a Windows machine anywhere in the loop.

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude -Ithird_party

CORE_SRCS = src/core/CandidateResolver.cpp \
            src/core/ContextAnalyzer.cpp \
            src/core/ExceptionDictionary.cpp \
            src/core/FixedLayoutEngine.cpp \
            src/core/InputBuffer.cpp \
            src/core/PhoneticEngine.cpp \
            src/core/SpecialCharPicker.cpp \
            src/core/SymbolTable.cpp \
            src/core/TokenContext.cpp \
            src/core/Tokenizer.cpp \
            src/core/TokenTrie.cpp \
            src/core/UnicodeComposer.cpp

NATIVE_SRCS = src/native/InputInjector.cpp \
              src/native/KeyboardHook.cpp \
              src/native/KeyboardState.cpp

TEST_SRCS = tests/test_main.cpp $(CORE_SRCS)
APP_SRCS  = src/main.cpp $(CORE_SRCS) $(NATIVE_SRCS)

CONFIGS = config/phonetic_rules.json config/exceptions.json config/layout_probhat.json

ifeq ($(OS),Windows_NT)
    EXE      := .exe
    LDFLAGS  := -luser32
    MKDIR     = @if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
    COPYCFG   = @for %%f in ($(subst /,\,$(CONFIGS))) do @copy /Y "%%f" "bin\config\" >nul
    RMDIR     = @if exist bin rmdir /S /Q bin
    ALL_TARGETS = bin/shobdomala$(EXE) bin/shobdomala_tests$(EXE) copy_config
else
    EXE      :=
    LDFLAGS  :=
    MKDIR     = @mkdir -p $1
    COPYCFG   = @cp $(CONFIGS) bin/config/
    RMDIR     = @rm -rf bin
    # The Win32 hook cannot build here; the engine and its tests still can.
    ALL_TARGETS = bin/shobdomala_tests$(EXE) copy_config
endif

all: $(ALL_TARGETS)

bin/shobdomala$(EXE): $(APP_SRCS)
	$(call MKDIR,bin)
	$(CXX) $(CXXFLAGS) -o $@ $(APP_SRCS) $(LDFLAGS)

bin/shobdomala_tests$(EXE): $(TEST_SRCS)
	$(call MKDIR,bin)
	$(CXX) $(CXXFLAGS) -Itests -o $@ $(TEST_SRCS)

copy_config:
	$(call MKDIR,bin/config)
	$(COPYCFG)

test: bin/shobdomala_tests$(EXE) copy_config
	cd bin && ./shobdomala_tests$(EXE)

clean:
	$(RMDIR)

.PHONY: all clean test copy_config
