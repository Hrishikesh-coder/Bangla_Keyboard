#include "native/KeyboardHook.h"
#include "native/InputInjector.h"
#include <iostream>
#include <cctype>

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
 * ============================================================================
 */

HHOOK KeyboardHook::s_hHook = nullptr;
PhoneticEngine* KeyboardHook::s_engine = nullptr;
InputBuffer KeyboardHook::s_buffer;
SpecialCharPicker KeyboardHook::s_specialPicker;
DWORD KeyboardHook::s_threadId = 0;

KeyboardHook::~KeyboardHook() {
    uninstall();
}

bool KeyboardHook::install(PhoneticEngine* engine) {
    if (s_hHook != nullptr) {
        return true; // Already installed
    }

    s_engine = engine;
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
        s_buffer.clear();
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
    // If set, pass through immediately so we don't process our own output!
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
    // Step 2: Global Toggle Shortcut: Ctrl + Shift + B
    // ------------------------------------------------------------------------
    if (isKeyDown && isCtrl && isShift && kbd->vkCode == KeyboardState::TOGGLE_VK) {
        state.toggleMode();
        std::cout << "\n============================================\n"
                  << " [MODE TOGGLE] Input mode switched to: "
                  << state.getModeString() << "\n"
                  << "============================================" << std::endl;

        // If toggling to English, flush or discard any leftover Roman buffer
        if (state.getMode() == InputMode::ENGLISH && !s_buffer.empty()) {
            s_buffer.clear();
            if (s_engine) s_engine->clearActive();
        }
        return 1; // Consume key event
    }

    // ------------------------------------------------------------------------
    // Step 3: English Mode: Pass everything through unchanged
    // ------------------------------------------------------------------------
    if (state.getMode() == InputMode::ENGLISH) {
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    // ========================================================================
    // BENGALI MODE PROCESSING
    // ========================================================================

    // Shortcut 3a: Candidate cycling: Ctrl + Shift + Space
    if (isKeyDown && isCtrl && isShift && kbd->vkCode == KeyboardState::CYCLE_VK) {
        if (s_engine && !s_buffer.empty()) {
            bool cycled = s_engine->cycleActiveCandidate();
            if (cycled) {
                std::cout << "[CANDIDATE CYCLE] " << s_buffer.content()
                          << " -> " << s_engine->getActiveComposedString() << std::endl;
            }
        }
        return 1; // Consume
    }

    // Shortcut 3b: Special character picker: Ctrl + Shift + D
    if (isKeyDown && isCtrl && isShift && kbd->vkCode == KeyboardState::SPECIAL_VK) {
        s_specialPicker.activate();
        std::cout << "\n" << s_specialPicker.getMenuDisplay()
                  << "\n[PICKER] Press 1-5 to insert, or any other key to cancel." << std::endl;
        return 1; // Consume
    }

    // Handle active Special Character Picker digit selection
    if (s_specialPicker.isActive()) {
        if (isKeyDown) {
            auto result = s_specialPicker.handleKey(kbd->vkCode);
            if (result.has_value()) {
                std::cout << "[PICKER] Injected: " << result.value() << std::endl;
                InputInjector::injectText(result.value());
                return 1; // Consume digit key
            }
            // If another key was pressed, handleKey deactivated the picker
            // and we allow this key to proceed to normal processing below.
        } else if (isKeyUp) {
            // If the keyup corresponds to a digit that triggered selection, consume it
            if ((kbd->vkCode >= '1' && kbd->vkCode <= '5') ||
                (kbd->vkCode >= 0x61 && kbd->vkCode <= 0x65)) {
                return 1;
            }
        }
    }

    // ------------------------------------------------------------------------
    // Step 4: Roman Alphabetic Keys ('A' - 'Z') -> Buffer Capture
    // ------------------------------------------------------------------------
    if (!isCtrl && !isAlt && kbd->vkCode >= 'A' && kbd->vkCode <= 'Z') {
        if (isKeyDown) {
            char c = isShift ? static_cast<char>(kbd->vkCode)
                             : static_cast<char>(std::tolower(kbd->vkCode));
            s_buffer.append(c);

            if (s_engine) {
                s_engine->updateActiveBuffer(s_buffer.content());
                std::cout << "[BUFFER] \"" << s_buffer.content() << "\" -> "
                          << s_engine->getActiveComposedString() << std::endl;
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
                if (s_engine) {
                    s_engine->updateActiveBuffer(s_buffer.content());
                    std::cout << "[BACKSPACE] Buffer: \"" << s_buffer.content() << "\" -> "
                              << s_engine->getActiveComposedString() << std::endl;
                }
            }
            return 1; // Consume backspace while buffer has content
        }
        // If buffer was already empty, let backspace pass through to active app
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    // ------------------------------------------------------------------------
    // Step 6: Word Delimiters & Punctuation -> Flush Buffer & Inject
    // ------------------------------------------------------------------------
    // Trigger on Space, Enter, or common punctuation keys
    const bool isSpace = (kbd->vkCode == VK_SPACE);
    const bool isReturn = (kbd->vkCode == VK_RETURN);
    const bool isPunctuation = (kbd->vkCode == VK_OEM_PERIOD ||
                                kbd->vkCode == VK_OEM_COMMA  ||
                                kbd->vkCode == VK_OEM_1      || // ;:
                                kbd->vkCode == VK_OEM_2      || // /?
                                kbd->vkCode == VK_OEM_7);       // '"

    if ((isSpace || isReturn || isPunctuation) && !s_buffer.empty()) {
        if (isKeyDown) {
            std::string roman = s_buffer.content();
            std::string bengali = s_engine ? s_engine->transliterate(roman) : roman;

            std::cout << "[FLUSH] \"" << roman << "\" ==> \"" << bengali << "\"" << std::endl;

            // Clear buffer before injection to prevent race conditions
            s_buffer.clear();
            if (s_engine) s_engine->clearActive();

            // Inject the Bengali Unicode string into the active application
            InputInjector::injectText(bengali);
        }
        // Let the space/enter/punctuation pass through so formatting is preserved
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    // Any other key: pass through unchanged
    return CallNextHookEx(s_hHook, nCode, wParam, lParam);
}
