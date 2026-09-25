#pragma once

#include "core/PhoneticEngine.h"
#include "core/InputBuffer.h"
#include "core/SpecialCharPicker.h"
#include "native/KeyboardState.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/**
 * @brief Manages installation and event handling of the Windows WH_KEYBOARD_LL hook.
 *
 * Implements low-level keyboard interception, mode toggling, candidate cycling,
 * special character selection, and injection coordination.
 */
class KeyboardHook {
public:
    KeyboardHook() = default;
    ~KeyboardHook();

    /// Installs the WH_KEYBOARD_LL hook. Returns true on success.
    bool install(PhoneticEngine* engine);

    /// Uninstalls the active hook.
    void uninstall();

    /// Runs the message pump required by the hook installation thread.
    void runMessageLoop();

    /// Posts WM_QUIT to terminate the message loop.
    void stopMessageLoop();

    /// Returns whether the hook is currently installed.
    bool isInstalled() const;

private:
    static LRESULT CALLBACK hookCallback(int nCode, WPARAM wParam, LPARAM lParam);

    static HHOOK s_hHook;
    static PhoneticEngine* s_engine;
    static InputBuffer s_buffer;
    static SpecialCharPicker s_specialPicker;
    static DWORD s_threadId;
};
