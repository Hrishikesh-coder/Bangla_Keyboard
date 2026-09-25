#include "core/ExceptionDictionary.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

bool ExceptionDictionary::loadFromFile(const std::string& jsonFilePath) {
    std::ifstream file(jsonFilePath);
    if (!file.is_open()) {
        // A missing exception file is not fatal: the engine simply has no overrides.
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

bool ExceptionDictionary::loadFromString(const std::string& jsonContent) {
    try {
        json j = json::parse(jsonContent);
        clear();

        // Accept either a flat object or one nested under a "words" key, so the file can
        // carry metadata alongside the entries.
        const json& words = (j.is_object() && j.contains("words") && j["words"].is_object())
                                ? j["words"]
                                : j;

        for (auto& [key, value] : words.items()) {
            if (!key.empty() && key[0] == '_') {
                continue; // reserved for comments
            }
            if (value.is_string()) {
                add(key, value.get<std::string>());
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ExceptionDictionary] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

void ExceptionDictionary::add(const std::string& romanWord, const std::string& output) {
    m_exact[romanWord] = output;
    m_lower.emplace(toLower(romanWord), output);
}

bool ExceptionDictionary::lookup(const std::string& romanWord, std::string& out) const {
    auto exact = m_exact.find(romanWord);
    if (exact != m_exact.end()) {
        out = exact->second;
        return true;
    }
    auto lower = m_lower.find(toLower(romanWord));
    if (lower != m_lower.end()) {
        out = lower->second;
        return true;
    }
    return false;
}

bool ExceptionDictionary::contains(const std::string& romanWord) const {
    std::string ignored;
    return lookup(romanWord, ignored);
}

void ExceptionDictionary::clear() {
    m_exact.clear();
    m_lower.clear();
}

size_t ExceptionDictionary::size() const {
    return m_exact.size();
}

std::string ExceptionDictionary::toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}
