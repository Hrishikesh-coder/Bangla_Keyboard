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
    notifyChanged();
}

void KeyboardState::toggleMode() {
    // Ctrl+Shift+B keeps its original meaning: English on, English off. If the user is
    // currently in the fixed-layout mode, that also counts as "Bengali on".
    if (m_mode == InputMode::ENGLISH) {
        m_mode = InputMode::BENGALI_PHONETIC;
    } else {
        m_mode = InputMode::ENGLISH;
    }
    notifyChanged();
}

void KeyboardState::cycleMode() {
    switch (m_mode) {
        case InputMode::ENGLISH:          m_mode = InputMode::BENGALI_PHONETIC; break;
        case InputMode::BENGALI_PHONETIC: m_mode = InputMode::BENGALI_FIXED;    break;
        case InputMode::BENGALI_FIXED:    m_mode = InputMode::ENGLISH;          break;
    }
    notifyChanged();
}

const char* KeyboardState::getModeString() const {
    switch (m_mode) {
        case InputMode::ENGLISH:          return "ENGLISH";
        case InputMode::BENGALI_PHONETIC: return "BENGALI (phonetic)";
        case InputMode::BENGALI_FIXED:    return "BENGALI (fixed layout)";
    }
    return "UNKNOWN";
}

bool KeyboardState::isBengali() const {
    return m_mode == InputMode::BENGALI_PHONETIC || m_mode == InputMode::BENGALI_FIXED;
}

bool KeyboardState::isLivePreviewEnabled() const {
    return m_livePreview;
}

void KeyboardState::setLivePreview(bool enabled) {
    m_livePreview = enabled;
    notifyChanged();
}

void KeyboardState::toggleLivePreview() {
    m_livePreview = !m_livePreview;
    notifyChanged();
}
