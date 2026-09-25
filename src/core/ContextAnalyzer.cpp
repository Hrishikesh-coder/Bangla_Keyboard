#include "core/ContextAnalyzer.h"

ContextAnalyzer::ContextAnalyzer(const SymbolTable& symbolTable)
    : m_symbolTable(symbolTable) {
}

std::vector<uint32_t> ContextAnalyzer::analyze(const std::vector<std::string>& tokens) const {
    std::vector<uint32_t> contexts(tokens.size(), Ctx::NONE);

    for (size_t i = 0; i < tokens.size(); ++i) {
        uint32_t mask = Ctx::NONE;

        if (i == 0) {
            mask |= Ctx::WORD_START;
        }
        if (i + 1 == tokens.size()) {
            mask |= Ctx::WORD_END;
        }

        if (i > 0) {
            switch (m_symbolTable.classOf(tokens[i - 1])) {
                case TokenClass::CONSONANT: mask |= Ctx::AFTER_CONSONANT; break;
                case TokenClass::VOWEL:     mask |= Ctx::AFTER_VOWEL;     break;
                case TokenClass::MODIFIER:  mask |= Ctx::AFTER_MODIFIER;  break;
                default: break;
            }
        }

        if (i + 1 < tokens.size()) {
            switch (m_symbolTable.classOf(tokens[i + 1])) {
                case TokenClass::CONSONANT: mask |= Ctx::BEFORE_CONSONANT; break;
                case TokenClass::VOWEL:     mask |= Ctx::BEFORE_VOWEL;     break;
                case TokenClass::MODIFIER:  mask |= Ctx::BEFORE_MODIFIER;  break;
                default: break;
            }
        }

        contexts[i] = mask;
    }

    return contexts;
}

std::string ContextAnalyzer::describe(const std::vector<std::string>& tokens) const {
    std::vector<uint32_t> contexts = analyze(tokens);
    std::string out;
    for (size_t i = 0; i < tokens.size(); ++i) {
        out += "  \"" + tokens[i] + "\"";
        out += std::string("  class=") + tokenClassName(m_symbolTable.classOf(tokens[i]));
        out += "  context=" + describeContext(contexts[i]);
        out += "\n";
    }
    return out;
}
