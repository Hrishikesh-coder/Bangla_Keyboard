#include "core/SpecialCharPicker.h"
#include <sstream>

SpecialCharPicker::SpecialCharPicker() {
    m_items = {
        {'1', "ৎ", "Khanda Ta"},
        {'2', "ং", "Anusvara"},
        {'3', "ঃ", "Visarga"},
        {'4', "ঁ", "Chandrabindu"},
        {'5', "ঞ", "Nya"}
    };
}

bool SpecialCharPicker::isActive() const {
    return m_active;
}

void SpecialCharPicker::activate() {
    m_active = true;
}

void SpecialCharPicker::deactivate() {
    m_active = false;
}

std::optional<std::string> SpecialCharPicker::handleKey(int vkCode) {
    if (!m_active) {
        return std::nullopt;
    }

    // Ignore modifiers (Shift, Ctrl) so releasing them doesn't cancel the picker
    if (vkCode == 0x10 || vkCode == 0xA0 || vkCode == 0xA1 || // VK_SHIFT, LSHIFT, RSHIFT
        vkCode == 0x11 || vkCode == 0xA2 || vkCode == 0xA3) { // VK_CONTROL, LCONTROL, RCONTROL
        return std::nullopt;
    }

    // Accept both top-row digits ('1'-'5') and numpad digits (0x61-0x65)
    int digitVal = 0;
    if (vkCode >= '1' && vkCode <= '5') {
        digitVal = vkCode - '0';
    } else if (vkCode >= 0x61 && vkCode <= 0x65) { // VK_NUMPAD1 to VK_NUMPAD5
        digitVal = vkCode - 0x60;
    }

    m_active = false; // Always deactivate after receiving any other key

    if (digitVal >= 1 && digitVal <= 5) {
        return m_items[digitVal - 1].character;
    }

    return std::nullopt;
}

std::string SpecialCharPicker::getMenuDisplay() const {
    std::ostringstream oss;
    oss << "[SPECIAL CHARS] ";
    for (size_t i = 0; i < m_items.size(); ++i) {
        oss << "[" << m_items[i].digit << "] " << m_items[i].character << " (" << m_items[i].name << ")  ";
    }
    return oss.str();
}

const SpecialCharPicker::Item* SpecialCharPicker::getItem(size_t index1Based) const {
    if (index1Based >= 1 && index1Based <= m_items.size()) {
        return &m_items[index1Based - 1];
    }
    return nullptr;
}
