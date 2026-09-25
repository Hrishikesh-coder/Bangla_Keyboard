#include "core/FixedLayoutEngine.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

bool FixedLayoutEngine::loadFromFile(const std::string& jsonFilePath) {
    std::ifstream file(jsonFilePath);
    if (!file.is_open()) {
        std::cerr << "[FixedLayoutEngine] Could not open layout file: " << jsonFilePath << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

bool FixedLayoutEngine::loadFromString(const std::string& jsonContent) {
    try {
        json j = json::parse(jsonContent);
        clear();

        if (j.contains("name") && j["name"].is_string()) {
            m_layoutName = j["name"].get<std::string>();
        } else {
            m_layoutName = "unnamed layout";
        }

        if (!j.contains("map") || !j["map"].is_object()) {
            std::cerr << "[FixedLayoutEngine] Layout file has no \"map\" object." << std::endl;
            return false;
        }

        for (auto& [key, value] : j["map"].items()) {
            if (key.size() != 1) {
                std::cerr << "[FixedLayoutEngine] Layout key \"" << key
                          << "\" ignored: keys must be exactly one character." << std::endl;
                continue;
            }
            if (value.is_string()) {
                m_map[key[0]] = value.get<std::string>();
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FixedLayoutEngine] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

bool FixedLayoutEngine::isMapped(char key) const {
    return m_map.find(key) != m_map.end();
}

std::string FixedLayoutEngine::mapKey(char key) const {
    auto it = m_map.find(key);
    return it != m_map.end() ? it->second : std::string(1, key);
}

std::string FixedLayoutEngine::mapText(const std::string& text) const {
    std::string out;
    out.reserve(text.size() * 3);
    for (char ch : text) {
        out += mapKey(ch);
    }
    return out;
}

void FixedLayoutEngine::clear() {
    m_map.clear();
    m_layoutName.clear();
}
