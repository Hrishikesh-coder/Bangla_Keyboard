#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Prefix tree over the Roman tokens registered in the SymbolTable.
 *
 * Replaces the original "try every substring length from maxTokenLength down to 1" scan.
 * That scan built and hashed up to maxTokenLength temporary std::string objects at every
 * input position; the trie walks the input once and allocates nothing, so tokenization
 * cost becomes O(length of input) instead of O(length of input x maxTokenLength).
 *
 * This matters more than raw speed: the low-level keyboard hook re-tokenizes the whole
 * in-progress word on every single keystroke, inside a callback Windows will silently
 * unhook if it takes too long.
 *
 * Only ASCII is stored, which is all a Roman phonetic token can contain.
 */
class TokenTrie {
public:
    TokenTrie() = default;

    /// Inserts a token. Re-inserting the same token is harmless.
    void insert(const std::string& token);

    /// Removes every token.
    void clear();

    /**
     * @brief Finds the longest registered token that is a prefix of input[startPos...].
     * @return Length of the longest match, or 0 when no registered token matches.
     */
    size_t longestMatchLength(const std::string& input, size_t startPos) const;

    /// True when the token is present.
    bool contains(const std::string& token) const;

    /**
     * @brief Finds all registered tokens that start with the given prefix.
     * Useful for autocomplete suggestions.
     * @param prefix The prefix to search for.
     * @param limit Maximum number of completions to return (0 for unlimited).
     * @return A list of matching tokens in lexicographical order.
     */
    std::vector<std::string> getCompletions(const std::string& prefix, size_t limit = 0) const;

    /// Number of distinct tokens inserted.
    size_t size() const { return m_size; }

private:
    struct Node {
        std::array<std::unique_ptr<Node>, 128> children{};
        bool terminal = false;
    };

    void collectCompletions(const Node* node, std::string currentPrefix, std::vector<std::string>& results, size_t limit) const;

    Node m_root;
    size_t m_size = 0;
};
