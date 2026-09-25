#pragma once

#include <string>

/**
 * @brief Represents the active keyboard input mode.
 */
enum class InputMode {
    ENGLISH,
    BENGALI
};

/**
 * @brief Global keyboard state tracker for English/Bengali input mode.
 *
 * All keyboard shortcut configurations are centralized here for easy modification:
 * - Toggle English/Bengali: Ctrl + Shift + B
 * - Cycle Candidate:        Ctrl + Shift + Space
 * - Special Bengali Chars:  Ctrl + Shift + D
 */
class KeyboardState {
public:
    static KeyboardState& getInstance();

    InputMode getMode() const;
    void setMode(InputMode mode);
    void toggleMode();
    const char* getModeString() const;

    // Centralized shortcut definitions
    static constexpr int TOGGLE_VK = 'B';
    static constexpr int CYCLE_VK = 0x20; // VK_SPACE
    static constexpr int SPECIAL_VK = 'D';

private:
    KeyboardState() = default;
    InputMode m_mode = InputMode::ENGLISH;
};
