#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

/**
 * @brief The one place the interface's visual language is defined.
 *
 * Every UI surface - the composition popup, the tray menu, the on-screen keyboard - draws
 * from these tokens. Hard-coding a colour or a font anywhere else is how an interface ends
 * up looking assembled rather than designed.
 *
 * Design intent: a dark, low-contrast "glass" card that sits over the user's document
 * without competing with it. An IME overlay is furniture, not content. It appears at the
 * caret, states what it knows, and gets out of the way. The single warm accent is reserved
 * for one job only - marking the candidate currently selected - so that the eye has exactly
 * one thing to track while cycling.
 *
 * Bengali is rendered in Nirmala UI, which ships with Windows 8 and later and does the
 * conjunct shaping and matra reordering properly. Latin chrome uses Segoe UI Variable where
 * available. Both fall back gracefully.
 */
namespace UiTheme {

// --- Colour tokens -----------------------------------------------------------
// Named by role, not by hue, so a future light theme is a change here and nowhere else.

constexpr COLORREF SURFACE        = RGB(0x17, 0x19, 0x1E); ///< Card background
constexpr COLORREF SURFACE_RAISED = RGB(0x21, 0x25, 0x2D); ///< Chips, key caps
constexpr COLORREF SURFACE_SUNKEN = RGB(0x11, 0x13, 0x17); ///< Input strip behind Roman text
constexpr COLORREF BORDER         = RGB(0x2E, 0x34, 0x40); ///< Hairline card edge
constexpr COLORREF BORDER_SUBTLE  = RGB(0x25, 0x2A, 0x34); ///< Dividers

constexpr COLORREF TEXT           = RGB(0xEC, 0xEE, 0xF2); ///< Primary: the Bengali itself
constexpr COLORREF TEXT_MUTED     = RGB(0x8B, 0x94, 0xA6); ///< Roman buffer, hints
constexpr COLORREF TEXT_FAINT     = RGB(0x5A, 0x62, 0x72); ///< Key hints, index digits

constexpr COLORREF ACCENT         = RGB(0xE2, 0xA0, 0x4A); ///< Selected candidate. Used nowhere else.
constexpr COLORREF ACCENT_SOFT    = RGB(0x3A, 0x2F, 0x1E); ///< Selected chip fill
constexpr COLORREF ACCENT_DIM     = RGB(0x8A, 0x67, 0x30); ///< Selected chip edge

constexpr COLORREF MODE_PHONETIC  = RGB(0x62, 0xB0, 0xE8); ///< Mode pip: phonetic
constexpr COLORREF MODE_FIXED     = RGB(0x7E, 0xC6, 0x8A); ///< Mode pip: fixed layout

// --- Metrics (design units at 96 DPI; scale() converts) ----------------------

constexpr int RADIUS        = 10;
constexpr int PAD           = 12;
constexpr int GAP           = 8;
constexpr int CHIP_PAD_X    = 10;
constexpr int CHIP_H        = 30;
constexpr int STRIP_H       = 22;
constexpr int COMPOSE_PT    = 21;  ///< Bengali composition, point size
constexpr int CHIP_PT       = 15;  ///< Candidate chips
constexpr int LABEL_PT      = 9;   ///< Roman buffer and hints
constexpr int KEYCAP_PT     = 15;
constexpr int KEYHINT_PT    = 7;

/// Scales a 96-DPI design unit to the given DPI.
int scale(int value, UINT dpi);

/// DPI of the monitor a window sits on, falling back to the system DPI on older Windows.
UINT dpiForWindow(HWND hwnd);

/// Creates the Bengali text font (Nirmala UI, falling back to any Bengali-capable face).
HFONT createBengaliFont(int pointSize, UINT dpi, int weight = FW_NORMAL);

/// Creates the Latin chrome font (Segoe UI Variable / Segoe UI).
HFONT createUiFont(int pointSize, UINT dpi, int weight = FW_NORMAL);

/// Fills a rounded rectangle, with an optional 1px border. Pass -1 for no border.
void fillRoundRect(HDC hdc, const RECT& rect, int radius, COLORREF fill, COLORREF border = CLR_INVALID);

/// Draws text with the given colour and DrawText flags. Does not set the font.
void drawText(HDC hdc, const std::wstring& text, RECT rect, COLORREF colour, UINT flags);

/// Measures a single line of text with the currently selected font.
SIZE measureText(HDC hdc, const std::wstring& text);

/// UTF-8 to UTF-16. Every engine string crosses this boundary before being drawn.
std::wstring toWide(const std::string& utf8);

/// Rounds the window's corners so the card reads as a floating surface.
void applyRoundedRegion(HWND hwnd, int width, int height, int radius);

} // namespace UiTheme
