#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

/// One suggested word, with the evidence for suggesting it.
struct WordSuggestion {
    std::string word;
    uint32_t frequency = 0;   ///< Corpus frequency; higher ranks first
    int distance = 0;         ///< Edit distance from what was typed; 0 for a prefix match
};

/**
 * @brief A frequency-ranked trie over whole Bangla words.
 *
 * Distinct from TokenTrie, and necessarily so. TokenTrie indexes *Roman rule tokens*, is
 * keyed by a 128-entry array of ASCII bytes, and answers "which longer rules start with
 * kh". That is a useful thing to know and not what Tier 4 asks for. This one indexes
 * Bengali words, is keyed by Unicode codepoint, and answers "which words start with বাং".
 *
 * Keying by codepoint rather than by UTF-8 byte is what makes edit distance meaningful:
 * every Bengali letter is three bytes, so a byte-wise distance would report 3 for a
 * one-letter typo and would happily produce broken sequences mid-word.
 *
 * Two queries:
 *   - predict(): prefix completion, ranked by frequency. Cheap - one walk plus a subtree
 *     traversal.
 *   - correct(): fuzzy match within an edit distance, for when the prefix walk finds
 *     nothing because the user mistyped. Implemented as a Levenshtein DP row carried down
 *     the trie, so an entire subtree is abandoned as soon as its best possible score
 *     exceeds the threshold. Comparing against every word separately would be O(words x
 *     length) per keystroke, which is not affordable inside a keyboard hook.
 */
class WordDictionary {
public:
    WordDictionary() = default;

    /// Loads from a JSON file. A missing file is not an error; the feature simply idles.
    bool loadFromFile(const std::string& jsonFilePath);

    /// Loads from a JSON string: {"words": {"বাংলা": 900, ...}} or {"words": ["বাংলা", ...]}.
    bool loadFromString(const std::string& jsonContent);

    /// Inserts or updates a word. Frequency 0 is treated as 1, so a bare list still ranks.
    void add(const std::string& word, uint32_t frequency = 1);

    /// True when the exact word is in the dictionary.
    bool contains(const std::string& word) const;

    /**
     * @brief Words beginning with `prefix`, most frequent first.
     *
     * The prefix itself is included when it is a word in its own right; callers that are
     * offering completions of what the user already typed will want to drop it.
     */
    std::vector<WordSuggestion> predict(const std::string& prefix, size_t limit = 5) const;

    /**
     * @brief Words within `maxDistance` edits of `word`, closest first then most frequent.
     *
     * Intended for the case where predict() came back empty, which usually means a typo
     * rather than an unusual word.
     */
    std::vector<WordSuggestion> correct(const std::string& word,
                                        int maxDistance = 2,
                                        size_t limit = 5) const;

    size_t size() const { return m_wordCount; }
    void clear();

    /// Splits UTF-8 into codepoints. Exposed because the tests and the composer need it.
    static std::vector<char32_t> toCodepoints(const std::string& utf8);
    static std::string fromCodepoints(const std::vector<char32_t>& codepoints);

private:
    struct Node {
        std::map<char32_t, std::unique_ptr<Node>> children;
        uint32_t frequency = 0; ///< Non-zero marks the end of a word
    };

    const Node* walk(const std::vector<char32_t>& codepoints) const;

    void collect(const Node* node,
                 std::vector<char32_t>& prefix,
                 std::vector<WordSuggestion>& out) const;

    /// One step of the trie-walking Levenshtein search.
    void searchFuzzy(const Node* node,
                     char32_t letter,
                     const std::vector<char32_t>& target,
                     const std::vector<int>& previousRow,
                     std::vector<char32_t>& current,
                     int maxDistance,
                     std::vector<WordSuggestion>& out) const;

    Node m_root;
    size_t m_wordCount = 0;
};
