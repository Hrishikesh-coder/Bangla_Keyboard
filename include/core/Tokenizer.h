#pragma once

#include "SymbolTable.h"
#include <string>
#include <vector>

/**
 * @brief Tokenizes Roman input strings using longest-match-first tokenization.
 *
 * Obtains valid phonetic tokens dynamically from the SymbolTable rather than
 * hardcoding tokens.
 *
 * Example:
 * Input "shanti" is matched against registered tokens ("sh", "a", "n", "t", "i")
 * producing ["sh", "a", "n", "t", "i"] rather than single characters ["s", "h", ...].
 */
class Tokenizer {
public:
    explicit Tokenizer(const SymbolTable& symbolTable);

    /**
     * @brief Performs longest-match-first tokenization on the given Roman input string.
     * @param input Raw Roman input (e.g. "shanti").
     * @return Vector of matched token strings.
     */
    std::vector<std::string> tokenize(const std::string& input) const;

private:
    const SymbolTable& m_symbolTable;
};
