#include "ui/OnScreenKeyboard.h"

#include <algorithm>

namespace {

const wchar_t* kClassName = L"ShobdomalaOnScreenKeyboard";

inline int lparamX(LPARAM lp) { return static_cast<int>(static_cast<short>(LOWORD(lp))); }
inline int lparamY(LPARAM lp) { return static_cast<int>(static_cast<short>(HIWORD(lp))); }

/// Physical key rows, base and shifted pairs, exactly as a US keyboard presents them.
/// The board's shape is the keyboard's; only the glyphs printed on it come from the layout.
struct RowSpec {
    const char* base;
    const char* shifted;
};

const RowSpec kRows[] = {
    { "`1234567890-=", "~!@#$%^&*()_+" },
    { "qwertyuiop[]\\", "QWERTYUIOP{}|" },
    { "asdfghjkl;'",    "ASDFGHJKL:\""  },
    { "zxcvbnm,./",     "ZXCVBNM<>?"    }
};

/// Sentinel for the Shift key cap, which is not a layout key.
constexpr char kShiftKey = '\x01';
constexpr char kSpaceKey = ' ';

} // namespace

OnScreenKeyboard::~OnScreenKeyboard() {
    destroy();
}

bool OnScreenKeyboard::create(HINSTANCE instance, const FixedLayoutEngine* layout) {
    m_instance = instance;
    m_layout = layout;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = &OnScreenKeyboard::wndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kClassName, L"Shobdomala keyboard",
        WS_POPUP,
        0, 0, 10, 10,
        nullptr, nullptr, instance, this);

    if (!m_hwnd) {
        return false;
    }

    m_dpi = UiTheme::dpiForWindow(m_hwnd);
    rebuildFonts();
    buildKeys();
    layoutKeys();
    return true;
}

void OnScreenKeyboard::destroy() {
    releaseFonts();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void OnScreenKeyboard::setLayout(const FixedLayoutEngine* layout) {
    m_layout = layout;
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void OnScreenKeyboard::rebuildFonts() {
    releaseFonts();
    m_fontCap = UiTheme::createBengaliFont(UiTheme::KEYCAP_PT, m_dpi);
    m_fontHint = UiTheme::createUiFont(UiTheme::KEYHINT_PT, m_dpi);
    m_fontLabel = UiTheme::createUiFont(UiTheme::LABEL_PT, m_dpi, FW_SEMIBOLD);
}

void OnScreenKeyboard::releaseFonts() {
    if (m_fontCap)   { DeleteObject(m_fontCap);   m_fontCap = nullptr; }
    if (m_fontHint)  { DeleteObject(m_fontHint);  m_fontHint = nullptr; }
    if (m_fontLabel) { DeleteObject(m_fontLabel); m_fontLabel = nullptr; }
}

// ---------------------------------------------------------------------------
// Board construction
// ---------------------------------------------------------------------------

void OnScreenKeyboard::buildKeys() {
    m_rows.clear();

    for (const auto& spec : kRows) {
        std::vector<Key> row;
        const size_t count = strlen(spec.base);
        for (size_t i = 0; i < count; ++i) {
            Key key;
            key.base = spec.base[i];
            key.shifted = (i < strlen(spec.shifted)) ? spec.shifted[i] : spec.base[i];
            row.push_back(key);
        }
        m_rows.push_back(std::move(row));
    }

    // Bottom row: a latching Shift and a space bar. Shift latches rather than requiring a
    // held button because this board is driven by a mouse, which has only one pointer.
    std::vector<Key> bottom;
    Key shift;
    shift.base = kShiftKey;
    shift.shifted = kShiftKey;
    shift.widthUnits = 180;
    bottom.push_back(shift);

    Key space;
    space.base = kSpaceKey;
    space.shifted = kSpaceKey;
    space.widthUnits = 620;
    bottom.push_back(space);

    m_rows.push_back(std::move(bottom));
}

void OnScreenKeyboard::layoutKeys() {
    const int pad     = UiTheme::scale(UiTheme::PAD, m_dpi);
    const int gap     = UiTheme::scale(5, m_dpi);
    const int keyW    = UiTheme::scale(46, m_dpi);
    const int keyH    = UiTheme::scale(46, m_dpi);
    const int headerH = UiTheme::scale(26, m_dpi);

    int widest = 0;
    int y = pad + headerH;

    for (auto& row : m_rows) {
        int x = pad;
        for (auto& key : row) {
            const int width = keyW * key.widthUnits / 100;
            key.rect = RECT{ x, y, x + width, y + keyH };
            x += width + gap;
        }
        widest = std::max(widest, x - gap + pad);
        y += keyH + gap;
    }

    m_width = widest;
    m_height = y - gap + pad;
}

std::string OnScreenKeyboard::glyphFor(const Key& key) const {
    if (!m_layout || key.base == kShiftKey) {
        return std::string();
    }
    if (key.base == kSpaceKey) {
        return std::string(" ");
    }
    const char physical = m_shift ? key.shifted : key.base;
    return m_layout->isMapped(physical) ? m_layout->mapKey(physical) : std::string();
}

std::wstring OnScreenKeyboard::hintFor(const Key& key) const {
    const char physical = m_shift ? key.shifted : key.base;
    wchar_t buffer[2] = { static_cast<wchar_t>(physical), 0 };
    return std::wstring(buffer);
}

int OnScreenKeyboard::hitTest(POINT point) const {
    for (size_t r = 0; r < m_rows.size(); ++r) {
        for (size_t c = 0; c < m_rows[r].size(); ++c) {
            if (PtInRect(&m_rows[r][c].rect, point)) {
                return static_cast<int>(r * 100 + c);
            }
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Visibility
// ---------------------------------------------------------------------------

void OnScreenKeyboard::show() {
    if (!m_hwnd) {
        return;
    }

    // Park the board at the bottom centre of the work area on first show, where it covers
    // the least of a typical document.
    RECT current;
    GetWindowRect(m_hwnd, &current);
    int x = current.left;
    int y = current.top;

    if (!m_visible && x == 0 && y == 0) {
        HMONITOR monitor = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(monitor, &mi)) {
            x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - m_width) / 2;
            y = mi.rcWork.bottom - m_height - UiTheme::scale(24, m_dpi);
        }
    }

    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, m_width, m_height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    UiTheme::applyRoundedRegion(m_hwnd, m_width, m_height,
                                UiTheme::scale(UiTheme::RADIUS, m_dpi));
    m_visible = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void OnScreenKeyboard::hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
    m_visible = false;
}

void OnScreenKeyboard::toggle() {
    m_visible ? hide() : show();
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void OnScreenKeyboard::onPaint() {
    PAINTSTRUCT ps;
    HDC screenDc = BeginPaint(m_hwnd, &ps);

    HDC hdc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, m_width, m_height);
    HGDIOBJ oldBitmap = SelectObject(hdc, bitmap);

    const int pad    = UiTheme::scale(UiTheme::PAD, m_dpi);
    const int radius = UiTheme::scale(UiTheme::RADIUS, m_dpi);
    const int keyR   = UiTheme::scale(6, m_dpi);

    RECT full { 0, 0, m_width, m_height };
    UiTheme::fillRoundRect(hdc, full, radius, UiTheme::SURFACE, UiTheme::BORDER);

    // Header doubles as the drag handle, so it says what the board is and how to move it.
    SelectObject(hdc, m_fontLabel);
    RECT header { pad, pad, m_width - pad, pad + UiTheme::scale(20, m_dpi) };
    const std::wstring title = m_layout
        ? UiTheme::toWide(m_layout->layoutName())
        : std::wstring(L"No layout loaded");
    UiTheme::drawText(hdc, title, header, UiTheme::TEXT_MUTED,
                      DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
    UiTheme::drawText(hdc, L"drag to move", header, UiTheme::TEXT_FAINT,
                      DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);

    for (size_t r = 0; r < m_rows.size(); ++r) {
        for (size_t c = 0; c < m_rows[r].size(); ++c) {
            const Key& key = m_rows[r][c];
            const bool hovered = (static_cast<int>(r) == m_hoverRow &&
                                  static_cast<int>(c) == m_hoverCol);
            const bool isShiftKey = (key.base == kShiftKey);
            const bool latched = isShiftKey && m_shift;

            COLORREF fill = latched  ? UiTheme::ACCENT_SOFT
                          : hovered  ? UiTheme::BORDER_SUBTLE
                                     : UiTheme::SURFACE_RAISED;
            COLORREF edge = latched ? UiTheme::ACCENT_DIM : UiTheme::BORDER_SUBTLE;
            UiTheme::fillRoundRect(hdc, key.rect, keyR, fill, edge);

            if (isShiftKey) {
                SelectObject(hdc, m_fontLabel);
                UiTheme::drawText(hdc, L"Shift", key.rect,
                                  latched ? UiTheme::ACCENT : UiTheme::TEXT_MUTED,
                                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
                continue;
            }
            if (key.base == kSpaceKey) {
                SelectObject(hdc, m_fontLabel);
                UiTheme::drawText(hdc, L"space", key.rect, UiTheme::TEXT_FAINT,
                                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
                continue;
            }

            const std::string glyph = glyphFor(key);

            // The Bengali glyph is the content; the Latin key is the hint that teaches it.
            RECT capRect = key.rect;
            capRect.bottom -= UiTheme::scale(11, m_dpi);
            SelectObject(hdc, m_fontCap);
            UiTheme::drawText(hdc,
                              glyph.empty() ? std::wstring(L"\u00B7") : UiTheme::toWide(glyph),
                              capRect,
                              glyph.empty() ? UiTheme::TEXT_FAINT : UiTheme::TEXT,
                              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX | DT_NOCLIP);

            RECT hintRect = key.rect;
            hintRect.top = key.rect.bottom - UiTheme::scale(14, m_dpi);
            SelectObject(hdc, m_fontHint);
            UiTheme::drawText(hdc, hintFor(key), hintRect, UiTheme::TEXT_FAINT,
                              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
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

LRESULT CALLBACK OnScreenKeyboard::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<OnScreenKeyboard*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            if (self) {
                self->onPaint();
            }
            return 0;

        case WM_NCHITTEST: {
            // Anything that is not a key cap acts as a title bar, so the board can be
            // dragged out of the way without needing a separate caption.
            if (!self) break;
            POINT screen { lparamX(lParam), lparamY(lParam) };
            POINT client = screen;
            ScreenToClient(hwnd, &client);
            return (self->hitTest(client) >= 0) ? HTCLIENT : HTCAPTION;
        }

        case WM_MOUSEMOVE: {
            if (!self) break;
            POINT point { lparamX(lParam), lparamY(lParam) };
            int hit = self->hitTest(point);
            int row = (hit >= 0) ? hit / 100 : -1;
            int col = (hit >= 0) ? hit % 100 : -1;
            if (row != self->m_hoverRow || col != self->m_hoverCol) {
                self->m_hoverRow = row;
                self->m_hoverCol = col;
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
                self->m_hoverRow = -1;
                self->m_hoverCol = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN: {
            if (!self) break;
            POINT point { lparamX(lParam), lparamY(lParam) };
            int hit = self->hitTest(point);
            if (hit < 0) {
                break;
            }
            const Key& key = self->m_rows[hit / 100][hit % 100];

            if (key.base == kShiftKey) {
                self->m_shift = !self->m_shift;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            std::string glyph = self->glyphFor(key);
            if (!glyph.empty() && self->m_onKey) {
                self->m_onKey(glyph);
                // Shift is one-shot, like a real keyboard: it releases after the key it
                // modified, so the user is not left silently latched.
                if (self->m_shift) {
                    self->m_shift = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }

        case WM_DPICHANGED:
            if (self) {
                self->m_dpi = HIWORD(wParam);
                self->rebuildFonts();
                self->layoutKeys();
                SetWindowPos(hwnd, nullptr, 0, 0, self->m_width, self->m_height,
                             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                UiTheme::applyRoundedRegion(hwnd, self->m_width, self->m_height,
                                            UiTheme::scale(UiTheme::RADIUS, self->m_dpi));
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;

        default:
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
