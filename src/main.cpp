#include "core/PhoneticEngine.h"
#include "core/SpecialCharPicker.h"
#include "native/KeyboardHook.h"
#include "native/KeyboardState.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <memory>

static KeyboardHook g_hook;

// Clean console Ctrl+C handler
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT || dwCtrlType == CTRL_CLOSE_EVENT) {
        std::cout << "\n[MAIN] Shutdown signal received. Cleaning up hook..." << std::endl;
        g_hook.uninstall();
        g_hook.stopMessageLoop();
        return TRUE;
    }
    return FALSE;
}

// Interactive CLI test/demo mode (runs offline without installing any Windows hook)
static int runOfflineDemo(PhoneticEngine& engine) {
    std::cout << "========================================================\n"
              << "       Shobdomala - Offline Transliteration Demo\n"
              << "       (No Windows hooks installed in this mode)\n"
              << "========================================================\n"
              << "Type Roman words and press Enter to see Bengali output.\n"
              << "Commands:\n"
              << "  :q             - Quit demo\n"
              << "  :tokens <word> - Show longest-match-first tokens\n"
              << "  :rules         - Show loaded symbol count\n"
              << "--------------------------------------------------------\n" << std::endl;

    std::string line;
    while (true) {
        std::cout << "shobdomala> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line.empty()) {
            continue;
        }

        if (line == ":q" || line == ":quit" || line == "exit") {
            break;
        }

        if (line == ":rules") {
            std::cout << "Registered rules count: " << engine.getSymbolTable().size() << std::endl;
            continue;
        }

        if (line.rfind(":tokens ", 0) == 0) {
            std::string word = line.substr(8);
            Tokenizer tokenizer(engine.getSymbolTable());
            auto tokens = tokenizer.tokenize(word);
            std::cout << "Tokens for \"" << word << "\": [ ";
            for (size_t i = 0; i < tokens.size(); ++i) {
                std::cout << "\"" << tokens[i] << "\"" << (i + 1 < tokens.size() ? ", " : " ");
            }
            std::cout << "]" << std::endl;
            continue;
        }

        std::string bengali = engine.transliterate(line);
        std::cout << "Result: " << bengali << std::endl;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    // Set console output code page to UTF-8 so Bengali characters display properly in cmd/PowerShell
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Locate phonetic_rules.json (checks current dir, config/ dir, and ../config/)
    std::string rulesPath = "config/phonetic_rules.json";
    PhoneticEngine engine;

    if (!engine.loadRules(rulesPath)) {
        // Fallback search locations
        if (!engine.loadRules("../config/phonetic_rules.json")) {
            engine.loadRules("phonetic_rules.json");
        }
    }

    if (engine.getSymbolTable().size() == 0) {
        std::cerr << "[WARNING] No phonetic rules loaded from config/phonetic_rules.json!\n"
                  << "Make sure config/phonetic_rules.json is present in the working directory."
                  << std::endl;
    } else {
        std::cout << "[INIT] Loaded " << engine.getSymbolTable().size()
                  << " phonetic rules successfully." << std::endl;
    }

    // Check for offline test/demo mode
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test" || arg == "--demo" || arg == "-t") {
            return runOfflineDemo(engine);
        }
    }

    // Register console Ctrl+C handler for graceful hook cleanup
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Print welcome banner and shortcuts summary
    std::cout << "========================================================\n"
              << "   Shobdomala: Windows Bengali Phonetic Input Prototype\n"
              << "========================================================\n"
              << "Keyboard Shortcuts:\n"
              << "  [Ctrl + Shift + B]     : Toggle ENGLISH <-> BENGALI mode\n"
              << "  [Ctrl + Shift + Space] : Cycle ambiguous candidate\n"
              << "  [Ctrl + Shift + D]     : Special characters (ৎ ং ঃ ঁ ঞ)\n"
              << "  [Ctrl + C]             : Exit Shobdomala cleanly\n"
              << "--------------------------------------------------------\n"
              << "Initial Mode: " << KeyboardState::getInstance().getModeString() << "\n"
              << "To start typing Bengali in any window, press Ctrl+Shift+B.\n"
              << "========================================================\n" << std::endl;

    // Install the low-level keyboard hook
    if (!g_hook.install(&engine)) {
        std::cerr << "[ERROR] Failed to install keyboard hook. Terminating." << std::endl;
        return 1;
    }

    // Run the required Win32 message pump
    g_hook.runMessageLoop();

    // Clean uninstall upon exit
    g_hook.uninstall();
    std::cout << "[MAIN] Program terminated cleanly." << std::endl;
    return 0;
}
