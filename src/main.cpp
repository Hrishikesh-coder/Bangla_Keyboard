#include "core/PhoneticEngine.h"
#include "core/FixedLayoutEngine.h"
#include "core/WordDictionary.h"
#include "core/SpecialCharPicker.h"
#include "native/KeyboardHook.h"
#include "native/KeyboardState.h"
#include "native/InputInjector.h"
#include "native/ConsoleHost.h"
#include "native/Settings.h"
#include "ui/CandidateWindow.h"
#include "ui/OnScreenKeyboard.h"
#include "ui/TrayIcon.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
#include <iostream>
#include <string>
#include <memory>

static KeyboardHook g_hook;
static ConsoleHost g_console;
static Settings g_settings;
static std::string g_settingsPath;
static TrayIcon g_tray;
static CandidateWindow g_candidateWindow;
static OnScreenKeyboard g_onScreenKeyboard;

/// Shortcut reference, shown from the tray menu.
static void showHelp() {
    MessageBoxW(nullptr,
        L"SHORTCUTS\n"
        L"  Ctrl + Shift + B\t\tEnglish \u2194 Bengali (phonetic)\n"
        L"  Ctrl + Shift + L\t\tCycle English / phonetic / fixed layout\n"
        L"  Ctrl + Shift + Space\tCycle the highlighted candidate\n"
        L"  Alt + 1..9\t\tDirectly select candidate chip 1..9\n"
        L"  Ctrl + 1..9\t\tDirectly select word prediction 1..9\n"
        L"  Ctrl + Shift + D\t\tInsert \u09CE \u0982 \u0983 \u0981 \u099E\n"
        L"  Ctrl + Shift + P\t\tLive preview on / off\n\n"
        L"TYPING (phonetic)\n"
        L"  Lowercase 'o' is the inherent vowel:\n"
        L"      kol \u2192 \u0995\u09B2        rong \u2192 \u09B0\u0982\n"
        L"  Capital O forces an explicit \u0993 / \u09CB:\n"
        L"      sOnar \u2192 \u09B8\u09CB\u09A8\u09BE\u09B0\n"
        L"  Omit the vowel entirely to stack a conjunct:\n"
        L"      kl \u2192 \u0995\u09CD\u09B2\n"
        L"  Capitals select the retroflex series:\n"
        L"      T=\u099F  D=\u09A1  N=\u09A3  S=\u09B6  Sh=\u09B7  R=\u09A1\u09BC\n\n"
        L"The overlay at your caret shows what was captured, what it composed,\n"
        L"and the alternatives. Click an alternative or press Alt+1..9 to choose it.",
        L"Shobdomala \u2014 shortcuts and typing guide",
        MB_OK | MB_ICONINFORMATION);
}

/// Captures the current state and writes it out, so the next launch starts where this one
/// left off. Called on every change rather than only at exit: an IME is usually ended by
/// logging off or by Task Manager, neither of which runs shutdown code.
static void persistSettings() {
    auto& state = KeyboardState::getInstance();
    g_settings.startupMode = state.getMode();
    g_settings.livePreview = state.isLivePreviewEnabled();
    g_settings.onScreenKeyboardVisible = g_onScreenKeyboard.isVisible();
    g_onScreenKeyboard.position(g_settings.onScreenKeyboardX, g_settings.onScreenKeyboardY);
    g_settings.save(g_settingsPath);
}

/// Mirrors the current state onto the tray icon.
static void syncTray() {
    auto& state = KeyboardState::getInstance();
    g_tray.refresh(state.getMode(), state.isLivePreviewEnabled(),
                   g_onScreenKeyboard.isVisible());
}

static void handleTrayCommand(TrayIcon::Command command) {
    auto& state = KeyboardState::getInstance();

    switch (command) {
        case TrayIcon::Command::ToggleMode:        state.toggleMode(); break;
        case TrayIcon::Command::SetEnglish:        state.setMode(InputMode::ENGLISH); break;
        case TrayIcon::Command::SetPhonetic:       state.setMode(InputMode::BENGALI_PHONETIC); break;
        case TrayIcon::Command::SetFixedLayout:    state.setMode(InputMode::BENGALI_FIXED); break;
        case TrayIcon::Command::ToggleLivePreview: state.toggleLivePreview(); break;

        case TrayIcon::Command::ToggleOnScreenKeyboard:
            g_onScreenKeyboard.toggle();
            syncTray();
            persistSettings();
            break;

        case TrayIcon::Command::ShowHelp:
            showHelp();
            break;

        case TrayIcon::Command::Exit:
            persistSettings();
            g_hook.uninstall();
            g_hook.stopMessageLoop();
            break;
    }
}

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
    // --- console policy -------------------------------------------------
    // This is linked as a GUI-subsystem application, so it starts with no console at all.
    // Decide up front whether one is needed, before anything tries to write to std::cout.
    //
    //   --test / --demo   : the REPL reads std::cin, so a console is mandatory.
    //   --verbose         : the user asked to watch the engine trace.
    //   otherwise         : attach to the launching terminal if there is one, and stay
    //                       silent if there is not. No stray window on the desktop.
    bool wantsDemo = false;
    bool wantsVerbose = false;
    bool wantsLivePreview = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--test" || arg == "--demo" || arg == "-t") {
            wantsDemo = true;
        } else if (arg == "--verbose" || arg == "-v") {
            wantsVerbose = true;
        } else if (arg == "--no-preview") {
            wantsLivePreview = false;
        }
    }

    g_console.attach(wantsDemo || wantsVerbose);

    // Preferences from the last run. A missing or corrupt file leaves the defaults.
    g_settingsPath = Settings::defaultPath();
    g_settings.load(g_settingsPath);

    PhoneticEngine engine;
    FixedLayoutEngine layout;
    WordDictionary words;

    if (!engine.loadRules(findConfig("phonetic_rules.json"))) {
        std::cerr << "[WARNING] Could not load config/phonetic_rules.json!" << std::endl;
    }
    if (!engine.loadExceptions(findConfig("exceptions.json"))) {
        std::cout << "[INIT] No exceptions.json found; continuing without word overrides." << std::endl;
    }
    if (!words.loadFromFile(findConfig("words_bangla.json"))) {
        std::cout << "[INIT] No word list found; prediction and autocorrect are off."
                  << std::endl;
    } else {
        engine.setCandidateResolver(std::make_unique<DictionaryCandidateResolver>(&words));
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
                  << layout.size() << " fixed-layout keys, "
                  << words.size() << " dictionary words." << std::endl;
    }

    // Saved preferences first, then command-line flags, which are an explicit override for
    // this run only and so must win.
    KeyboardState::getInstance().setLivePreview(g_settings.livePreview);
    KeyboardState::getInstance().setMode(g_settings.startupMode);
    if (!wantsLivePreview) {
        KeyboardState::getInstance().setLivePreview(false);
    }

    if (wantsDemo) {
        if (!g_console.active()) {
            // Nothing to read from and nothing to print to: say so where the user will
            // actually see it, rather than exiting silently.
            MessageBoxW(nullptr,
                        L"Demo mode needs a console and one could not be opened.\n"
                        L"Run shobdomala.exe --test from a command prompt.",
                        L"Shobdomala", MB_OK | MB_ICONWARNING);
            return 1;
        }
        int result = runOfflineDemo(engine, layout);
        g_console.waitBeforeClosing();
        return result;
    }

    // Register console Ctrl+C handler for graceful hook cleanup
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // ---------------------------------------------------------------------
    // User interface
    // ---------------------------------------------------------------------
    // All three surfaces live on this thread, which is also the thread the keyboard hook
    // runs its callback on. That is deliberate: the hook can update the overlay with a
    // direct call, with no cross-thread marshalling on the keystroke path.
    HINSTANCE instance = GetModuleHandleW(nullptr);

    if (!g_candidateWindow.create(instance)) {
        std::cerr << "[WARNING] Could not create the composition overlay; "
                     "falling back to console-only preview." << std::endl;
    }
    g_candidateWindow.setOnSelect([](size_t optionIndex) {
        KeyboardHook::selectCandidate(optionIndex);
    });
    g_candidateWindow.setOnSelectWord([](const std::string& word) {
        // One callback, two meanings, disambiguated by whether a word is still in progress:
        // mid-word it completes, after the boundary it corrects what already landed.
        KeyboardHook::selectWordOrCorrection(word);
    });

    if (!g_onScreenKeyboard.create(instance, &layout)) {
        std::cerr << "[WARNING] Could not create the on-screen keyboard." << std::endl;
    }
    g_onScreenKeyboard.setPosition(g_settings.onScreenKeyboardX, g_settings.onScreenKeyboardY);
    if (g_settings.onScreenKeyboardVisible) {
        g_onScreenKeyboard.show();
    }
    g_onScreenKeyboard.setOnKey([](const std::string& glyph) {
        // The board never takes focus, so this lands in whatever the user was typing in.
        InputInjector::injectText(glyph);
    });

    if (!g_tray.create(instance)) {
        std::cerr << "[WARNING] Could not create the tray icon; "
                     "use the keyboard shortcuts instead." << std::endl;
    }
    g_tray.setOnCommand(handleTrayCommand);

    // Mode can change from a hotkey inside the hook or from the tray menu. Both routes
    // end up here, so the icon can never disagree with the engine.
    KeyboardState::getInstance().setOnChanged([]() {
        syncTray();
        persistSettings();
        auto& state = KeyboardState::getInstance();
        g_tray.notify(L"Shobdomala",
                      state.getMode() == InputMode::ENGLISH
                          ? L"English"
                          : (state.getMode() == InputMode::BENGALI_FIXED
                                 ? L"Bengali \u2014 fixed layout"
                                 : L"Bengali \u2014 phonetic"));
    });
    syncTray();

    // Print welcome banner and shortcuts summary
    std::cout << "========================================================\n"
              << "   Shobdomala: Windows Bengali Phonetic Input Prototype\n"
              << "========================================================\n"
              << "Keyboard Shortcuts:\n"
              << "  [Ctrl + Shift + B]     : Toggle ENGLISH <-> BENGALI (phonetic)\n"
              << "  [Ctrl + Shift + L]     : Cycle English / phonetic / fixed layout\n"
              << "  [Ctrl + Shift + Space] : Cycle ambiguous candidate\n"
              << "  [Alt + 1..9]           : Directly select candidate chip 1..9\n"
              << "  [Ctrl + 1..9]          : Directly select word prediction 1..9\n"
              << "  [Ctrl + Shift + D]     : Special characters (Khanda Ta, Anusvara, ...)\n"
              << "  [Ctrl + Shift + P]     : Toggle live in-place preview\n"
              << "  [Ctrl + C]             : Exit Shobdomala cleanly\n"
              << "--------------------------------------------------------\n"
              << "A composition overlay follows your caret while you type.\n"
              << "The tray icon shows the current mode; right-click it for\n"
              << "the on-screen keyboard and the typing guide.\n"
              << "--------------------------------------------------------\n"
              << "Initial Mode:  " << KeyboardState::getInstance().getModeString() << "\n"
              << "Live preview:  "
              << (KeyboardState::getInstance().isLivePreviewEnabled() ? "ON" : "OFF")
              << "  (start with --no-preview for flush-on-space behaviour)\n"
              << "To start typing Bengali in any window, press Ctrl+Shift+B.\n"
              << "========================================================\n" << std::endl;

    // Install the low-level keyboard hook
    if (!g_hook.install(&engine, &layout, &g_candidateWindow, &words)) {
        std::cerr << "[ERROR] Failed to install keyboard hook. Terminating." << std::endl;
        return 1;
    }

    // Run the required Win32 message pump
    g_hook.runMessageLoop();

    // Clean uninstall upon exit
    persistSettings();
    g_hook.uninstall();
    g_onScreenKeyboard.destroy();
    g_candidateWindow.destroy();
    g_tray.destroy();
    std::cout << "[MAIN] Program terminated cleanly." << std::endl;
    return 0;
}
