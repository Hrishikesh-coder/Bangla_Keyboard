#pragma once

#include <string>
#include <vector>
#include <optional>

/**
 * @brief Manages the direct selection of 5 special Bengali characters:
 * 1: ৎ (Khanda Ta)
 * 2: ং (Anusvara)
 * 3: ঃ (Visarga)
 * 4: ঁ (Chandrabindu)
 * 5: ঞ (Nya)
 *
 * Triggered via Ctrl+Shift+D. Intercepts the subsequent digit keystroke 1-5
 * to inject the chosen character, or cancels if another key is pressed.
 */
class SpecialCharPicker {
public:
    struct Item {
        char digit;
        std::string character;
        std::string name;
    };

    SpecialCharPicker();

    /// Returns whether the picker is currently awaiting a digit selection.
    bool isActive() const;

    /// Activates the picker.
    void activate();

    /// Cancels and deactivates the picker.
    void deactivate();

    /**
     * @brief Processes a key event while active.
     * @param vkCode Windows virtual key code or ASCII character.
     * @return The selected Bengali UTF-8 character string if a valid digit 1-5 was pressed;
     *         std::nullopt if canceled or invalid key.
     */
    std::optional<std::string> handleKey(int vkCode);

    /// Formats a user-friendly console display of the available numbered items.
    std::string getMenuDisplay() const;

    /// Direct lookup of item by 1-based index (1 to 5).
    const Item* getItem(size_t index1Based) const;

private:
    bool m_active = false;
    std::vector<Item> m_items;
};
