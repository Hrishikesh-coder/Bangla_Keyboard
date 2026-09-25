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

    /**
     * @brief Sends `count` backspace keystrokes to the focused window.
     *
     * Needed for live in-place preview: to redraw an in-progress word we must first
     * remove the version already on screen. Real backspace virtual-key events are used
     * rather than KEYEVENTF_UNICODE, because U+0008 is not a deletion request.
     *
     * @return Number of backspaces actually accepted by the input stream.
     */
    static size_t injectBackspaces(size_t count);

    /**
     * @brief Replaces text already on screen with new text, in one input batch.
     *
     * Erases `previousUnits` UTF-16 code units and types `utf8Text` in their place.
     * Batching both halves into a single SendInput call matters: two calls can be
     * interleaved with real keystrokes, which shows up as visible flicker or, worse,
     * backspaces landing after the replacement text.
     *
     * @return Number of UTF-16 code units now on screen (the length of `utf8Text`).
     */
    static size_t replaceText(size_t previousUnits, const std::string& utf8Text);

    /**
     * @brief Counts UTF-16 code units in a UTF-8 string.
     *
     * This is the unit backspace operates on in Windows edit controls, and it is not the
     * same as the number of bytes or the number of codepoints.
     */
    static size_t utf16UnitCount(const std::string& utf8Text);
};
