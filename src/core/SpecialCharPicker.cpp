#include "core/SpecialCharPicker.h"
#include <sstream>

SpecialCharPicker::SpecialCharPicker() {
    m_items = {
        {'1', "ৎ", "Khanda Ta"},
        {'2', "ং", "Anusvara"},
        {'3', "ঃ", "Visarga"},
        {'4', "ঁ", "Chandrabindu"},
        {'5', "ঞ", "Nya"},
        // Word-final hasant. 64 distinct words in the corpus end in one -- বাহ্, আল্লাহ্,
        // দুঃখ্ -- and none of them were typable at all: every phonetic rule emits a
        // consonant, and the composer only ever inserts a hasant *between* two of them.
        // The obvious key would be Avro's ,, but the hook only captures A-Z into the
        // buffer, so punctuation never reaches the engine. The picker already exists for
        // exactly this class of character, so it goes here rather than growing a new
        // mechanism.
        {'6', "\u09CD", "Hasant"}
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

    // Accept both top-row and numpad digits. Bounds come from m_items rather than being
    // written out, so adding an entry to the table is the only edit an extension needs.
    const int count = static_cast<int>(m_items.size());
    int digitVal = 0;
    if (vkCode >= '1' && vkCode <= '9') {
        digitVal = vkCode - '0';
    } else if (vkCode >= 0x61 && vkCode <= 0x69) { // VK_NUMPAD1 to VK_NUMPAD9
        digitVal = vkCode - 0x60;
    }

    m_active = false; // Always deactivate after receiving any other key

    if (digitVal >= 1 && digitVal <= count) {
        return m_items[static_cast<size_t>(digitVal) - 1].character;
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
