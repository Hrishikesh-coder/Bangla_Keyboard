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

    size_t pos = 0;
    const size_t inputLen = input.length();
    const size_t maxTokenLen = m_symbolTable.getMaxTokenLength();

    while (pos < inputLen) {
        bool matched = false;
        const size_t checkLen = std::min(maxTokenLen, inputLen - pos);

        // Longest-match-first: iterate from the maximum possible token length down to 1
        for (size_t len = checkLen; len >= 1; --len) {
            std::string sub = input.substr(pos, len);
            if (m_symbolTable.hasToken(sub)) {
                tokens.push_back(std::move(sub));
                pos += len;
                matched = true;
                break;
            }
        }

        // If no known token matched at current position, preserve the single character as-is
        if (!matched) {
            tokens.push_back(input.substr(pos, 1));
            pos += 1;
        }
    }

    return tokens;
}
