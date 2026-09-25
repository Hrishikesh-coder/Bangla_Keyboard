#include "core/InputBuffer.h"

void InputBuffer::append(char c) {
    m_buffer.push_back(c);
}

void InputBuffer::append(const std::string& str) {
    m_buffer.append(str);
}

bool InputBuffer::backspace() {
    if (!m_buffer.empty()) {
        m_buffer.pop_back();
        return true;
    }
    return false;
}

void InputBuffer::clear() {
    m_buffer.clear();
}

const std::string& InputBuffer::content() const {
    return m_buffer;
}

bool InputBuffer::empty() const {
    return m_buffer.empty();
}

size_t InputBuffer::length() const {
    return m_buffer.length();
}
