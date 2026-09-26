#include "native/ConsoleHost.h"

#include <cstdio>
#include <iostream>

ConsoleHost::~ConsoleHost() {
    if (m_owned) {
        FreeConsole();
    }
}

bool ConsoleHost::attach(bool allocateIfMissing) {
    if (m_active) {
        return true;
    }

    // Prefer the console we were launched from. Someone running this from a terminal
    // expects output there, not in a second window that appears beside it.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        m_active = true;
        m_owned = false;
    } else if (allocateIfMissing && AllocConsole()) {
        m_active = true;
        m_owned = true;
        SetConsoleTitleW(L"Shobdomala \u2014 engine trace");
    } else {
        return false;
    }

    bindStandardStreams();

    // Bengali in the trace output is the whole point of reading it.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    return true;
}

void ConsoleHost::bindStandardStreams() {
    // CONOUT$ and CONIN$ address the console device directly rather than whatever the
    // standard handles happened to point at, which matters because a GUI-subsystem process
    // starts with those handles closed.
    FILE* stream = nullptr;

    if (freopen_s(&stream, "CONOUT$", "w", stdout) == 0) {
        setvbuf(stdout, nullptr, _IONBF, 0);
    }
    if (freopen_s(&stream, "CONOUT$", "w", stderr) == 0) {
        setvbuf(stderr, nullptr, _IONBF, 0);
    }
    freopen_s(&stream, "CONIN$", "r", stdin);

    // The iostream objects cached the old, closed handles when they were constructed.
    // Clearing the error state is what makes std::cout usable again after the reopen.
    std::cout.clear();
    std::cerr.clear();
    std::cin.clear();
    std::ios::sync_with_stdio(true);
}

void ConsoleHost::waitBeforeClosing() const {
    if (!m_owned) {
        return;
    }
    std::cout << "\nPress Enter to close this window." << std::endl;
    std::cin.clear();
    std::string discard;
    std::getline(std::cin, discard);
}
