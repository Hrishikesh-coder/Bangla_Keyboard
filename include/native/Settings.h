#pragma once

#include "native/KeyboardState.h"

#include <string>

/**
 * @brief User preferences that survive a restart.
 *
 * Small on purpose. Everything here is a choice the user made by hand and would be
 * irritated to make again on every launch: which mode they were in, whether they wanted
 * live preview, and where they dragged the on-screen keyboard to.
 *
 * Deliberately NOT here: anything already expressed by a config file. The rules, the
 * layout and the word list are edited as JSON and reloaded on start; duplicating them into
 * a settings store would create two sources of truth and a question about which wins.
 *
 * Stored in %APPDATA%\Shobdomala\settings.json rather than beside the executable, because
 * a program directory is often read-only and because settings belong to a user, not to an
 * installation.
 */
class Settings {
public:
    InputMode startupMode = InputMode::ENGLISH;
    bool livePreview = true;
    bool onScreenKeyboardVisible = false;
    int onScreenKeyboardX = 0;
    int onScreenKeyboardY = 0;

    /// Resolves %APPDATA%\Shobdomala\settings.json, creating the directory if needed.
    static std::string defaultPath();

    /// Loads settings. A missing or malformed file leaves the defaults in place.
    bool load(const std::string& path);

    /// Writes settings. Returns false if the file could not be written.
    bool save(const std::string& path) const;
};
