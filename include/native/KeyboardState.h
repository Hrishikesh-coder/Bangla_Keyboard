#pragma once

#include <string>

/**
 * @brief Represents the active keyboard input mode.
 *
 * BENGALI is kept as an alias for BENGALI_PHONETIC so code and rule files written against
 * the original two-mode prototype keep compiling.
 */
enum class InputMode {
    ENGLISH,
    BENGALI_PHONETIC,   ///< Roman input, buffered and transliterated by PhoneticEngine.
    BENGALI_FIXED,      ///< Direct key -> glyph mapping through FixedLayoutEngine.

    BENGALI = BENGALI_PHONETIC
};

/**
 * @brief Global keyboard state: input mode and input-behaviour switches.
 *
 * All keyboard shortcut configurations are centralized here for easy modification:
 * - Toggle English/Bengali phonetic: Ctrl + Shift + B
 * - Cycle through all three modes:   Ctrl + Shift + L
 * - Cycle candidate:                 Ctrl + Shift + Space
 * - Special Bengali characters:      Ctrl + Shift + D
 * - Toggle live preview:             Ctrl + Shift + P
 */
class KeyboardState {
public:
    static KeyboardState& getInstance();

    InputMode getMode() const;
    void setMode(InputMode mode);

    /// Switches between ENGLISH and BENGALI_PHONETIC, preserving the original Ctrl+Shift+B behaviour.
    void toggleMode();

    /// Advances ENGLISH -> BENGALI_PHONETIC -> BENGALI_FIXED -> ENGLISH.
    void cycleMode();

    const char* getModeString() const;

    /// True for any Bengali input mode.
    bool isBengali() const;

    /**
     * @brief Whether the in-progress word is rendered into the target app as it is typed.
     *
     * On: each keystroke erases the previous rendering with backspaces and injects the
     * updated one, so the user sees Bengali forming live, as in Avro.
     * Off: nothing is injected until a word delimiter is typed (the original behaviour).
     *
     * The switch exists because the live path assumes the caret has not moved since the
     * word began. In an app that rewrites the field under you - an autocomplete box, a
     * terminal, a spreadsheet cell editor - the backspaces can delete the wrong text, and
     * flush-on-delimiter is the safe fallback.
     */
    bool isLivePreviewEnabled() const;
    void setLivePreview(bool enabled);
    void toggleLivePreview();

    // Centralized shortcut definitions
    static constexpr int TOGGLE_VK       = 'B';
    static constexpr int CYCLE_VK        = 0x20; // VK_SPACE
    static constexpr int SPECIAL_VK      = 'D';
    static constexpr int MODE_CYCLE_VK   = 'L';
    static constexpr int LIVE_PREVIEW_VK = 'P';

private:
    KeyboardState() = default;
    InputMode m_mode = InputMode::ENGLISH;
    bool m_livePreview = true;
};
