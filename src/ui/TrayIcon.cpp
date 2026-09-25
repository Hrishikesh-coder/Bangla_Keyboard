#include "ui/TrayIcon.h"

#include <shellapi.h>

namespace {

const wchar_t* kClassName = L"ShobdomalaTrayWindow";
constexpr UINT kCallbackMessage = WM_APP + 1;
constexpr UINT kIconId = 1;

enum MenuId : UINT {
    ID_ENGLISH = 100,
    ID_PHONETIC,
    ID_FIXED,
    ID_LIVE_PREVIEW,
    ID_ONSCREEN_KB,
    ID_HELP,
    ID_EXIT
};

} // namespace

TrayIcon::~TrayIcon() {
    destroy();
}

bool TrayIcon::create(HINSTANCE instance) {
    m_instance = instance;
    m_callbackMessage = kCallbackMessage;

    // Explorer can restart. When it does it broadcasts TaskbarCreated, and every tray icon
    // must re-add itself or it silently disappears for the rest of the session.
    m_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &TrayIcon::wndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    // HWND_MESSAGE: a message-only window. It has no visual presence at all, which is
    // exactly right for something whose entire UI lives in the notification area.
    m_hwnd = CreateWindowExW(0, kClassName, L"Shobdomala", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, nullptr, instance, this);
    if (!m_hwnd) {
        return false;
    }

    m_icon = renderIcon(m_mode);

    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = m_hwnd;
    data.uID = kIconId;
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    data.uCallbackMessage = m_callbackMessage;
    data.hIcon = m_icon;
    wcscpy_s(data.szTip, L"Shobdomala - English");

    m_added = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    return m_added;
}

void TrayIcon::destroy() {
    if (m_added && m_hwnd) {
        NOTIFYICONDATAW data = {};
        data.cbSize = sizeof(data);
        data.hWnd = m_hwnd;
        data.uID = kIconId;
        Shell_NotifyIconW(NIM_DELETE, &data);
        m_added = false;
    }
    if (m_icon) {
        DestroyIcon(m_icon);
        m_icon = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Icon rendering
// ---------------------------------------------------------------------------

HICON TrayIcon::renderIcon(InputMode mode) const {
    const int size = GetSystemMetrics(SM_CXSMICON);

    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);

    // A 32-bit DIB section gives us a real alpha channel, so the disc can be antialiased
    // against whatever taskbar colour the user has rather than sitting on a grey square.
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size; // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP colourBitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBitmap = SelectObject(memory, colourBitmap);

    COLORREF discColour;
    const wchar_t* glyph;
    switch (mode) {
        case InputMode::BENGALI_PHONETIC:
            discColour = UiTheme::MODE_PHONETIC;
            glyph = L"\u0985"; // অ
            break;
        case InputMode::BENGALI_FIXED:
            discColour = UiTheme::MODE_FIXED;
            glyph = L"\u0995"; // ক
            break;
        default:
            discColour = RGB(0x6B, 0x74, 0x86);
            glyph = L"A";
            break;
    }

    RECT full { 0, 0, size, size };
    HBRUSH disc = CreateSolidBrush(discColour);
    HGDIOBJ oldBrush = SelectObject(memory, disc);
    HGDIOBJ oldPen = SelectObject(memory, GetStockObject(NULL_PEN));
    Ellipse(memory, 0, 0, size + 1, size + 1);
    SelectObject(memory, oldBrush);
    SelectObject(memory, oldPen);
    DeleteObject(disc);

    HFONT font = (mode == InputMode::ENGLISH)
                     ? UiTheme::createUiFont(size * 5 / 12, 96, FW_BOLD)
                     : UiTheme::createBengaliFont(size * 6 / 12, 96, FW_SEMIBOLD);
    HGDIOBJ oldFont = SelectObject(memory, font);
    SetBkMode(memory, TRANSPARENT);
    SetTextColor(memory, RGB(0x0E, 0x10, 0x14));
    DrawTextW(memory, glyph, -1, &full,
              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX | DT_NOCLIP);
    SelectObject(memory, oldFont);
    DeleteObject(font);

    // GDI text drawing zeroes the alpha byte of every pixel it touches. Force every pixel
    // inside the disc back to opaque, or the glyph appears as a transparent hole.
    auto* pixels = static_cast<unsigned char*>(bits);
    const double centre = (size - 1) / 2.0;
    const double radius = size / 2.0;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const double dx = x - centre;
            const double dy = y - centre;
            const bool inside = (dx * dx + dy * dy) <= (radius * radius);
            pixels[(y * size + x) * 4 + 3] = inside ? 255 : 0;
        }
    }

    SelectObject(memory, oldBitmap);

    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO iconInfo = {};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmMask = mask;
    iconInfo.hbmColor = colourBitmap;
    HICON icon = CreateIconIndirect(&iconInfo);

    DeleteObject(mask);
    DeleteObject(colourBitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);

    return icon;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void TrayIcon::refresh(InputMode mode, bool livePreview, bool keyboardVisible) {
    m_mode = mode;
    m_livePreview = livePreview;
    m_keyboardVisible = keyboardVisible;

    if (!m_added) {
        return;
    }

    HICON updated = renderIcon(mode);

    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = m_hwnd;
    data.uID = kIconId;
    data.uFlags = NIF_ICON | NIF_TIP;
    data.hIcon = updated;

    const wchar_t* modeText =
        (mode == InputMode::BENGALI_PHONETIC) ? L"Bengali (phonetic)"
        : (mode == InputMode::BENGALI_FIXED)  ? L"Bengali (fixed layout)"
                                              : L"English";
    swprintf_s(data.szTip, L"Shobdomala - %s\nCtrl+Shift+B to switch", modeText);

    Shell_NotifyIconW(NIM_MODIFY, &data);

    if (m_icon) {
        DestroyIcon(m_icon);
    }
    m_icon = updated;
}

void TrayIcon::notify(const std::wstring& title, const std::wstring& message) {
    if (!m_added) {
        return;
    }
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = m_hwnd;
    data.uID = kIconId;
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_NONE;
    wcsncpy_s(data.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(data.szInfo, message.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------

void TrayIcon::showMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    // Modes are mutually exclusive, so they are radio items rather than checkboxes. The
    // distinction is not cosmetic: it tells the user only one can be active.
    AppendMenuW(menu, MF_STRING, ID_ENGLISH,  L"English");
    AppendMenuW(menu, MF_STRING, ID_PHONETIC, L"Bengali \u2014 phonetic\tCtrl+Shift+B");
    AppendMenuW(menu, MF_STRING, ID_FIXED,    L"Bengali \u2014 fixed layout\tCtrl+Shift+L");

    UINT selected = (m_mode == InputMode::BENGALI_PHONETIC) ? ID_PHONETIC
                  : (m_mode == InputMode::BENGALI_FIXED)    ? ID_FIXED
                                                            : ID_ENGLISH;
    CheckMenuRadioItem(menu, ID_ENGLISH, ID_FIXED, selected, MF_BYCOMMAND);

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (m_livePreview ? MF_CHECKED : 0),
                ID_LIVE_PREVIEW, L"Live preview\tCtrl+Shift+P");
    AppendMenuW(menu, MF_STRING | (m_keyboardVisible ? MF_CHECKED : 0),
                ID_ONSCREEN_KB, L"On-screen keyboard");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_HELP, L"Shortcuts and typing guide");
    AppendMenuW(menu, MF_STRING, ID_EXIT, L"Exit Shobdomala");

    POINT cursor;
    GetCursorPos(&cursor);

    // Required, and easy to miss: without this the menu will not dismiss when the user
    // clicks elsewhere, because our window is message-only and never takes focus.
    SetForegroundWindow(m_hwnd);

    UINT command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                  cursor.x, cursor.y, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    if (!m_onCommand) {
        return;
    }

    switch (command) {
        case ID_ENGLISH:       m_onCommand(Command::SetEnglish); break;
        case ID_PHONETIC:      m_onCommand(Command::SetPhonetic); break;
        case ID_FIXED:         m_onCommand(Command::SetFixedLayout); break;
        case ID_LIVE_PREVIEW:  m_onCommand(Command::ToggleLivePreview); break;
        case ID_ONSCREEN_KB:   m_onCommand(Command::ToggleOnScreenKeyboard); break;
        case ID_HELP:          m_onCommand(Command::ShowHelp); break;
        case ID_EXIT:          m_onCommand(Command::Exit); break;
        default: break;
    }
}

LRESULT CALLBACK TrayIcon::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    if (self && msg == self->m_taskbarCreatedMessage && self->m_taskbarCreatedMessage != 0) {
        // Explorer restarted: re-add the icon or it is gone for good.
        self->m_added = false;
        HINSTANCE instance = self->m_instance;
        HWND existing = self->m_hwnd;
        NOTIFYICONDATAW data = {};
        data.cbSize = sizeof(data);
        data.hWnd = existing;
        data.uID = kIconId;
        data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        data.uCallbackMessage = kCallbackMessage;
        data.hIcon = self->m_icon;
        wcscpy_s(data.szTip, L"Shobdomala");
        self->m_added = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
        self->refresh(self->m_mode, self->m_livePreview, self->m_keyboardVisible);
        (void)instance;
        return 0;
    }

    if (self && msg == self->m_callbackMessage) {
        switch (LOWORD(lParam)) {
            case WM_LBUTTONUP:
                if (self->m_onCommand) {
                    self->m_onCommand(Command::ToggleMode);
                }
                return 0;
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU:
                self->showMenu();
                return 0;
            default:
                break;
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
