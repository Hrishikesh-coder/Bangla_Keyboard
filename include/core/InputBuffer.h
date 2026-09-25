#pragma once

#include <string>

/**
 * @brief Accumulates Roman characters typed while Bengali mode is active.
 *
 * Provides a clean buffer management interface for appending keystrokes,
 * handling backspaces, and clearing on word-flush boundaries.
 */
class InputBuffer {
public:
    InputBuffer() = default;

    /// Appends a single Roman character to the buffer.
    void append(char c);

    /// Appends a string to the buffer.
    void append(const std::string& str);

    /// Removes the last character from the buffer. Returns true if a char was popped.
    bool backspace();

    /// Clears the contents of the buffer.
    void clear();

    /// Returns the current raw string stored in the buffer.
    const std::string& content() const;

    /// Checks if the buffer is empty.
    bool empty() const;

    /// Returns the number of characters currently in the buffer.
    size_t length() const;

private:
    std::string m_buffer;
};
