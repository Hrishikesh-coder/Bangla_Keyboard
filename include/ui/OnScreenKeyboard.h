#pragma once

#include "ui/UiTheme.h"
#include "core/FixedLayoutEngine.h"

#include <functional>
#include <string>
#include <vector>

/**
 * @brief A clickable on-screen keyboard showing the active fixed layout.
 *
 * Two jobs, and the second is the more valuable one.
 *
 * Its obvious job is input: a user who does not know where ঞ lives can click it. Like the
 * composition window it never takes focus, so a click injects into whatever the user was
 * already typing in.
 *
 * Its real job is teaching. Every key cap shows the Bengali glyph large and the Latin key
 * that produces it small underneath, so using the mouse gradually teaches the layout and
 * makes itself unnecessary. Holding or latching Shift flips the whole board to the shifted
 * glyphs, which is how the second half of the layout becomes discoverable at all.
 *
 * The board is built from FixedLayoutEngine's key map rather than from a hard-coded
 * picture, so editing config/layout_probhat.json changes what is drawn here too. A layout
 * editor and its keyboard can never disagree if there is only one source of truth.
 */
class OnScreenKeyboard {
public:
    OnScreenKeyboard() = default;
    ~OnScreenKeyboard();

    OnScreenKeyboard(const OnScreenKeyboard&) = delete;
    OnScreenKeyboard& operator=(const OnScreenKeyboard&) = delete;

    bool create(HINSTANCE instance, const FixedLayoutEngine* layout);
    void destroy();

    void show();
    void hide();
    void toggle();
    bool isVisible() const { return m_visible; }

    /// Re-reads the layout, for instance after a layout file is reloaded.
    void setLayout(const FixedLayoutEngine* layout);

    /// Invoked with the UTF-8 glyph when a key cap is clicked.
    void setOnKey(std::function<void(const std::string&)> callback) { m_onKey = std::move(callback); }

    HWND handle() const { return m_hwnd; }

private:
    struct Key {
        char base = 0;      ///< Unshifted character, e.g. 'k'
        char shifted = 0;   ///< Shifted character, e.g. 'K'
        RECT rect{};
        int widthUnits = 100; ///< Hundredths of a standard key width
    };

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void buildKeys();
    void layoutKeys();
    void onPaint();
    int hitTest(POINT point) const;
    std::string glyphFor(const Key& key) const;
    std::wstring hintFor(const Key& key) const;

    void rebuildFonts();
    void releaseFonts();

    HWND m_hwnd = nullptr;
    HINSTANCE m_instance = nullptr;
    const FixedLayoutEngine* m_layout = nullptr;
    UINT m_dpi = 96;

    HFONT m_fontCap = nullptr;
    HFONT m_fontHint = nullptr;
    HFONT m_fontLabel = nullptr;

    std::vector<std::vector<Key>> m_rows;
    int m_width = 0;
    int m_height = 0;
    int m_hoverRow = -1;
    int m_hoverCol = -1;
    bool m_shift = false;
    bool m_altgr = false;
    bool m_visible = false;
    bool m_trackingMouse = false;

    std::function<void(const std::string&)> m_onKey;
};
