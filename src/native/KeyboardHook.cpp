#include "native/KeyboardHook.h"
#include "native/InputInjector.h"
#include <iostream>
#include <cctype>
#include <algorithm>

/*
 * ============================================================================
 * WINDOWS LOW-LEVEL KEYBOARD API EXPLANATIONS:
 *
 * 1. SetWindowsHookExW
 *    - What it does:
 *      Installs an application-defined hook procedure into an existing hook chain.
 *      With WH_KEYBOARD_LL, Windows monitors keyboard input events globally across
 *      the entire operating system before they are posted to any thread's message queue.
 *    - Why we use it:
 *      We need system-wide interception of keystrokes so users can type Bengali
 *      phonetically into any application (Notepad, Chrome, VS Code, etc.).
 *    - Data received:
 *      idHook (WH_KEYBOARD_LL = 13), lpfn (pointer to hookCallback),
 *      hMod (GetModuleHandle(NULL)), dwThreadId (0 = global hook across all threads).
 *    - Data returned:
 *      HHOOK handle identifying the installed hook, or NULL if the call failed.
 *
 * 2. UnhookWindowsHookEx
 *    - What it does:
 *      Removes a hook procedure installed in a hook chain by SetWindowsHookEx.
 *    - Why we use it:
 *      Essential for clean shutdown so the OS does not leave dangling callback
 *      pointers or degrade system keyboard responsiveness after exit.
 *    - Data received:
 *      hhk (the HHOOK handle returned by SetWindowsHookEx).
 *    - Data returned:
 *      Non-zero (TRUE) if successful; zero (FALSE) on failure.
 *
 * 3. CallNextHookEx
 *    - What it does:
 *      Passes the hook information to the next hook procedure in the current hook chain.
 *    - Why we use it:
 *      Whenever we choose NOT to consume a key event (e.g. English mode, non-alphabetic keys,
 *      or our own injected events), we must allow other applications and hooks to receive it.
 *    - Data received:
 *      hhk (ignored by Windows NT, can pass s_hHook or NULL), nCode, wParam, lParam.
 *    - Data returned:
 *      LRESULT returned by the next hook procedure in the chain.
 *
 * 4. GetMessageW, TranslateMessage, DispatchMessageW
 *    - What they do:
 *      Standard Win32 message pump. GetMessage retrieves messages from the calling
 *      thread's message queue.
 *    - Why we use it:
 *      WH_KEYBOARD_LL is implemented by Windows via messages dispatched to the thread
 *      that called SetWindowsHookEx. Without a message pump running on that thread,
 *      Windows will not dispatch keyboard hook callbacks, and eventually uninstalls
 *      the hook as timed out!
 *    - Data received / returned:
 *      GetMessage returns >0 for normal messages, 0 for WM_QUIT, and -1 on error.
 *
 * 5. GetAsyncKeyState
 *    - What it does:
 *      Determines whether a key is up or down at the time the function is called,
 *      and whether the key was pressed after a previous call to GetAsyncKeyState.
 *    - Why we use it:
 *      Allows checking the real-time physical state of modifier keys (Ctrl, Shift, Alt)
 *      inside the low-level hook callback without maintaining complex key state tables.
 *    - Data received:
 *      vKey (virtual-key code, e.g. VK_CONTROL, VK_SHIFT).
 *    - Data returned:
 *      SHORT value where the most significant bit (0x8000) indicates whether the key
 *      is currently physically pressed down.
 *
 * 6. ToUnicodeEx
 *    - What it does:
 *      Translates a virtual-key code plus a keyboard state array into the Unicode
 *      characters the key would normally produce under a given keyboard layout.
 *    - Why we use it:
 *      Fixed Bengali layouts map punctuation keys (; ' , . / ` \) as well as letters.
 *      Deriving those from vkCode by hand would hardcode a US layout; ToUnicodeEx asks
 *      the active layout instead.
 *    - Important detail:
 *      Called with dwFlags bit 2 (0x4) set, which tells Windows not to disturb the
 *      current keyboard state. Without it, calling ToUnicodeEx from inside a low-level
 *      hook corrupts dead-key sequences for every other application on the desktop.
 *    - Data returned:
 *      Number of characters written; negative for a dead key; 0 when the key produces
 *      no character.
 * ============================================================================
 */

HHOOK KeyboardHook::s_hHook = nullptr;
PhoneticEngine* KeyboardHook::s_engine = nullptr;
FixedLayoutEngine* KeyboardHook::s_layout = nullptr;
CandidateWindow* KeyboardHook::s_candidateWindow = nullptr;
WordDictionary* KeyboardHook::s_words = nullptr;
InputBuffer KeyboardHook::s_buffer;
SpecialCharPicker KeyboardHook::s_specialPicker;
DWORD KeyboardHook::s_threadId = 0;
size_t KeyboardHook::s_previewUnits = 0;
std::string KeyboardHook::s_previewText;
std::string KeyboardHook::s_committedText;
size_t KeyboardHook::s_committedUnits = 0;
char KeyboardHook::s_committedDelimiter = 0;
UserDictionary* KeyboardHook::s_userWords = nullptr;
bool KeyboardHook::s_learnOnCommit = false;

namespace {
bool g_timingEnabled = false;
LARGE_INTEGER g_frequency = {};
double g_worstMs = 0.0;
unsigned long long g_samples = 0;
double g_totalMs = 0.0;
} // namespace

void KeyboardHook::setTimingEnabled(bool enabled) {
    g_timingEnabled = enabled;
    if (enabled && g_frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&g_frequency);
    }
}

KeyboardHook::~KeyboardHook() {
    uninstall();
}

bool KeyboardHook::install(PhoneticEngine* engine, FixedLayoutEngine* layout,
                           CandidateWindow* candidateWindow, WordDictionary* words) {
    if (s_hHook != nullptr) {
        return true; // Already installed
    }

    s_engine = engine;
    s_layout = layout;
    s_candidateWindow = candidateWindow;
    s_words = words;
    s_threadId = GetCurrentThreadId();

    HINSTANCE hInstance = GetModuleHandleW(nullptr);
    s_hHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        hookCallback,
        hInstance,
        0 // 0 = associate hook with all existing threads (system-wide)
    );

    if (s_hHook == nullptr) {
        DWORD err = GetLastError();
        std::cerr << "[KeyboardHook] SetWindowsHookExW failed with error code: " << err << std::endl;
        return false;
    }

    std::cout << "[KeyboardHook] System-wide low-level keyboard hook successfully installed." << std::endl;
    return true;
}

void KeyboardHook::uninstall() {
    if (s_hHook != nullptr) {
        UnhookWindowsHookEx(s_hHook);
        s_hHook = nullptr;
        s_engine = nullptr;
        s_layout = nullptr;
        if (s_candidateWindow) {
            s_candidateWindow->hide();
        }
        s_candidateWindow = nullptr;
        s_words = nullptr;
        s_buffer.clear();
        s_previewUnits = 0;
        s_previewText.clear();
        std::cout << "[KeyboardHook] Hook cleanly uninstalled." << std::endl;
    }
}

bool KeyboardHook::isInstalled() const {
    return s_hHook != nullptr;
}

void KeyboardHook::runMessageLoop() {
    MSG msg;
    std::cout << "[KeyboardHook] Message pump running. Press Ctrl+C to exit." << std::endl;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void KeyboardHook::stopMessageLoop() {
    if (s_threadId != 0) {
        PostThreadMessageW(s_threadId, WM_QUIT, 0, 0);
    }
}

// ---------------------------------------------------------------------------
// Composition helpers
// ---------------------------------------------------------------------------

void KeyboardHook::refreshUi() {
    if (!s_candidateWindow) {
        return;
    }

    auto& state = KeyboardState::getInstance();

    if (s_buffer.empty() || !s_engine) {
        s_candidateWindow->hide();
        return;
    }

    CandidateWindow::Content content;
    content.roman = s_buffer.content();
    content.composed = s_engine->getActiveComposedString();
    content.candidates = s_engine->activeCandidateOptions();
    content.selectedIndex = s_engine->activeCandidateSelection();
    content.fixedMode = (state.getMode() == InputMode::BENGALI_FIXED);
    content.modeLabel = content.fixedMode ? "FIXED" : "PHONETIC";

    // Offer the longer rule tokens that still start with the last token typed, so that
    // "kh" and "kkh" are discoverable from "k" rather than having to be learnt from the
    // JSON file. Only the *continuations* are listed: echoing the token the user has
    // already typed back at them says nothing.
    if (!content.fixedMode) {
        const auto& active = s_engine->getActiveCandidates();
        if (!active.empty()) {
            const std::string& lastToken = active.back().romanToken;
            for (const auto& completion :
                 s_engine->getSymbolTable().trie().getCompletions(lastToken, 5)) {
                if (completion != lastToken) {
                    auto opts = s_engine->getSymbolTable().lookup(completion);
                    if (opts && !opts->empty()) {
                        const std::string& bengaliStr = (*opts)[0];
                        // Prevent duplicates (e.g. if two rules map to the same glyph)
                        if (std::find(content.suggestions.begin(), content.suggestions.end(), bengaliStr) == content.suggestions.end()) {
                            content.suggestions.push_back(bengaliStr);
                        }
                    }
                }
            }
        }
    }

    // Whole-word completions for what has been composed so far. Corrections deliberately
    // do NOT happen here: while a word is unfinished most of its prefixes match nothing,
    // and correcting them tells the user they mistyped a word they are typing correctly.
    // See suggestFor() and docs/LIMITATIONS.md.
    if (s_words) {
        SuggestionSet set = suggestFor(*s_words, content.composed, /*wordFinished=*/false);
        content.words = set.words;
        content.wordsAreCorrections = (set.kind == SuggestionKind::Correction);
    }

    s_candidateWindow->update(content);
}

void KeyboardHook::offerCorrections() {
    if (!s_words || !s_candidateWindow || s_committedText.empty()) {
        return;
    }

    SuggestionSet set = suggestFor(*s_words, s_committedText, /*wordFinished=*/true);
    if (set.kind != SuggestionKind::Correction || set.empty()) {
        s_candidateWindow->hide();
        return;
    }

    CandidateWindow::Content content;
    content.roman = s_committedText;
    content.composed = s_committedText;
    content.modeLabel = "TYPED";
    content.words = set.words;
    content.wordsAreCorrections = true;
    s_candidateWindow->update(content);
}

void KeyboardHook::applyCorrection(const std::string& word) {
    if (word.empty() || s_committedText.empty()) {
        return;
    }

    // The user just stated what they meant. That is a free, unambiguous label, and the
    // words it produces -- names, jargon, loanwords -- are precisely the ones no shipped
    // corpus contains.
    if (s_userWords) {
        s_userWords->learn(word);
    }

    // The document already holds the word plus the delimiter that ended it, so both come
    // out and both go back. Retyping the delimiter keeps the caret and the spacing exactly
    // where the user left them.
    const size_t toDelete = s_committedUnits + (s_committedDelimiter ? 1 : 0);
    std::string replacement = word;
    if (s_committedDelimiter) {
        replacement += s_committedDelimiter;
    }

    InputInjector::replaceText(toDelete, replacement);
    std::cout << "[CORRECT] \"" << s_committedText << "\" ==> \"" << word << "\"" << std::endl;

    s_committedText.clear();
    s_committedUnits = 0;
    s_committedDelimiter = 0;
    if (s_candidateWindow) {
        s_candidateWindow->hide();
    }
}

void KeyboardHook::selectWordOrCorrection(const std::string& word) {
    if (!s_buffer.empty()) {
        selectWord(word);       // still typing: complete it
    } else {
        applyCorrection(word);  // word already landed: replace it
    }
}

void KeyboardHook::selectWord(const std::string& word) {
    if (word.empty() || s_buffer.empty()) {
        return;
    }

    // Replace everything shown for this word, then finish it: a prediction is a decision
    // about the whole word, so leaving the buffer open would let the next keystroke append
    // to a word the user has already settled.
    s_previewUnits = InputInjector::replaceText(s_previewUnits, word);
    s_previewText = word;

    std::cout << "[PREDICT] \"" << s_buffer.content() << "\" ==> \"" << word << "\"" << std::endl;

    s_buffer.clear();
    if (s_engine) {
        s_engine->clearActive();
    }
    s_previewUnits = 0;
    s_previewText.clear();
    s_committedText.clear();
    s_committedUnits = 0;
    if (s_candidateWindow) {
        s_candidateWindow->hide();
    }
}

void KeyboardHook::selectCandidate(size_t optionIndex) {
    if (!s_engine || s_buffer.empty()) {
        return;
    }
    const int tokenIndex = s_engine->activeAmbiguousTokenIndex();
    if (tokenIndex < 0) {
        return;
    }
    if (s_engine->setActiveSelection(static_cast<size_t>(tokenIndex), optionIndex)) {
        refreshPreview();
        refreshUi();
    }
}

void KeyboardHook::refreshPreview() {
    if (!s_engine || !KeyboardState::getInstance().isLivePreviewEnabled()) {
        return;
    }

    std::string composed = s_engine->getActiveComposedString();
    if (composed == s_previewText) {
        return; // Nothing changed on screen; do not churn the input stream.
    }

    s_previewUnits = InputInjector::replaceText(s_previewUnits, composed);
    s_previewText = composed;
}

void KeyboardHook::commitBuffer() {
    if (s_buffer.empty()) {
        return;
    }

    const std::string roman = s_buffer.content();
    const std::string finalText = finalTextFor(roman);

    // The live preview is produced from the active candidate list, which does not consult
    // the exception dictionary. When the two disagree - "dhonnobad" previews as ধোন্নোবাদ
    // but commits as ধন্যবাদ - the preview is corrected here.
    if (finalText != s_previewText) {
        InputInjector::replaceText(s_previewUnits, finalText);
    }

    std::cout << "[COMMIT] \"" << roman << "\" ==> \"" << finalText << "\"" << std::endl;

    // If the user overrode the engine while composing this word, keep the result.
    if (s_learnOnCommit && s_userWords && !finalText.empty()) {
        s_userWords->learn(finalText);
        s_learnOnCommit = false;
    }

    // Remember what landed, so a correction can undo it.
    s_committedText = finalText;
    s_committedUnits = InputInjector::utf16UnitCount(finalText);

    s_buffer.clear();
    s_previewUnits = 0;
    s_previewText.clear();

    // The word is decided: the overlay has nothing left to say.
    if (s_candidateWindow) {
        s_candidateWindow->hide();
    }
}

std::string KeyboardHook::finalTextFor(const std::string& roman) {
    if (!s_engine) {
        return roman;
    }

    // A whole-word override wins over everything: it exists precisely for spellings the
    // rules cannot derive, and for English words that must pass through untouched.
    std::string overrideText;
    if (s_engine->getExceptions().lookup(roman, overrideText)) {
        s_engine->clearActive();
        return overrideText;
    }

    // Otherwise compose from the active candidate list rather than re-running
    // transliterate(). The active list carries whatever the user chose with
    // Ctrl+Shift+Space; transliterate() would rebuild from index 0 and silently discard
    // those selections. flushActive() also clears the active state.
    return s_engine->flushActive();
}

void KeyboardHook::discardBuffer() {
    if (s_previewUnits > 0) {
        InputInjector::injectBackspaces(s_previewUnits);
    }
    s_buffer.clear();
    if (s_engine) s_engine->clearActive();
    s_previewUnits = 0;
    s_previewText.clear();
    if (s_candidateWindow) {
        s_candidateWindow->hide();
    }
}

bool KeyboardHook::charFromKey(const KBDLLHOOKSTRUCT* kbd, char& out) {
    // Build a minimal keyboard state rather than calling GetKeyboardState: inside a
    // low-level hook the calling thread's state is not the focused thread's state, so
    // GetKeyboardState reports stale modifiers.
    BYTE keyState[256] = {0};
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        keyState[VK_SHIFT] = 0x80;
    }
    if (GetKeyState(VK_CAPITAL) & 0x0001) {
        keyState[VK_CAPITAL] = 0x01;
    }

    WCHAR buffer[8] = {0};
    // Flag 0x4 = do not modify keyboard state. Required inside a hook, or dead keys
    // break system-wide.
    int written = ToUnicodeEx(kbd->vkCode, kbd->scanCode, keyState, buffer, 8, 0x4,
                              GetKeyboardLayout(0));

    if (written == 1 && buffer[0] > 0 && buffer[0] < 128) {
        out = static_cast<char>(buffer[0]);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Hook callback
// ---------------------------------------------------------------------------

LRESULT CALLBACK KeyboardHook::hookCallback(int nCode, WPARAM wParam, LPARAM lParam) {
    // If nCode is less than zero, the hook procedure must pass the message
    // to CallNextHookEx without further processing and should return the value.
    if (nCode < 0) {
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    auto* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

    // ------------------------------------------------------------------------
    // Step 1: Prevent recursion from our own injected events
    // ------------------------------------------------------------------------
    // When SendInput synthesizes events, the OS sets the LLKHF_INJECTED flag.
    // If set, pass through immediately so we don't process our own output! This now
    // covers the backspaces used for live preview as well as the Bengali characters.
    if (kbd->flags & LLKHF_INJECTED) {
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    const bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    const bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

    // Query physical state of modifier keys
    const bool isCtrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool isShift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
    const bool isAlt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;

    auto& state = KeyboardState::getInstance();

    // ------------------------------------------------------------------------
    // Step 2: Global shortcuts
    // ------------------------------------------------------------------------

    // Ctrl + Shift + B: toggle ENGLISH <-> BENGALI (phonetic)
    if (isKeyDown && isCtrl && isShift && kbd->vkCode == KeyboardState::TOGGLE_VK) {
        // Finish whatever is in flight before the rules underneath it change.
        commitBuffer();
        state.toggleMode();
        std::cout << "\n============================================\n"
                  << " [MODE TOGGLE] Input mode switched to: "
                  << state.getModeString() << "\n"
                  << "============================================" << std::endl;
        return 1; // Consume key event
    }

    // Ctrl + Shift + L: cycle ENGLISH -> phonetic -> fixed layout -> ENGLISH
    if (isKeyDown && isCtrl && isShift && kbd->vkCode == KeyboardState::MODE_CYCLE_VK) {
        commitBuffer();
        state.cycleMode();
        if (state.getMode() == InputMode::BENGALI_FIXED && (!s_layout || s_layout->size() == 0)) {
            std::cout << "[MODE] Fixed-layout mode selected but no layout is loaded; "
                         "keys will pass through unchanged." << std::endl;
        }
        std::cout << "[MODE] " << state.getModeString() << std::endl;
        return 1;
    }

    // Ctrl + Shift + P: toggle live in-place preview
    if (isKeyDown && isCtrl && isShift && kbd->vkCode == KeyboardState::LIVE_PREVIEW_VK) {
        commitBuffer();
        state.toggleLivePreview();
        std::cout << "[PREVIEW] Live in-place preview "
                  << (state.isLivePreviewEnabled() ? "ON" : "OFF (flush on delimiter)")
                  << std::endl;
        return 1;
    }

    // ------------------------------------------------------------------------
    // Step 3: English Mode: Pass everything through unchanged
    // ------------------------------------------------------------------------
    if (!state.isBengali()) {
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    // ========================================================================
    // FIXED LAYOUT MODE
    // ========================================================================
    // Stateless: one key, one glyph, injected immediately. No buffer, no candidates,
    // no preview bookkeeping - which is exactly why fixed layouts feel predictable.
    if (state.getMode() == InputMode::BENGALI_FIXED) {
        if (!s_layout) {
            return CallNextHookEx(s_hHook, nCode, wParam, lParam);
        }

        // AltGr is the third shift level of a real fixed layout - Probhat keeps the nukta,
        // ৗ, ঽ and the currency signs there. Windows has no AltGr virtual key: the
        // keyboard driver synthesises left-Ctrl plus right-Alt, so AltGr is detected as
        // "right Alt is down", and a genuine Ctrl+Alt chord is indistinguishable from it.
        // That ambiguity is the platform's, not ours.
        const bool isAltGr = (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;

        if ((isCtrl || isAlt) && !isAltGr) {
            return CallNextHookEx(s_hHook, nCode, wParam, lParam);
        }

        const auto level = isAltGr ? FixedLayoutEngine::Level::AltGr
                                   : FixedLayoutEngine::Level::Base;

        char key = 0;
        if (charFromKey(kbd, key) && s_layout->isMapped(key, level)) {
            if (isKeyDown) {
                InputInjector::injectText(s_layout->mapKey(key, level));
            }
            return 1; // Consume both down and up for mapped keys
        }
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    // ========================================================================
    // PHONETIC MODE
    // ========================================================================

    // Shortcut: Candidate cycling: Ctrl + Shift + Space
    if (isKeyDown && isCtrl && isShift && kbd->vkCode == KeyboardState::CYCLE_VK) {
        if (s_engine && !s_buffer.empty()) {
            if (s_userWords) {
                // Cycling is also a correction: the engine offered something and the user
                // rejected it. Learn the word they settle on, once it is committed.
                s_learnOnCommit = true;
            }
            if (s_engine->cycleActiveCandidate()) {
                std::cout << "[CANDIDATE CYCLE] " << s_buffer.content()
                          << " -> " << s_engine->getActiveComposedString() << std::endl;
                refreshPreview();
                refreshUi();
            }
        }
        return 1; // Consume
    }

    // Shortcut: Direct candidate selection via Alt + 1..9
    if (isKeyDown && isAlt && !isCtrl && !isShift && kbd->vkCode >= '1' && kbd->vkCode <= '9') {
        size_t idx = static_cast<size_t>(kbd->vkCode - '1');
        if (s_engine && !s_buffer.empty()) {
            auto opts = s_engine->activeCandidateOptions();
            if (idx < opts.size()) {
                selectCandidate(idx);
                return 1;
            }
        }
    }

    // Shortcut: Direct word prediction / correction selection via Ctrl + 1..9
    if (isKeyDown && isCtrl && !isAlt && !isShift && kbd->vkCode >= '1' && kbd->vkCode <= '9') {
        size_t idx = static_cast<size_t>(kbd->vkCode - '1');
        if (s_words && (!s_buffer.empty() || !s_committedText.empty())) {
            const std::string query = !s_buffer.empty()
                                          ? (s_engine ? s_engine->getActiveComposedString() : "")
                                          : s_committedText;
            SuggestionSet set = suggestFor(*s_words, query, s_buffer.empty());
            if (idx < set.words.size()) {
                selectWordOrCorrection(set.words[idx]);
                return 1;
            }
        }
    }

    if (isKeyUp && (isAlt || isCtrl) && !isShift && kbd->vkCode >= '1' && kbd->vkCode <= '9') {
        return 1;
    }

    // Shortcut: Special character picker: Ctrl + Shift + D
    if (isKeyDown && isCtrl && isShift && kbd->vkCode == KeyboardState::SPECIAL_VK) {
        s_specialPicker.activate();
        std::cout << "\n" << s_specialPicker.getMenuDisplay()
                  << "\n[PICKER] Press the digit to insert, or any other key to cancel."
                  << std::endl;
        return 1; // Consume
    }

    // Handle active Special Character Picker digit selection
    if (s_specialPicker.isActive()) {
        if (isKeyDown) {
            auto result = s_specialPicker.handleKey(kbd->vkCode);
            if (result.has_value()) {
                // Commit first: the picked sign belongs after the finished word, not
                // inside the preview we are about to erase.
                commitBuffer();
                std::cout << "[PICKER] Injected: " << result.value() << std::endl;
                InputInjector::injectText(result.value());
                return 1; // Consume digit key
            }
            // If another key was pressed, handleKey deactivated the picker
            // and we allow this key to proceed to normal processing below.
        } else if (isKeyUp) {
            // If the keyup corresponds to a digit that triggered selection, consume it
            if ((kbd->vkCode >= '1' && kbd->vkCode <= '9') ||
                (kbd->vkCode >= 0x61 && kbd->vkCode <= 0x69)) {
                return 1;
            }
        }
    }

    // ------------------------------------------------------------------------
    // Step 4: Roman letters -> buffer capture (and live preview)
    // ------------------------------------------------------------------------
    if (!isCtrl && !isAlt && kbd->vkCode >= 'A' && kbd->vkCode <= 'Z') {
        if (isKeyDown) {
            // Resolve through the active layout so Caps Lock and Shift both behave.
            // Case matters to the rules: T is ট while t is ত.
            char c = 0;
            if (!charFromKey(kbd, c)) {
                c = isShift ? static_cast<char>(kbd->vkCode)
                            : static_cast<char>(std::tolower(kbd->vkCode));
            }

            // A new word begins: whatever correction was on offer for the previous one is
            // no longer actionable, because the text it would edit is no longer adjacent
            // to the caret.
            s_committedText.clear();
            s_committedUnits = 0;

            LARGE_INTEGER start = {};
            if (g_timingEnabled) {
                QueryPerformanceCounter(&start);
            }

            s_buffer.append(c);
            if (s_engine) {
                s_engine->updateActiveBuffer(s_buffer.content());
                std::cout << "[BUFFER] \"" << s_buffer.content() << "\" -> "
                          << s_engine->getActiveComposedString() << std::endl;
                refreshPreview();
                refreshUi();
            }

            if (g_timingEnabled && g_frequency.QuadPart != 0) {
                LARGE_INTEGER end;
                QueryPerformanceCounter(&end);
                const double ms = 1000.0 * static_cast<double>(end.QuadPart - start.QuadPart)
                                  / static_cast<double>(g_frequency.QuadPart);
                ++g_samples;
                g_totalMs += ms;
                g_worstMs = (ms > g_worstMs) ? ms : g_worstMs;
                std::cout << "[TIMING] " << ms << " ms  (worst " << g_worstMs
                          << ", mean " << (g_totalMs / static_cast<double>(g_samples))
                          << ", budget 300)" << std::endl;
            }
        }
        return 1; // Consume both keydown and keyup for captured letters
    }

    // ------------------------------------------------------------------------
    // Step 5: Backspace handling
    // ------------------------------------------------------------------------
    if (kbd->vkCode == VK_BACK) {
        if (!s_buffer.empty()) {
            if (isKeyDown) {
                s_buffer.backspace();
                if (s_buffer.empty()) {
                    // Nothing left of the word: erase the preview entirely.
                    discardBuffer();
                    std::cout << "[BACKSPACE] Buffer cleared." << std::endl;
                } else if (s_engine) {
                    s_engine->updateActiveBuffer(s_buffer.content());
                    std::cout << "[BACKSPACE] Buffer: \"" << s_buffer.content() << "\" -> "
                              << s_engine->getActiveComposedString() << std::endl;
                    refreshPreview();
                    refreshUi();
                }
            }
            return 1; // Consume backspace while buffer has content
        }
        // If buffer was already empty, let backspace pass through to active app
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    // ------------------------------------------------------------------------
    // Step 6: Word delimiters & punctuation -> commit the word
    // ------------------------------------------------------------------------
    const bool isSpace = (kbd->vkCode == VK_SPACE);
    const bool isReturn = (kbd->vkCode == VK_RETURN);
    const bool isPunctuation = (kbd->vkCode == VK_OEM_PERIOD ||
                                kbd->vkCode == VK_OEM_COMMA  ||
                                kbd->vkCode == VK_OEM_1      || // ;:
                                kbd->vkCode == VK_OEM_2      || // /?
                                kbd->vkCode == VK_OEM_7);       // '"

    if ((isSpace || isReturn || isPunctuation) && !s_buffer.empty()) {
        if (isKeyDown) {
            // Remember which delimiter ended the word. A correction has to retype it, and
            // Enter cannot be retyped safely -- re-injecting a newline could submit a form
            // or run a command -- so a word ended by Enter is committed without an offer.
            s_committedDelimiter = isSpace ? ' '
                                 : isReturn ? 0
                                 : '\0';
            if (isPunctuation) {
                char punctuation = 0;
                s_committedDelimiter = charFromKey(kbd, punctuation) ? punctuation : 0;
            }

            if (KeyboardState::getInstance().isLivePreviewEnabled()) {
                // The word is already on screen; commitBuffer only patches it if the
                // final text disagrees with what was previewed.
                commitBuffer();
            } else {
                // Flush-on-delimiter path: nothing has been injected yet.
                const std::string roman = s_buffer.content();
                const std::string bengali = finalTextFor(roman);
                std::cout << "[FLUSH] \"" << roman << "\" ==> \"" << bengali << "\"" << std::endl;
                // Clear buffer before injection to prevent race conditions
                s_buffer.clear();
                s_previewUnits = 0;
                s_previewText.clear();
                if (s_candidateWindow) {
                    s_candidateWindow->hide();
                }
                InputInjector::injectText(bengali);
            }
        }
        // Let the space/enter/punctuation pass through so formatting is preserved. Only
        // once it has landed does the correction offer make sense, because the delimiter
        // is part of what a correction would have to replace.
        if (isKeyDown && !isReturn) {
            offerCorrections();
        }
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    // Any other key (arrows, Home/End, Tab, mouse-driven focus changes...) ends the word:
    // the caret is about to move, so the preview must not be edited by backspaces any more.
    const bool isModifier = (kbd->vkCode == VK_CONTROL || kbd->vkCode == VK_LCONTROL || kbd->vkCode == VK_RCONTROL ||
                             kbd->vkCode == VK_SHIFT   || kbd->vkCode == VK_LSHIFT   || kbd->vkCode == VK_RSHIFT   ||
                             kbd->vkCode == VK_MENU    || kbd->vkCode == VK_LMENU    || kbd->vkCode == VK_RMENU    ||
                             kbd->vkCode == VK_CAPITAL || kbd->vkCode == VK_LWIN     || kbd->vkCode == VK_RWIN);
    if (isKeyDown && !s_buffer.empty() && !isModifier) {
        commitBuffer();
    }

    return CallNextHookEx(s_hHook, nCode, wParam, lParam);
}
