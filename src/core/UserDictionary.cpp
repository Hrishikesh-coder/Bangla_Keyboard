#include "core/UserDictionary.h"
#include "core/BanglaText.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

std::string UserDictionary::defaultPath() {
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA")) {
        return std::string(appdata) + "\\Shobdomala\\learned.json";
    }
#endif
    return "shobdomala_learned.json";
}

bool UserDictionary::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false; // first run
    }
    try {
        std::stringstream buffer;
        buffer << file.rdbuf();
        json j = json::parse(buffer.str());
        const json& words = (j.is_object() && j.contains("words")) ? j["words"] : j;
        if (!words.is_object()) {
            return false;
        }
        m_learned.clear();
        for (auto& [word, weight] : words.items()) {
            if (!word.empty() && word[0] == '_') {
                continue;
            }
            m_learned[BanglaText::normalize(word)] =
                weight.is_number_unsigned() ? weight.get<uint32_t>() : kInitialWeight;
        }
        return true;
    } catch (const std::exception& e) {
        // A corrupt learned-words file must never stop the program starting; the worst
        // case is that the user teaches it again.
        std::cerr << "[UserDictionary] Ignoring malformed file: " << e.what() << std::endl;
        return false;
    }
}

bool UserDictionary::save(const std::string& path) const {
    json words = json::object();
    for (const auto& [word, weight] : m_learned) {
        words[word] = weight;
    }

    json j;
    j["_comment"] = "Words learned from your corrections. Safe to delete or hand-edit.";
    j["words"] = words;

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file << j.dump(1) << "\n";
    return file.good();
}

void UserDictionary::learn(const std::string& word) {
    if (word.empty()) {
        return;
    }
    // Normalise on the way in for the same reason the corpus list is normalised: য় has
    // two encodings, and a learned word stored in the form the engine cannot produce
    // would never be found again.
    const std::string canonical = BanglaText::normalize(word);

    auto it = m_learned.find(canonical);
    if (it == m_learned.end()) {
        m_learned[canonical] = kInitialWeight;
    } else {
        // Saturating, so a word typed thousands of times cannot overflow or come to
        // dominate every ranking in the dictionary.
        it->second = std::min<uint32_t>(it->second + kReinforcement, 1000000u);
    }
}

bool UserDictionary::forget(const std::string& word) {
    return m_learned.erase(BanglaText::normalize(word)) > 0;
}

void UserDictionary::mergeInto(WordDictionary& dictionary) const {
    for (const auto& [word, weight] : m_learned) {
        dictionary.add(word, weight);
    }
}

bool UserDictionary::contains(const std::string& word) const {
    return m_learned.count(BanglaText::normalize(word)) > 0;
}
