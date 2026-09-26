#include "native/Settings.h"

#include <nlohmann/json.hpp>
#include <shlobj.h>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

namespace {

InputMode modeFromString(const std::string& name) {
    if (name == "phonetic") return InputMode::BENGALI_PHONETIC;
    if (name == "fixed")    return InputMode::BENGALI_FIXED;
    return InputMode::ENGLISH;
}

const char* modeToString(InputMode mode) {
    switch (mode) {
        case InputMode::BENGALI_PHONETIC: return "phonetic";
        case InputMode::BENGALI_FIXED:    return "fixed";
        default:                          return "english";
    }
}

} // namespace

std::string Settings::defaultPath() {
    // SHGetFolderPathW with CSIDL_APPDATA rather than SHGetKnownFolderPath: the KNOWNFOLDERID
    // constants live in uuid.lib, which drags an extra link dependency in for one path
    // lookup, and this form needs no COM allocation to free afterwards.
    wchar_t roaming[MAX_PATH] = {0};
    std::string base;

    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, roaming))) {
        int length = WideCharToMultiByte(CP_UTF8, 0, roaming, -1, nullptr, 0, nullptr, nullptr);
        if (length > 0) {
            base.resize(static_cast<size_t>(length - 1));
            WideCharToMultiByte(CP_UTF8, 0, roaming, -1, &base[0], length, nullptr, nullptr);
        }
    }

    if (base.empty()) {
        // No roaming profile (a stripped service account, say): fall back to the working
        // directory rather than failing to persist anything at all.
        return "shobdomala_settings.json";
    }

    const std::string folder = base + "\\Shobdomala";
    CreateDirectoryA(folder.c_str(), nullptr); // harmless when it already exists
    return folder + "\\settings.json";
}

bool Settings::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false; // first run; defaults stand
    }

    try {
        std::stringstream buffer;
        buffer << file.rdbuf();
        json j = json::parse(buffer.str());

        if (j.contains("mode") && j["mode"].is_string()) {
            startupMode = modeFromString(j["mode"].get<std::string>());
        }
        if (j.contains("livePreview") && j["livePreview"].is_boolean()) {
            livePreview = j["livePreview"].get<bool>();
        }
        if (j.contains("onScreenKeyboard") && j["onScreenKeyboard"].is_object()) {
            const json& kb = j["onScreenKeyboard"];
            if (kb.contains("visible") && kb["visible"].is_boolean()) {
                onScreenKeyboardVisible = kb["visible"].get<bool>();
            }
            if (kb.contains("x") && kb["x"].is_number_integer()) {
                onScreenKeyboardX = kb["x"].get<int>();
            }
            if (kb.contains("y") && kb["y"].is_number_integer()) {
                onScreenKeyboardY = kb["y"].get<int>();
            }
        }
        return true;
    } catch (const std::exception& e) {
        // A corrupt settings file must never stop the program starting. Keep the defaults
        // and say so once.
        std::cerr << "[Settings] Ignoring malformed settings file: " << e.what() << std::endl;
        return false;
    }
}

bool Settings::save(const std::string& path) const {
    json j;
    j["mode"] = modeToString(startupMode);
    j["livePreview"] = livePreview;
    j["onScreenKeyboard"] = {
        { "visible", onScreenKeyboardVisible },
        { "x", onScreenKeyboardX },
        { "y", onScreenKeyboardY }
    };

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file << j.dump(2) << "\n";
    return file.good();
}
