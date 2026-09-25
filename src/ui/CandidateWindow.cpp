#include "ui/CandidateWindow.h"

#include <algorithm>

namespace {
const wchar_t* kClassName = L"ShobdomalaCandidateWindow";
constexpr int kMaxChips = 9;

/// Mouse coordinates in an LPARAM are signed 16-bit; a plain LOWORD goes wrong on a
/// multi-monitor desktop with negative coordinates.
inline int lparamX(LPARAM lp) { return static_cast<int>(static_cast<short>(LOWORD(lp))); }
inline int lparamY(LPARAM lp) { return static_cast<int>(static_cast<short>(HIWORD(lp))); }
} // namespace

CandidateWindow::~CandidateWindow() {
    destroy();
}

bool CandidateWindow::create(HINSTANCE instance) {
    m_instance = instance;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    // CS_DROPSHADOW gives the card a real system shadow, which is what separates it from
    // the document underneath without needing a heavy border.
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = &CandidateWindow::wndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // fully custom painted
    wc.lpszClassName = kClassName;

    // Re-registering the class is harmless if another instance already did it.
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kClassName, L"Shobdomala",
        WS_POPUP,
        0, 0, 10, 10,
        nullptr, nullptr, instance, this);

    if (!m_hwnd) {
        return false;
    }

    m_dpi = UiTheme::dpiForWindow(m_hwnd);
    rebuildFonts();
    return true;
}

void CandidateWindow::destroy() {
    releaseFonts();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void CandidateWindow::rebuildFonts() {
    releaseFonts();
    m_fontCompose = UiTheme::createBengaliFont(UiTheme::COMPOSE_PT, m_dpi);
    m_fontChip = UiTheme::createBengaliFont(UiTheme::CHIP_PT, m_dpi);
    m_fontLabel = UiTheme::createUiFont(UiTheme::LABEL_PT, m_dpi, FW_SEMIBOLD);
}

void CandidateWindow::releaseFonts() {
    if (m_fontCompose) { DeleteObject(m_fontCompose); m_fontCompose = nullptr; }
    if (m_fontChip)    { DeleteObject(m_fontChip);    m_fontChip = nullptr; }
    if (m_fontLabel)   { DeleteObject(m_fontLabel);   m_fontLabel = nullptr; }
}

// ---------------------------------------------------------------------------
// Caret tracking
// ---------------------------------------------------------------------------

POINT CandidateWindow::caretScreenPosition() {
    POINT result = {0, 0};

    HWND foreground = GetForegroundWindow();
    if (foreground) {
        GUITHREADINFO info = {};
        info.cbSize = sizeof(info);
        DWORD threadId = GetWindowThreadProcessId(foreground, nullptr);

        // Ask the *foreground* thread where its caret is. Our own thread has no caret, so
        // GetCaretPos here would be meaningless.
        if (GetGUIThreadInfo(threadId, &info) && info.hwndCaret &&
            (info.rcCaret.bottom - info.rcCaret.top) > 0) {
            result.x = info.rcCaret.left;
            result.y = info.rcCaret.bottom;
            ClientToScreen(info.hwndCaret, &result);
            return result;
        }

        // No caret reported (many web views and custom editors): anchor to the window.
        RECT windowRect;
        if (GetWindowRect(foreground, &windowRect)) {
            result.x = windowRect.left + 24;
            result.y = windowRect.top + 96;
            return result;
        }
    }

    GetCursorPos(&result);
    return result;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void CandidateWindow::layout() {
    m_chipRects.clear();

    const int pad    = UiTheme::scale(UiTheme::PAD, m_dpi);
    const int gap    = UiTheme::scale(UiTheme::GAP, m_dpi);
    const int chipH  = UiTheme::scale(UiTheme::CHIP_H, m_dpi);
    const int chipPX = UiTheme::scale(UiTheme::CHIP_PAD_X, m_dpi);
    const int stripH = UiTheme::scale(UiTheme::STRIP_H, m_dpi);

    HDC hdc = GetDC(m_hwnd);

    // --- measure the header strip (mode pip + Roman buffer)
    SelectObject(hdc, m_fontLabel);
    const std::wstring header = UiTheme::toWide(m_content.modeLabel) + L"   " +
                                UiTheme::toWide(m_content.roman);
    SIZE headerSize = UiTheme::measureText(hdc, header);

    // --- measure the composed Bengali
    SelectObject(hdc, m_fontCompose);
    const std::wstring composed = UiTheme::toWide(m_content.composed);
    SIZE composedSize = UiTheme::measureText(hdc, composed.empty() ? L" " : composed);

    // --- measure the candidate chips
    SelectObject(hdc, m_fontChip);
    const size_t chipCount = std::min<size_t>(m_content.candidates.size(), kMaxChips);
    int chipsWidth = 0;
    std::vector<int> chipWidths;
    chipWidths.reserve(chipCount);

    for (size_t i = 0; i < chipCount; ++i) {
        // An empty candidate is the inherent vowel, which has no glyph. Showing an empty
        // chip would be indistinguishable from a rendering bug, so it is labelled.
        std::wstring text = m_content.candidates[i].empty()
                                ? std::wstring(L"\u09A0\u09BE\u0981")  // placeholder metrics
                                : UiTheme::toWide(m_content.candidates[i]);
        SIZE size = UiTheme::measureText(hdc, text);
        int width = size.cx + chipPX * 2;
        chipWidths.push_back(width);
        chipsWidth += width + (i + 1 < chipCount ? gap : 0);
    }

    ReleaseDC(m_hwnd, hdc);

    const int contentWidth = std::max({ static_cast<int>(headerSize.cx),
                                        static_cast<int>(composedSize.cx),
                                        chipsWidth });
    const int minWidth = UiTheme::scale(180, m_dpi);

    m_width = std::max(minWidth, contentWidth + pad * 2);
    m_height = pad + stripH + composedSize.cy + pad;
    if (chipCount > 0) {
        m_height += gap + chipH + UiTheme::scale(4, m_dpi);
    }

    // --- place the chips now that the width is known
    if (chipCount > 0) {
        int x = pad;
        const int y = pad + stripH + composedSize.cy + gap + UiTheme::scale(4, m_dpi);
        for (size_t i = 0; i < chipCount; ++i) {
            RECT rect { x, y, x + chipWidths[i], y + chipH };
            m_chipRects.push_back(rect);
            x += chipWidths[i] + gap;
        }
    }
}

void CandidateWindow::positionAtCaret() {
    POINT caret = caretScreenPosition();

    const int offsetY = UiTheme::scale(6, m_dpi);
    int x = caret.x;
    int y = caret.y + offsetY;

    // Keep the whole card on the monitor the caret is on, and flip above the caret rather
    // than letting it hang off the bottom of the screen.
    HMONITOR monitor = MonitorFromPoint(caret, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(monitor, &mi)) {
        const RECT& work = mi.rcWork;
        if (x + m_width > work.right)  x = work.right - m_width - UiTheme::scale(8, m_dpi);
        if (x < work.left)             x = work.left + UiTheme::scale(8, m_dpi);
        if (y + m_height > work.bottom) {
            // Flip above: the caret's own line height is unknown, so back off by the card
            // height plus a line's worth of clearance.
            y = caret.y - m_height - offsetY - UiTheme::scale(18, m_dpi);
        }
        if (y < work.top) y = work.top + UiTheme::scale(8, m_dpi);
    }

    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, m_width, m_height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    UiTheme::applyRoundedRegion(m_hwnd, m_width, m_height,
                                UiTheme::scale(UiTheme::RADIUS, m_dpi));
}

// ---------------------------------------------------------------------------
// Update / visibility
// ---------------------------------------------------------------------------

void CandidateWindow::update(const Content& content) {
    if (!m_hwnd) {
        return;
    }

    if (content.roman.empty() && content.composed.empty()) {
        hide();
        return;
    }

    m_content = content;
    m_hoverChip = -1;

    UINT dpi = UiTheme::dpiForWindow(m_hwnd);
    if (dpi != m_dpi) {
        m_dpi = dpi;
        rebuildFonts();
    }

    layout();
    positionAtCaret();
    m_visible = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
    UpdateWindow(m_hwnd);
}

void CandidateWindow::hide() {
    if (m_hwnd && m_visible) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
    m_visible = false;
    m_hoverChip = -1;
}

bool CandidateWindow::isVisible() const {
    return m_visible;
}

int CandidateWindow::hitTestChip(POINT point) const {
    for (size_t i = 0; i < m_chipRects.size(); ++i) {
        if (PtInRect(&m_chipRects[i], point)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void CandidateWindow::onPaint() {
    PAINTSTRUCT ps;
    HDC screenDc = BeginPaint(m_hwnd, &ps);

    // Double buffer: this window repaints on every keystroke, and painting directly to the
    // screen DC would flicker visibly while typing.
    HDC hdc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, m_width, m_height);
    HGDIOBJ oldBitmap = SelectObject(hdc, bitmap);

    const int pad    = UiTheme::scale(UiTheme::PAD, m_dpi);
    const int gap    = UiTheme::scale(UiTheme::GAP, m_dpi);
    const int stripH = UiTheme::scale(UiTheme::STRIP_H, m_dpi);
    const int radius = UiTheme::scale(UiTheme::RADIUS, m_dpi);

    RECT full { 0, 0, m_width, m_height };
    UiTheme::fillRoundRect(hdc, full, radius, UiTheme::SURFACE, UiTheme::BORDER);

    // --- header: mode pip, mode label, Roman buffer ---------------------------
    SelectObject(hdc, m_fontLabel);

    const int pipSize = UiTheme::scale(6, m_dpi);
    const int pipY = pad + (stripH - pipSize) / 2;
    RECT pip { pad, pipY, pad + pipSize, pipY + pipSize };
    UiTheme::fillRoundRect(hdc, pip, pipSize / 2,
                           m_content.fixedMode ? UiTheme::MODE_FIXED : UiTheme::MODE_PHONETIC);

    int textX = pad + pipSize + UiTheme::scale(7, m_dpi);
    RECT modeRect { textX, pad, m_width - pad, pad + stripH };
    const std::wstring modeLabel = UiTheme::toWide(m_content.modeLabel);
    UiTheme::drawText(hdc, modeLabel, modeRect, UiTheme::TEXT_FAINT,
                      DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);

    SIZE modeSize = UiTheme::measureText(hdc, modeLabel);
    RECT romanRect { textX + modeSize.cx + UiTheme::scale(10, m_dpi), pad,
                     m_width - pad, pad + stripH };
    UiTheme::drawText(hdc, UiTheme::toWide(m_content.roman), romanRect, UiTheme::TEXT_MUTED,
                      DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);

    // --- the composed Bengali -------------------------------------------------
    SelectObject(hdc, m_fontCompose);
    const std::wstring composed = UiTheme::toWide(m_content.composed);
    SIZE composedSize = UiTheme::measureText(hdc, composed.empty() ? L" " : composed);

    RECT composeRect { pad, pad + stripH, m_width - pad, pad + stripH + composedSize.cy };
    UiTheme::drawText(hdc, composed, composeRect, UiTheme::TEXT,
                      DT_SINGLELINE | DT_LEFT | DT_NOPREFIX | DT_NOCLIP);

    // --- candidate chips ------------------------------------------------------
    if (!m_chipRects.empty()) {
        RECT divider { pad, composeRect.bottom + gap / 2,
                       m_width - pad, composeRect.bottom + gap / 2 + 1 };
        UiTheme::fillRoundRect(hdc, divider, 0, UiTheme::BORDER_SUBTLE);

        for (size_t i = 0; i < m_chipRects.size(); ++i) {
            const bool selected = (i == m_content.selectedIndex);
            const bool hovered = (static_cast<int>(i) == m_hoverChip);

            COLORREF fill = selected ? UiTheme::ACCENT_SOFT
                                     : (hovered ? UiTheme::BORDER_SUBTLE : UiTheme::SURFACE_RAISED);
            COLORREF edge = selected ? UiTheme::ACCENT_DIM : UiTheme::BORDER_SUBTLE;

            UiTheme::fillRoundRect(hdc, m_chipRects[i], UiTheme::scale(7, m_dpi), fill, edge);

            SelectObject(hdc, m_fontChip);
            const bool isEpsilon = m_content.candidates[i].empty();
            const std::wstring label = isEpsilon
                                           ? std::wstring(L"\u2014")   // em dash: inherent vowel
                                           : UiTheme::toWide(m_content.candidates[i]);
            COLORREF textColour = selected ? UiTheme::ACCENT
                                           : (isEpsilon ? UiTheme::TEXT_FAINT : UiTheme::TEXT);
            UiTheme::drawText(hdc, label, m_chipRects[i], textColour,
                              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX | DT_NOCLIP);
        }
    }

    BitBlt(screenDc, 0, 0, m_width, m_height, hdc, 0, 0, SRCCOPY);

    SelectObject(hdc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    EndPaint(m_hwnd, &ps);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

LRESULT CALLBACK CandidateWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        // Never take focus, whatever the user clicks. This is what lets the target
        // application keep its caret while our chips remain clickable.
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_ERASEBKGND:
            return 1; // fully custom painted; erasing would only cause flicker

        case WM_PAINT:
            if (self) {
                self->onPaint();
            }
            return 0;

        case WM_MOUSEMOVE: {
            if (!self) break;
            POINT point { lparamX(lParam), lparamY(lParam) };
            int chip = self->hitTestChip(point);
            if (chip != self->m_hoverChip) {
                self->m_hoverChip = chip;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (!self->m_trackingMouse) {
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);
                self->m_trackingMouse = true;
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (self) {
                self->m_trackingMouse = false;
                if (self->m_hoverChip != -1) {
                    self->m_hoverChip = -1;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;

        case WM_LBUTTONDOWN: {
            if (!self) break;
            POINT point { lparamX(lParam), lparamY(lParam) };
            int chip = self->hitTestChip(point);
            if (chip >= 0 && self->m_onSelect) {
                self->m_onSelect(static_cast<size_t>(chip));
            }
            return 0;
        }

        case WM_DPICHANGED:
            if (self) {
                self->m_dpi = HIWORD(wParam);
                self->rebuildFonts();
                self->layout();
                self->positionAtCaret();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;

        default:
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
