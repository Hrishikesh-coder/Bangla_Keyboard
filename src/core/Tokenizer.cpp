#include "core/Tokenizer.h"
#include <algorithm>

Tokenizer::Tokenizer(const SymbolTable& symbolTable)
    : m_symbolTable(symbolTable) {
}

std::vector<std::string> Tokenizer::tokenize(const std::string& input) const {
    std::vector<std::string> tokens;
    if (input.empty()) {
        return tokens;
    }

    const TokenTrie& trie = m_symbolTable.trie();
    size_t pos = 0;
    const size_t inputLen = input.length();

    while (pos < inputLen) {
        // Longest-match-first, resolved in a single trie walk: descend as far as the input
        // allows and keep the deepest terminal node seen. This is the same greedy
        // "maximal munch" rule as before, without the repeated substring construction.
        size_t matchLen = trie.longestMatchLength(input, pos);

        if (matchLen > 0) {
            tokens.push_back(input.substr(pos, matchLen));
            pos += matchLen;
        } else {
            // No known token starts here: preserve the single character as-is so that
            // punctuation and digits survive transliteration untouched (UTF-8 aware).
            size_t charLen = 1;
            unsigned char c = input[pos];
            if ((c & 0xE0) == 0xC0) charLen = 2;
            else if ((c & 0xF0) == 0xE0) charLen = 3;
            else if ((c & 0xF8) == 0xF0) charLen = 4;
            
            if (pos + charLen > inputLen) {
                charLen = inputLen - pos;
            }
            
            tokens.push_back(input.substr(pos, charLen));
            pos += charLen;
        }
    }

    return tokens;
}
