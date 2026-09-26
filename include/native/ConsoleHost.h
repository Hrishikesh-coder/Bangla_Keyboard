#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/**
 * @brief Gives a GUI-subsystem application a console when, and only when, it needs one.
 *
 * Shobdomala is now linked as a Windows GUI application, which is correct: an input method
 * has no business flashing a console window onto the desktop every time it starts. But a
 * GUI-subsystem process gets no console at all, and this codebase leans on one in two
 * places that matter:
 *
 *   - roughly thirty std::cout traces through the hook and the engine, which are the
 *     project's main teaching surface and how anyone debugs a transliteration;
 *   - `--test`, the offline REPL, which reads from std::cin and is unusable without one.
 *
 * Without this class the GUI build silently swallows all of it: std::cout writes to a
 * closed handle and std::getline returns EOF immediately, so the demo mode appears to exit
 * on startup for no visible reason.
 *
 * Policy:
 *   - AttachConsole(ATTACH_PARENT_PROCESS) first. If the user launched us from cmd or
 *     PowerShell, their existing window is the right place for output. Allocating a second
 *     one would be worse than useless.
 *   - AllocConsole only when there is no parent console and output was explicitly asked
 *     for (`--test` or `--verbose`).
 *   - Otherwise no console at all: the tray icon and the caret overlay are the interface.
 */
class ConsoleHost {
public:
    ConsoleHost() = default;
    ~ConsoleHost();

    ConsoleHost(const ConsoleHost&) = delete;
    ConsoleHost& operator=(const ConsoleHost&) = delete;

    /**
     * @brief Attaches to the parent console, or creates one if `allocateIfMissing`.
     * @return True when stdout is now connected to something.
     */
    bool attach(bool allocateIfMissing);

    /// True when this object created the console (as opposed to borrowing the parent's).
    bool owned() const { return m_owned; }

    /// True when stdout is connected.
    bool active() const { return m_active; }

    /**
     * @brief Holds an owned console open until the user presses a key.
     *
     * A console we allocated dies with the process, taking any final message with it. Only
     * meaningful when owned() is true.
     */
    void waitBeforeClosing() const;

private:
    /// Repoints std::cout / std::cerr / std::cin at the console device.
    void bindStandardStreams();

    bool m_active = false;
    bool m_owned = false;
};
