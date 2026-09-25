#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

/**
 * @brief Manages the phonetic symbol table mapping Roman tokens to Bengali Unicode strings.
 *
 * Implemented using a hash table (std::unordered_map<std::string, std::vector<std::string>>).
 * Can be loaded dynamically from phonetic_rules.json or populated programmatically.
 */
class SymbolTable {
public:
    SymbolTable() = default;

    /// Loads phonetic rules from a JSON file path.
    bool loadFromFile(const std::string& jsonFilePath);

    /// Loads phonetic rules from a raw JSON string.
    bool loadFromString(const std::string& jsonContent);

    /// Manually inserts or updates a rule.
    void addRule(const std::string& romanToken, const std::vector<std::string>& candidates);

    /// Checks if a Roman token exists in the symbol table.
    bool hasToken(const std::string& romanToken) const;

    /// Looks up the list of Bengali candidates for a Roman token. Returns nullptr if not found.
    const std::vector<std::string>* lookup(const std::string& romanToken) const;

    /// Returns all known tokens sorted in descending order of length.
    /// This is used directly by the Tokenizer for longest-match-first matching.
    const std::vector<std::string>& getTokensSortedByLengthDesc() const;

    /// Returns the length of the longest registered token.
    size_t getMaxTokenLength() const;

    /// Clears all rules.
    void clear();

    /// Returns the number of distinct Roman tokens registered.
    size_t size() const;

private:
    void rebuildSortedTokens();

    std::unordered_map<std::string, std::vector<std::string>> m_table;
    std::vector<std::string> m_sortedTokens;
    size_t m_maxTokenLength = 0;
};
