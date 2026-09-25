#pragma once

#include "TokenContext.h"
#include "TokenTrie.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

/**
 * @brief One contextual override: a candidate list that applies only in a given context.
 *
 * `required` is a bitmask of Ctx flags. Every bit must be present in the token's computed
 * context for the variant to fire, so ["after_consonant", "word_end"] means "at the end of a
 * word, directly after a consonant".
 */
struct ContextVariant {
    uint32_t required = Ctx::NONE;
    std::vector<std::string> candidates;
};

/**
 * @brief A complete rule for one Roman token.
 *
 * Holds the default candidate list plus any contextual overrides, and the token's linguistic
 * class so neighbouring tokens can be reasoned about.
 */
struct TokenRule {
    std::string token;
    TokenClass cls = TokenClass::UNKNOWN;
    std::vector<std::string> candidates;
    std::vector<ContextVariant> variants;

    /**
     * @brief Chooses the candidate list to use for a given context.
     *
     * Variants are tested in declaration order and the first whose required bits are all
     * satisfied wins, so the JSON file reads top-to-bottom as "most specific rule first".
     */
    const std::vector<std::string>& select(uint32_t context) const;
};

/**
 * @brief Manages the phonetic symbol table mapping Roman tokens to Bengali Unicode strings.
 *
 * Backed by a hash table for lookup and a TokenTrie for longest-match tokenization. Rules are
 * loaded from phonetic_rules.json, which accepts three value forms per token:
 *
 *   "kh": "খ"                               // single candidate
 *   "sh": ["শ", "ষ", "স"]                   // ordered candidate list
 *   "ng": {                                 // full form with class and contextual overrides
 *     "class": "consonant",
 *     "candidates": ["ঙ", "ং"],
 *     "context": [
 *       { "when": ["before_consonant"], "candidates": ["ং", "ঙ"] }
 *     ]
 *   }
 *
 * The first two forms are exactly what the original prototype used, so existing rule files
 * keep working unchanged.
 */
class SymbolTable {
public:
    SymbolTable() = default;

    /// Loads phonetic rules from a JSON file path.
    bool loadFromFile(const std::string& jsonFilePath);

    /// Loads phonetic rules from a raw JSON string.
    bool loadFromString(const std::string& jsonContent);

    /// Manually inserts or updates a rule with a plain candidate list.
    void addRule(const std::string& romanToken, const std::vector<std::string>& candidates);

    /// Manually inserts or updates a full contextual rule.
    void addRule(const TokenRule& rule);

    /// Checks if a Roman token exists in the symbol table.
    bool hasToken(const std::string& romanToken) const;

    /// Looks up the default candidate list for a token. Returns nullptr if not found.
    const std::vector<std::string>* lookup(const std::string& romanToken) const;

    /// Looks up the candidate list that applies in the given context. Returns nullptr if not found.
    const std::vector<std::string>* lookupContextual(const std::string& romanToken,
                                                     uint32_t context) const;

    /// Returns the full rule for a token, or nullptr.
    const TokenRule* lookupRule(const std::string& romanToken) const;

    /// Returns the linguistic class of a token, or TokenClass::UNKNOWN if unregistered.
    TokenClass classOf(const std::string& romanToken) const;

    /// Returns all known tokens sorted in descending order of length.
    const std::vector<std::string>& getTokensSortedByLengthDesc() const;

    /// Trie over all registered tokens; used by the Tokenizer for longest-match-first matching.
    const TokenTrie& trie() const { return m_trie; }

    /// Returns the length of the longest registered token.
    size_t getMaxTokenLength() const;

    /// Clears all rules.
    void clear();

    /// Returns the number of distinct Roman tokens registered.
    size_t size() const;

    /// Number of tokens that carry at least one contextual override.
    size_t contextualRuleCount() const;

private:
    void rebuildIndexes();

    /**
     * @brief Infers a token's class from its candidates when the JSON does not declare one.
     *
     * Looks at the first Bengali codepoint of the first non-empty candidate, so a legacy
     * rule file gets sensible classes without being rewritten.
     */
    static TokenClass deriveClass(const std::vector<std::string>& candidates);

    std::unordered_map<std::string, TokenRule> m_table;
    std::vector<std::string> m_sortedTokens;
    TokenTrie m_trie;
    size_t m_maxTokenLength = 0;
};
