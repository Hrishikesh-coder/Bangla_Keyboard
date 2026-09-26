#include "core/InputBuffer.h"

void InputBuffer::append(char c) {
    m_buffer.push_back(c);
}

void InputBuffer::append(const std::string& str) {
    m_buffer.append(str);
}

bool InputBuffer::backspace() {
    if (m_buffer.empty()) {
        return false;
    }
    
    // Remove the last UTF-8 character (which may be multiple bytes)
    while (!m_buffer.empty()) {
        unsigned char c = m_buffer.back();
        m_buffer.pop_back();
        if ((c & 0xC0) != 0x80) {
            // Reached the leading byte of a multi-byte sequence, or an ASCII character
            break;
        }
    }
    return true;
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
