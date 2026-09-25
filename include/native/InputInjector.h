#pragma once

#include <string>

/**
 * @brief Injects synthesized Unicode text into the currently active Windows application.
 *
 * Uses the Win32 SendInput API with KEYBDINPUT set to KEYEVENTF_UNICODE.
 * Automatically flags events as injected, allowing the low-level hook to skip them.
 */
class InputInjector {
public:
    InputInjector() = default;

    /**
     * @brief Injects a UTF-8 encoded string as Unicode keyboard events into the focused window.
     * @param utf8Text UTF-8 string to inject.
     * @return Number of characters successfully sent.
     */
    static size_t injectText(const std::string& utf8Text);

    /**
     * @brief Injects a single UTF-16 code unit (wchar_t).
     * @param ch Wide character code point.
     * @return True if successfully sent.
     */
    static bool injectChar(wchar_t ch);
};
