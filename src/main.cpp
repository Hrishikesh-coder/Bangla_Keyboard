#include "core/PhoneticEngine.h"
#include "core/FixedLayoutEngine.h"
#include "core/SpecialCharPicker.h"
#include "native/KeyboardHook.h"
#include "native/KeyboardState.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
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

/// Finds a config file whether we were launched from the repo root or from bin/.
static std::string findConfig(const std::string& name) {
    const std::string candidates[] = {
        "config/" + name,
        "../config/" + name,
        name
    };
    for (const auto& path : candidates) {
        std::ifstream probe(path);
        if (probe.is_open()) {
            return path;
        }
    }
    return "config/" + name;
}

// Interactive CLI test/demo mode (runs offline without installing any Windows hook)
static int runOfflineDemo(PhoneticEngine& engine, FixedLayoutEngine& layout) {
    std::cout << "========================================================\n"
              << "       Shobdomala - Offline Transliteration Demo\n"
              << "       (No Windows hooks installed in this mode)\n"
              << "========================================================\n"
              << "Type Roman words and press Enter to see Bengali output.\n"
              << "Commands:\n"
              << "  :q               - Quit demo\n"
              << "  :tokens <word>   - Show longest-match-first tokens\n"
              << "  :explain <word>  - Show tokens, classes, contexts and chosen candidates\n"
              << "  :layout <keys>   - Map keys through the fixed layout instead\n"
              << "  :rules           - Show loaded rule and exception counts\n"
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
            std::cout << "Registered tokens:      " << engine.getSymbolTable().size() << "\n"
                      << "With contextual rules:  " << engine.getSymbolTable().contextualRuleCount() << "\n"
                      << "Exception overrides:    " << engine.getExceptions().size() << "\n"
                      << "Fixed layout:           " << layout.layoutName()
                      << " (" << layout.size() << " keys)" << std::endl;
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

        if (line.rfind(":explain ", 0) == 0) {
            std::string word = line.substr(9);
            std::cout << "Derivation of \"" << word << "\":\n" << engine.explain(word);
            continue;
        }

        if (line.rfind(":layout ", 0) == 0) {
            std::string keys = line.substr(8);
            std::cout << "Fixed layout (" << layout.layoutName() << "): "
                      << layout.mapText(keys) << std::endl;
            continue;
        }

        // Whole-line transliteration so that the exception dictionary, which works on
        // whole words, gets a chance to fire on each word of a sentence.
        std::cout << "Result: " << engine.transliterateText(line) << std::endl;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    // Set console output code page to UTF-8 so Bengali characters display properly in cmd/PowerShell
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    PhoneticEngine engine;
    FixedLayoutEngine layout;

    if (!engine.loadRules(findConfig("phonetic_rules.json"))) {
        std::cerr << "[WARNING] Could not load config/phonetic_rules.json!" << std::endl;
    }
    if (!engine.loadExceptions(findConfig("exceptions.json"))) {
        std::cout << "[INIT] No exceptions.json found; continuing without word overrides." << std::endl;
    }
    if (!layout.loadFromFile(findConfig("layout_probhat.json"))) {
        std::cout << "[INIT] No fixed layout loaded; fixed-layout mode will be unavailable." << std::endl;
    }

    if (engine.getSymbolTable().size() == 0) {
        std::cerr << "[WARNING] No phonetic rules loaded from config/phonetic_rules.json!\n"
                  << "Make sure config/phonetic_rules.json is present in the working directory."
                  << std::endl;
    } else {
        std::cout << "[INIT] Loaded " << engine.getSymbolTable().size() << " phonetic rules ("
                  << engine.getSymbolTable().contextualRuleCount() << " context-sensitive), "
                  << engine.getExceptions().size() << " exception overrides, "
                  << layout.size() << " fixed-layout keys." << std::endl;
    }

    // Check for offline test/demo mode
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test" || arg == "--demo" || arg == "-t") {
            return runOfflineDemo(engine, layout);
        }
        if (arg == "--no-preview") {
            KeyboardState::getInstance().setLivePreview(false);
        }
    }

    // Register console Ctrl+C handler for graceful hook cleanup
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Print welcome banner and shortcuts summary
    std::cout << "========================================================\n"
              << "   Shobdomala: Windows Bengali Phonetic Input Prototype\n"
              << "========================================================\n"
              << "Keyboard Shortcuts:\n"
              << "  [Ctrl + Shift + B]     : Toggle ENGLISH <-> BENGALI (phonetic)\n"
              << "  [Ctrl + Shift + L]     : Cycle English / phonetic / fixed layout\n"
              << "  [Ctrl + Shift + Space] : Cycle ambiguous candidate\n"
              << "  [Ctrl + Shift + D]     : Special characters (Khanda Ta, Anusvara, ...)\n"
              << "  [Ctrl + Shift + P]     : Toggle live in-place preview\n"
              << "  [Ctrl + C]             : Exit Shobdomala cleanly\n"
              << "--------------------------------------------------------\n"
              << "Initial Mode:  " << KeyboardState::getInstance().getModeString() << "\n"
              << "Live preview:  "
              << (KeyboardState::getInstance().isLivePreviewEnabled() ? "ON" : "OFF")
              << "  (start with --no-preview for flush-on-space behaviour)\n"
              << "To start typing Bengali in any window, press Ctrl+Shift+B.\n"
              << "========================================================\n" << std::endl;

    // Install the low-level keyboard hook
    if (!g_hook.install(&engine, &layout)) {
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
