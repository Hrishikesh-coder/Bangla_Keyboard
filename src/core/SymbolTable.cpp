#include "core/SymbolTable.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

bool SymbolTable::loadFromFile(const std::string& jsonFilePath) {
    std::ifstream file(jsonFilePath);
    if (!file.is_open()) {
        std::cerr << "[SymbolTable] Error: Could not open rules file: " << jsonFilePath << std::endl;
        return false;
    }

    try {
        json j;
        file >> j;
        clear();

        for (auto& [key, value] : j.items()) {
            std::vector<std::string> candidates;
            if (value.is_array()) {
                for (auto& elem : value) {
                    if (elem.is_string()) {
                        candidates.push_back(elem.get<std::string>());
                    }
                }
            } else if (value.is_string()) {
                candidates.push_back(value.get<std::string>());
            }
            m_table[key] = std::move(candidates);
        }

        rebuildSortedTokens();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SymbolTable] JSON parse error from file: " << e.what() << std::endl;
        return false;
    }
}

bool SymbolTable::loadFromString(const std::string& jsonContent) {
    try {
        json j = json::parse(jsonContent);
        clear();

        for (auto& [key, value] : j.items()) {
            std::vector<std::string> candidates;
            if (value.is_array()) {
                for (auto& elem : value) {
                    if (elem.is_string()) {
                        candidates.push_back(elem.get<std::string>());
                    }
                }
            } else if (value.is_string()) {
                candidates.push_back(value.get<std::string>());
            }
            m_table[key] = std::move(candidates);
        }

        rebuildSortedTokens();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SymbolTable] JSON parse error from string: " << e.what() << std::endl;
        return false;
    }
}

void SymbolTable::addRule(const std::string& romanToken, const std::vector<std::string>& candidates) {
    m_table[romanToken] = candidates;
    rebuildSortedTokens();
}

bool SymbolTable::hasToken(const std::string& romanToken) const {
    return m_table.find(romanToken) != m_table.end();
}

const std::vector<std::string>* SymbolTable::lookup(const std::string& romanToken) const {
    auto it = m_table.find(romanToken);
    if (it != m_table.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::vector<std::string>& SymbolTable::getTokensSortedByLengthDesc() const {
    return m_sortedTokens;
}

size_t SymbolTable::getMaxTokenLength() const {
    return m_maxTokenLength;
}

void SymbolTable::clear() {
    m_table.clear();
    m_sortedTokens.clear();
    m_maxTokenLength = 0;
}

size_t SymbolTable::size() const {
    return m_table.size();
}

void SymbolTable::rebuildSortedTokens() {
    m_sortedTokens.clear();
    m_sortedTokens.reserve(m_table.size());
    m_maxTokenLength = 0;

    for (const auto& [key, _] : m_table) {
        m_sortedTokens.push_back(key);
        if (key.length() > m_maxTokenLength) {
            m_maxTokenLength = key.length();
        }
    }

    // Sort tokens by descending length. For equal length, sort lexicographically.
    // This allows longest-match-first matching without needing a complex trie.
    std::sort(m_sortedTokens.begin(), m_sortedTokens.end(), [](const std::string& a, const std::string& b) {
        if (a.length() != b.length()) {
            return a.length() > b.length();
        }
        return a < b;
    });
}
