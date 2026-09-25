#include "native/KeyboardState.h"

KeyboardState& KeyboardState::getInstance() {
    static KeyboardState instance;
    return instance;
}

InputMode KeyboardState::getMode() const {
    return m_mode;
}

void KeyboardState::setMode(InputMode mode) {
    m_mode = mode;
}

void KeyboardState::toggleMode() {
    if (m_mode == InputMode::ENGLISH) {
        m_mode = InputMode::BENGALI;
    } else {
        m_mode = InputMode::ENGLISH;
    }
}

const char* KeyboardState::getModeString() const {
    switch (m_mode) {
        case InputMode::ENGLISH: return "ENGLISH";
        case InputMode::BENGALI: return "BENGALI";
    }
    return "UNKNOWN";
}
