#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Whole-word overrides applied before the rule engine runs.
 *
 * Rule-based transliteration is systematic, and Bengali spelling is not. Two cases the
 * rules cannot reach on their own:
 *
 *  - Words whose written form contains something nobody types. "dhonnobad" is pronounced
 *    with a doubled ন but written ধন্যবাদ, with a য-fola. No phonetic rule can derive that.
 *  - English words that must survive untouched. "download" typed mid-sentence should stay
 *    "download", not become ডোৱ্নলোঅদ. Such words map to themselves, which is how the
 *    engine learns to leave them alone.
 *
 * Lookup tries the exact spelling first and then the lowercased form, so a case-sensitive
 * entry can be added later without disturbing the common case.
 */
class ExceptionDictionary {
public:
    ExceptionDictionary() = default;

    /// Loads overrides from a JSON file. Returns false if the file is missing or malformed.
    bool loadFromFile(const std::string& jsonFilePath);

    /// Loads overrides from a raw JSON string.
    bool loadFromString(const std::string& jsonContent);

    /// Inserts or replaces one override.
    void add(const std::string& romanWord, const std::string& output);

    /**
     * @brief Looks up an override.
     * @param romanWord The full Roman word as typed.
     * @param out Receives the replacement text when found.
     * @return True when an override exists and `out` was written.
     */
    bool lookup(const std::string& romanWord, std::string& out) const;

    /// True when an override exists for the word.
    bool contains(const std::string& romanWord) const;

    void clear();
    size_t size() const;

private:
    static std::string toLower(const std::string& s);

    std::unordered_map<std::string, std::string> m_exact;
    std::unordered_map<std::string, std::string> m_lower;
};
