#include "ui/UiTheme.h"

#include <vector>

namespace {

/// GetDpiForWindow is Windows 10 1607+. Resolve it dynamically so the binary still runs
/// on older systems rather than failing to load.
using GetDpiForWindowFn = UINT(WINAPI*)(HWND);

GetDpiForWindowFn resolveGetDpiForWindow() {
    static GetDpiForWindowFn fn = []() -> GetDpiForWindowFn {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (!user32) {
            return nullptr;
        }
        return reinterpret_cast<GetDpiForWindowFn>(
            reinterpret_cast<void*>(GetProcAddress(user32, "GetDpiForWindow")));
    }();
    return fn;
}

/// Picks the first installed face from a preference list, so a missing font degrades to the
/// next best rather than to the system default.
HFONT createFont(const wchar_t* const* faces, size_t faceCount,
                 int pointSize, UINT dpi, int weight) {
    const int height = -MulDiv(pointSize, static_cast<int>(dpi), 72);

    for (size_t i = 0; i < faceCount; ++i) {
        HFONT font = CreateFontW(
            height, 0, 0, 0, weight,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_TT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            faces[i]);

        if (!font) {
            continue;
        }

        // Confirm Windows actually gave us the face we asked for; it silently substitutes.
        HDC screen = GetDC(nullptr);
        HGDIOBJ previous = SelectObject(screen, font);
        wchar_t actual[LF_FACESIZE] = {0};
        GetTextFaceW(screen, LF_FACESIZE, actual);
        SelectObject(screen, previous);
        ReleaseDC(nullptr, screen);

        if (_wcsicmp(actual, faces[i]) == 0 || i + 1 == faceCount) {
            return font;
        }
        DeleteObject(font);
    }
    return nullptr;
}

} // namespace

namespace UiTheme {

int scale(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

UINT dpiForWindow(HWND hwnd) {
    if (auto fn = resolveGetDpiForWindow()) {
        UINT dpi = fn(hwnd);
        if (dpi != 0) {
            return dpi;
        }
    }
    HDC screen = GetDC(nullptr);
    UINT dpi = static_cast<UINT>(GetDeviceCaps(screen, LOGPIXELSY));
    ReleaseDC(nullptr, screen);
    return dpi ? dpi : 96;
}

HFONT createBengaliFont(int pointSize, UINT dpi, int weight) {
    // Nirmala UI ships with Windows 8+ and shapes Bengali conjuncts and reordered matras
    // correctly. Vrinda is the older Windows Bengali face. Segoe UI is the last resort and
    // will render boxes, which is at least an obvious failure rather than a subtle one.
    static const wchar_t* kFaces[] = { L"Nirmala UI", L"Vrinda", L"Shonar Bangla", L"Segoe UI" };
    return createFont(kFaces, sizeof(kFaces) / sizeof(kFaces[0]), pointSize, dpi, weight);
}

HFONT createUiFont(int pointSize, UINT dpi, int weight) {
    static const wchar_t* kFaces[] = { L"Segoe UI Variable Text", L"Segoe UI", L"Tahoma" };
    return createFont(kFaces, sizeof(kFaces) / sizeof(kFaces[0]), pointSize, dpi, weight);
}

void fillRoundRect(HDC hdc, const RECT& rect, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = (border == CLR_INVALID) ? static_cast<HPEN>(GetStockObject(NULL_PEN))
                                       : CreatePen(PS_SOLID, 1, border);

    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    if (radius <= 0) {
        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    } else {
        RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius * 2, radius * 2);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    if (border != CLR_INVALID) {
        DeleteObject(pen);
    }
}

void drawText(HDC hdc, const std::wstring& text, RECT rect, COLORREF colour, UINT flags) {
    SetTextColor(hdc, colour);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text.c_str(), static_cast<int>(text.size()), &rect, flags);
}

SIZE measureText(HDC hdc, const std::wstring& text) {
    SIZE size = {0, 0};
    GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()), &size);
    return size;
}

std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) {
        return std::wstring();
    }
    int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                     nullptr, 0);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                        &wide[0], length);
    return wide;
}

void applyRoundedRegion(HWND hwnd, int width, int height, int radius) {
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius * 2, radius * 2);
    // Windows takes ownership of the region on success; do not delete it here.
    if (!SetWindowRgn(hwnd, region, TRUE)) {
        DeleteObject(region);
    }
}

} // namespace UiTheme
