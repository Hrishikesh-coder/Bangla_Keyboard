#include "core/FixedLayoutEngine.h"
#include "core/BanglaText.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

namespace {

/// Reads one level object ("base", "shift", "altgr") into a char -> string map.
size_t readLevelInto(const json& root, const char* field,
                     std::unordered_map<char, std::string>& out) {
    if (!root.contains(field) || !root[field].is_object()) {
        return 0;
    }
    size_t count = 0;
    for (auto& [key, value] : root[field].items()) {
        if (key.size() != 1) {
            std::cerr << "[FixedLayoutEngine] Key \"" << key << "\" in \"" << field
                      << "\" ignored: keys must be exactly one character." << std::endl;
            continue;
        }
        if (value.is_string()) {
            // The Probhat chart specifies the precomposed ড় and ঢ়, which are the odd ones
            // out in this codebase: the phonetic rules, the exception dictionary and the
            // word list are all decomposed. Normalising here means text typed on the
            // layout is byte-identical to the same text typed phonetically, and can be
            // looked up in the same dictionary.
            out[key[0]] = BanglaText::normalize(value.get<std::string>());
            ++count;
        }
    }
    return count;
}

} // namespace

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

        m_layoutName = (j.contains("name") && j["name"].is_string())
                           ? j["name"].get<std::string>()
                           : std::string("unnamed layout");

        // Base and shift share one map because the keyboard has already applied Shift by
        // the time we see the character: 'k' and 'K' are simply two different keys to us.
        size_t loaded = 0;
        loaded += readLevelInto(j, "base", m_base);
        loaded += readLevelInto(j, "shift", m_base);
        loaded += readLevelInto(j, "altgr", m_altgr);

        // Accept the older single-"map" form so a layout written for the two-level engine
        // still loads.
        loaded += readLevelInto(j, "map", m_base);

        if (loaded == 0) {
            std::cerr << "[FixedLayoutEngine] Layout file defines no keys "
                         "(expected \"base\"/\"shift\"/\"altgr\" objects)." << std::endl;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FixedLayoutEngine] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

const std::unordered_map<char, std::string>& FixedLayoutEngine::mapFor(Level level) const {
    return (level == Level::AltGr) ? m_altgr : m_base;
}

bool FixedLayoutEngine::isMapped(char key, Level level) const {
    const auto& map = mapFor(level);
    return map.find(key) != map.end();
}

std::string FixedLayoutEngine::mapKey(char key, Level level) const {
    const auto& map = mapFor(level);
    auto it = map.find(key);
    return it != map.end() ? it->second : std::string(1, key);
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
    m_base.clear();
    m_altgr.clear();
    m_layoutName.clear();
}

size_t FixedLayoutEngine::size() const {
    return m_base.size() + m_altgr.size();
}

const std::unordered_map<char, std::string>& FixedLayoutEngine::keyMap(Level level) const {
    return mapFor(level);
}

void FixedLayoutEngine::setKey(char key, const std::string& glyph, Level level) {
    auto& map = (level == Level::AltGr) ? m_altgr : m_base;
    if (glyph.empty()) {
        map.erase(key);
    } else {
        map[key] = glyph;
    }
}

std::string FixedLayoutEngine::saveToString() const {
    json j;
    j["name"] = m_layoutName.empty() ? "unnamed layout" : m_layoutName;
    json baseObj = json::object();
    json shiftObj = json::object();
    json altgrObj = json::object();

    for (const auto& [k, v] : m_base) {
        std::string keyStr(1, k);
        if (std::isupper(static_cast<unsigned char>(k)) ||
            (k >= '!' && k <= '&') || k == '(' || k == ')' || k == '*' || k == '+' ||
            k == ':' || k == '<' || k == '>' || k == '?' || k == '@' || k == '^' ||
            k == '_' || k == '{' || k == '|' || k == '}' || k == '~') {
            shiftObj[keyStr] = v;
        } else {
            baseObj[keyStr] = v;
        }
    }
    for (const auto& [k, v] : m_altgr) {
        std::string keyStr(1, k);
        altgrObj[keyStr] = v;
    }

    j["base"] = baseObj;
    j["shift"] = shiftObj;
    j["altgr"] = altgrObj;
    return j.dump(2);
}

bool FixedLayoutEngine::saveToFile(const std::string& jsonFilePath) const {
    std::ofstream file(jsonFilePath);
    if (!file.is_open()) {
        return false;
    }
    file << saveToString();
    return true;
}
