#pragma once

#include "ui/UiTheme.h"
#include "native/KeyboardState.h"

#include <functional>
#include <string>

/**
 * @brief Notification-area icon: the application's only persistent visible surface.
 *
 * A system-wide IME has no main window - it has no business owning one - so the tray is
 * where mode lives. Three affordances, in order of how often they are used:
 *
 *   - the icon itself shows the current mode at a glance, drawn at runtime rather than
 *     shipped as a resource so it stays legible against both light and dark taskbars;
 *   - left click toggles English/Bengali, which is the action taken hundreds of times a day;
 *   - right click opens the full menu for everything else.
 *
 * The icon is drawn as a letter on a coloured disc: "A" for English, "অ" for phonetic,
 * "ক" for the fixed layout. Shape and colour both carry the state, so it stays readable
 * for a colour-blind user and at 16x16.
 */
class TrayIcon {
public:
    /// Commands the menu can raise, delivered through the callback.
    enum class Command {
        ToggleMode,
        SetEnglish,
        SetPhonetic,
        SetFixedLayout,
        ToggleLivePreview,
        ToggleOnScreenKeyboard,
        ShowHelp,
        Exit
    };

    TrayIcon() = default;
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    /// Creates the hidden message window and adds the icon. Returns false on failure.
    bool create(HINSTANCE instance);
    void destroy();

    void setOnCommand(std::function<void(Command)> callback) { m_onCommand = std::move(callback); }

    /// Redraws the icon and refreshes the tooltip for the current state.
    void refresh(InputMode mode, bool livePreview, bool keyboardVisible);

    /// Shows a short balloon notification. Used sparingly - only for mode changes.
    void notify(const std::wstring& title, const std::wstring& message);

    HWND handle() const { return m_hwnd; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void showMenu();

    /// Renders the tray icon bitmap for a mode at the current small-icon size.
    HICON renderIcon(InputMode mode) const;

    HWND m_hwnd = nullptr;
    HINSTANCE m_instance = nullptr;
    HICON m_icon = nullptr;
    UINT m_callbackMessage = 0;
    UINT m_taskbarCreatedMessage = 0;
    bool m_added = false;

    InputMode m_mode = InputMode::ENGLISH;
    bool m_livePreview = true;
    bool m_keyboardVisible = false;

    std::function<void(Command)> m_onCommand;
};
