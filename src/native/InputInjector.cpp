#include "native/InputInjector.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <iostream>

/*
 * ============================================================================
 * WINDOWS API EXPLANATIONS:
 *
 * 1. MultiByteToWideChar
 *    - What it does:
 *      Maps a character string (UTF-8 in our case) to a UTF-16 wide-character
 *      string (wchar_t / WCHAR) used natively by Windows NT APIs.
 *    - Why we use it:
 *      Our core engine operates on standard UTF-8 strings. The Windows SendInput
 *      Unicode keyboard API requires 16-bit UTF-16 code units in KEYBDINPUT.wScan.
 *    - Data received:
 *      CodePage (CP_UTF8), dwFlags (0), lpMultiByteStr (UTF-8 pointer), cbMultiByte (byte length),
 *      lpWideCharStr (destination buffer), cchWideChar (destination buffer capacity in wchar_t).
 *    - Data returned:
 *      Number of characters written to the destination buffer, or 0 on failure.
 *
 * 2. SendInput
 *    - What it does:
 *      Synthesizes keystrokes, mouse motions, and button clicks into the global
 *      input stream, directing them to the currently focused window as if they came
 *      from hardware.
 *    - Why we use it:
 *      We need our transliterated Bengali Unicode characters to appear inside
 *      whichever window currently has user focus (e.g. Notepad, Word, browser, editor).
 *    - Data received:
 *      cInputs (number of structures in pInputs array), pInputs (pointer to INPUT array),
 *      cbSize (sizeof(INPUT)).
 *    - Data returned:
 *      The number of events that were successfully inserted into the keyboard or
 *      mouse input stream. If return value == 0, the input was blocked by UIPI or failed.
 * ============================================================================
 */

size_t InputInjector::injectText(const std::string& utf8Text) {
    if (utf8Text.empty()) {
        return 0;
    }

    // Step 1: Calculate required buffer size for UTF-16
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), static_cast<int>(utf8Text.size()), nullptr, 0);
    if (wideLen <= 0) {
        std::cerr << "[InputInjector] MultiByteToWideChar size check failed." << std::endl;
        return 0;
    }

    std::wstring wideStr(wideLen, L'\0');
    int converted = MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), static_cast<int>(utf8Text.size()), &wideStr[0], wideLen);
    if (converted <= 0) {
        std::cerr << "[InputInjector] MultiByteToWideChar conversion failed." << std::endl;
        return 0;
    }

    // Step 2: Build array of INPUT events.
    // For each wide character, Windows expects:
    // 1) KEYDOWN with KEYEVENTF_UNICODE, wScan = character, wVk = 0.
    // 2) KEYUP   with KEYEVENTF_UNICODE | KEYEVENTF_KEYUP, wScan = character, wVk = 0.
    //
    // Note: Windows automatically sets LLKHF_INJECTED in the low-level keyboard hook
    // for all events synthesized via SendInput.
    std::vector<INPUT> inputs;
    inputs.reserve(wideStr.size() * 2);

    for (wchar_t ch : wideStr) {
        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = 0;
        down.ki.wScan = ch;
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        down.ki.time = 0;
        down.ki.dwExtraInfo = 0;
        inputs.push_back(down);

        INPUT up = {};
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = 0;
        up.ki.wScan = ch;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        up.ki.time = 0;
        up.ki.dwExtraInfo = 0;
        inputs.push_back(up);
    }

    UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    return sent / 2;
}

size_t InputInjector::utf16UnitCount(const std::string& utf8Text) {
    if (utf8Text.empty()) {
        return 0;
    }
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(),
                                      static_cast<int>(utf8Text.size()), nullptr, 0);
    return wideLen > 0 ? static_cast<size_t>(wideLen) : 0;
}

size_t InputInjector::injectBackspaces(size_t count) {
    if (count == 0) {
        return 0;
    }

    std::vector<INPUT> inputs;
    inputs.reserve(count * 2);

    for (size_t i = 0; i < count; ++i) {
        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = VK_BACK;
        inputs.push_back(down);

        INPUT up = {};
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = VK_BACK;
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }

    UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    return sent / 2;
}

size_t InputInjector::replaceText(size_t previousUnits, const std::string& utf8Text) {
    std::wstring wideStr;
    if (!utf8Text.empty()) {
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(),
                                          static_cast<int>(utf8Text.size()), nullptr, 0);
        if (wideLen > 0) {
            wideStr.resize(static_cast<size_t>(wideLen));
            MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), static_cast<int>(utf8Text.size()),
                                &wideStr[0], wideLen);
        }
    }

    if (previousUnits == 0 && wideStr.empty()) {
        return 0;
    }

    // One batch: backspaces first, then the replacement characters. SendInput guarantees
    // the events in a single call are not interleaved with other input.
    std::vector<INPUT> inputs;
    inputs.reserve((previousUnits + wideStr.size()) * 2);

    for (size_t i = 0; i < previousUnits; ++i) {
        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = VK_BACK;
        inputs.push_back(down);

        INPUT up = {};
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = VK_BACK;
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }

    for (wchar_t ch : wideStr) {
        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = ch;
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(down);

        INPUT up = {};
        up.type = INPUT_KEYBOARD;
        up.ki.wScan = ch;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }

    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    return wideStr.size();
}

bool InputInjector::injectChar(wchar_t ch) {
    INPUT inputs[2] = {};

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = 0;
    inputs[0].ki.wScan = ch;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 0;
    inputs[1].ki.wScan = ch;
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    UINT sent = SendInput(2, inputs, sizeof(INPUT));
    return sent == 2;
}
