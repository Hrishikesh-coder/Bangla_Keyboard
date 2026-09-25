#pragma once

#include "ui/UiTheme.h"

#include <functional>
#include <string>
#include <vector>

/**
 * @brief The floating composition window that follows the text caret.
 *
 * This closes the prototype's first documented limitation: previews were printed to the
 * console, so the user had to look away from what they were typing to see what the engine
 * thought they meant. A real IME shows its state where the eye already is.
 *
 * What it shows, top to bottom:
 *   - a mode pip and the raw Roman buffer, so the user can see what was actually captured;
 *   - the composed Bengali at reading size - the thing being decided;
 *   - candidate chips for the ambiguous token, the selected one carrying the single accent
 *     colour in the whole interface.
 *
 * Three Win32 details make this behave like a system IME rather than an ordinary window:
 *
 *   - WS_EX_NOACTIVATE means it never takes focus, so the application underneath keeps its
 *     caret and its selection while we draw on top of it.
 *   - WS_EX_TOOLWINDOW keeps it out of the taskbar and out of Alt+Tab.
 *   - The caret position comes from GetGUIThreadInfo on the *foreground* thread. A
 *     low-level hook runs on our thread, which has no idea where the target application's
 *     caret is; GetCaretPos would report ours.
 *
 * Chips are clickable. Because the window never activates, a click arrives as an ordinary
 * WM_LBUTTONDOWN without disturbing the focused application, which is what makes
 * mouse selection safe here.
 */
class CandidateWindow {
public:
    /// Everything the window needs for one repaint. Passed by value; it is small.
    struct Content {
        std::string roman;                     ///< Raw Roman buffer as captured
        std::string composed;                  ///< Current composed Bengali
        std::vector<std::string> candidates;   ///< Options for the ambiguous token
        size_t selectedIndex = 0;              ///< Index into `candidates`
        std::string modeLabel;                 ///< "PHONETIC" / "FIXED LAYOUT"
        bool fixedMode = false;                ///< Selects the mode pip colour
    };

    CandidateWindow() = default;
    ~CandidateWindow();

    CandidateWindow(const CandidateWindow&) = delete;
    CandidateWindow& operator=(const CandidateWindow&) = delete;

    bool create(HINSTANCE instance);
    void destroy();

    /// Repositions at the caret, resizes to fit, repaints and shows. Empty roman hides it.
    void update(const Content& content);

    void hide();
    bool isVisible() const;

    /// Invoked when the user clicks a candidate chip, with that chip's index.
    void setOnSelect(std::function<void(size_t)> callback) { m_onSelect = std::move(callback); }

    HWND handle() const { return m_hwnd; }

    /**
     * @brief Screen position of the caret in whichever application currently has focus.
     *
     * Falls back to the foreground window's top-left, then the cursor, so the window still
     * lands somewhere sensible in applications that do not report a caret.
     */
    static POINT caretScreenPosition();

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onPaint();
    void rebuildFonts();
    void releaseFonts();

    /// Computes the window size and the chip rectangles for the current content.
    void layout();

    /// Places the window near the caret, flipping and clamping to stay on screen.
    void positionAtCaret();

    int hitTestChip(POINT clientPoint) const;

    HWND m_hwnd = nullptr;
    HINSTANCE m_instance = nullptr;
    UINT m_dpi = 96;

    HFONT m_fontCompose = nullptr;
    HFONT m_fontChip = nullptr;
    HFONT m_fontLabel = nullptr;

    Content m_content;
    std::vector<RECT> m_chipRects;
    int m_width = 0;
    int m_height = 0;
    int m_hoverChip = -1;
    bool m_visible = false;
    bool m_trackingMouse = false;

    std::function<void(size_t)> m_onSelect;
};
