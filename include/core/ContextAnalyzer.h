#pragma once

#include "SymbolTable.h"
#include "TokenContext.h"

#include <string>
#include <vector>

/**
 * @brief Second pass of the transliteration pipeline: works out where each token sits.
 *
 * The original prototype went straight from tokens to candidates, which is why every rule
 * behaved the same everywhere and DefaultCandidateResolver could only ever return index 0.
 * But Bengali orthography is context-dependent in ways the token stream alone can settle:
 *
 *  - "a" is আ at the start of a word and the া matra after a consonant;
 *  - "ng" is ঙ between vowels but ং before a consonant or at the end of a word;
 *  - "o" is ও standing alone, the ো matra mid-word, and silent (the inherent ô) word-finally.
 *
 * So the pipeline gains a pass that annotates every token with a context bitmask before any
 * candidate is chosen. This mirrors a two-pass assembler: pass 1 (the Tokenizer) recognises
 * the symbols, pass 2 resolves the references that depend on what surrounds them.
 *
 * The analyzer is deliberately stateless and works purely on token classes, so it runs before
 * a single Bengali codepoint has been selected.
 */
class ContextAnalyzer {
public:
    explicit ContextAnalyzer(const SymbolTable& symbolTable);

    /**
     * @brief Computes a context bitmask for each token in the sequence.
     * @param tokens Token sequence produced by the Tokenizer.
     * @return Vector of Ctx flag masks, one per token, in the same order.
     */
    std::vector<uint32_t> analyze(const std::vector<std::string>& tokens) const;

    /// Convenience: describes the analysis in readable form, for the CLI's :context command.
    std::string describe(const std::vector<std::string>& tokens) const;

private:
    const SymbolTable& m_symbolTable;
};
