#include "core/SymbolTable.h"
#include "core/UnicodeComposer.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

namespace {

/// Bengali signs that attach to the preceding syllable rather than starting a new one.
bool isBengaliModifierSign(char32_t cp) {
    return cp == 0x0981   // ঁ Chandrabindu
        || cp == 0x0982   // ং Anusvara
        || cp == 0x0983   // ঃ Visarga
        || cp == 0x09CE;  // ৎ Khanda Ta
}

/// Reads a JSON value that may be a string or an array of strings into a candidate vector.
std::vector<std::string> readCandidateList(const json& value) {
    std::vector<std::string> candidates;
    if (value.is_array()) {
        for (const auto& elem : value) {
            if (elem.is_string()) {
                candidates.push_back(elem.get<std::string>());
            }
        }
    } else if (value.is_string()) {
        candidates.push_back(value.get<std::string>());
    }
    return candidates;
}

/// Parses one entry of a token's "context" array into a ContextVariant.
std::optional<ContextVariant> readContextVariant(const std::string& token, const json& entry) {
    if (!entry.is_object() || !entry.contains("candidates")) {
        std::cerr << "[SymbolTable] Token \"" << token
                  << "\": context entry ignored (needs a \"candidates\" field)." << std::endl;
        return std::nullopt;
    }

    ContextVariant variant;
    variant.candidates = readCandidateList(entry["candidates"]);

    if (entry.contains("when")) {
        const json& when = entry["when"];
        if (when.is_array()) {
            for (const auto& flagName : when) {
                if (!flagName.is_string()) continue;
                uint32_t flag = contextFlagFromString(flagName.get<std::string>());
                if (flag == Ctx::NONE) {
                    std::cerr << "[SymbolTable] Token \"" << token << "\": unknown context \""
                              << flagName.get<std::string>() << "\" ignored." << std::endl;
                } else {
                    variant.required |= flag;
                }
            }
        } else if (when.is_string()) {
            variant.required |= contextFlagFromString(when.get<std::string>());
        }
    }

    return variant;
}

/// Parses one "token": value pair into a TokenRule, accepting legacy and extended forms.
TokenRule readRule(const std::string& token, const json& value) {
    TokenRule rule;
    rule.token = token;

    if (value.is_object()) {
        if (value.contains("candidates")) {
            rule.candidates = readCandidateList(value["candidates"]);
        }
        if (value.contains("class") && value["class"].is_string()) {
            rule.cls = tokenClassFromString(value["class"].get<std::string>());
        }
        if (value.contains("context") && value["context"].is_array()) {
            for (const auto& entry : value["context"]) {
                if (auto variant = readContextVariant(token, entry)) {
                    rule.variants.push_back(std::move(*variant));
                }
            }
        }
    } else {
        // Legacy form: a bare string or a bare array of strings.
        rule.candidates = readCandidateList(value);
    }

    return rule;
}

} // namespace

// ---------------------------------------------------------------------------
// TokenRule
// ---------------------------------------------------------------------------

const std::vector<std::string>& TokenRule::select(uint32_t context) const {
    for (const auto& variant : variants) {
        // Every required bit must be present. A variant with no requirements always fires,
        // which makes it a usable "default override" if declared last.
        if ((context & variant.required) == variant.required) {
            return variant.candidates;
        }
    }
    return candidates;
}

// ---------------------------------------------------------------------------
// SymbolTable
// ---------------------------------------------------------------------------

bool SymbolTable::loadFromFile(const std::string& jsonFilePath) {
    std::ifstream file(jsonFilePath);
    if (!file.is_open()) {
        std::cerr << "[SymbolTable] Error: Could not open rules file: " << jsonFilePath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

bool SymbolTable::loadFromString(const std::string& jsonContent) {
    try {
        json j = json::parse(jsonContent);
        clear();

        for (auto& [key, value] : j.items()) {
            // Keys beginning with '_' are reserved for file metadata such as "_comment".
            if (!key.empty() && key[0] == '_') {
                continue;
            }
            TokenRule rule = readRule(key, value);
            if (rule.cls == TokenClass::UNKNOWN) {
                rule.cls = deriveClass(rule.candidates);
            }
            m_table[key] = std::move(rule);
        }

        rebuildIndexes();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SymbolTable] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

void SymbolTable::addRule(const std::string& romanToken, const std::vector<std::string>& candidates) {
    TokenRule rule;
    rule.token = romanToken;
    rule.candidates = candidates;
    rule.cls = deriveClass(candidates);
    m_table[romanToken] = std::move(rule);
    rebuildIndexes();
}

void SymbolTable::addRule(const TokenRule& rule) {
    TokenRule copy = rule;
    if (copy.cls == TokenClass::UNKNOWN) {
        copy.cls = deriveClass(copy.candidates);
    }
    m_table[copy.token] = std::move(copy);
    rebuildIndexes();
}

bool SymbolTable::hasToken(const std::string& romanToken) const {
    return m_table.find(romanToken) != m_table.end();
}

const std::vector<std::string>* SymbolTable::lookup(const std::string& romanToken) const {
    auto it = m_table.find(romanToken);
    return it != m_table.end() ? &it->second.candidates : nullptr;
}

const std::vector<std::string>* SymbolTable::lookupContextual(const std::string& romanToken,
                                                              uint32_t context) const {
    auto it = m_table.find(romanToken);
    return it != m_table.end() ? &it->second.select(context) : nullptr;
}

const TokenRule* SymbolTable::lookupRule(const std::string& romanToken) const {
    auto it = m_table.find(romanToken);
    return it != m_table.end() ? &it->second : nullptr;
}

TokenClass SymbolTable::classOf(const std::string& romanToken) const {
    auto it = m_table.find(romanToken);
    return it != m_table.end() ? it->second.cls : TokenClass::UNKNOWN;
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
    m_trie.clear();
    m_maxTokenLength = 0;
}

size_t SymbolTable::size() const {
    return m_table.size();
}

size_t SymbolTable::contextualRuleCount() const {
    size_t count = 0;
    for (const auto& [token, rule] : m_table) {
        (void)token;
        if (!rule.variants.empty()) {
            ++count;
        }
    }
    return count;
}

void SymbolTable::rebuildIndexes() {
    m_sortedTokens.clear();
    m_sortedTokens.reserve(m_table.size());
    m_trie.clear();
    m_maxTokenLength = 0;

    for (const auto& [key, rule] : m_table) {
        (void)rule;
        m_sortedTokens.push_back(key);
        m_trie.insert(key);
        if (key.length() > m_maxTokenLength) {
            m_maxTokenLength = key.length();
        }
    }

    // Kept for inspection, diagnostics and backwards compatibility; the Tokenizer now uses
    // the trie instead of walking this list.
    std::sort(m_sortedTokens.begin(), m_sortedTokens.end(), [](const std::string& a, const std::string& b) {
        if (a.length() != b.length()) {
            return a.length() > b.length();
        }
        return a < b;
    });
}

TokenClass SymbolTable::deriveClass(const std::vector<std::string>& candidates) {
    for (const auto& candidate : candidates) {
        if (candidate.empty()) {
            continue; // epsilon carries no class information
        }
        auto codepoints = UnicodeComposer::utf8ToCodepoints(candidate);
        if (codepoints.empty()) {
            continue;
        }
        char32_t first = codepoints.front();
        if (UnicodeComposer::isBengaliConsonant(first))         return TokenClass::CONSONANT;
        if (UnicodeComposer::isBengaliIndependentVowel(first))  return TokenClass::VOWEL;
        if (UnicodeComposer::isBengaliDependentVowel(first))    return TokenClass::VOWEL;
        if (isBengaliModifierSign(first))                       return TokenClass::MODIFIER;
        return TokenClass::PUNCTUATION;
    }
    return TokenClass::UNKNOWN;
}
