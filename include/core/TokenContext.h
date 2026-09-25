#pragma once

#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Linguistic class of a phonetic token, used to reason about neighbours.
 *
 * The class is declared in phonetic_rules.json ("class": "vowel") or, when omitted,
 * derived automatically from the first Bengali codepoint of the token's primary candidate.
 */
enum class TokenClass {
    UNKNOWN,
    VOWEL,
    CONSONANT,
    MODIFIER,     ///< ং (Anusvara), ঃ (Visarga), ঁ (Chandrabindu), ৎ (Khanda Ta)
    PUNCTUATION
};

const char* tokenClassName(TokenClass cls);
TokenClass tokenClassFromString(const std::string& name);

/**
 * @brief Bit flags describing where a token sits relative to its neighbours.
 *
 * A token's context is a bitmask; a contextual rule variant fires only when every bit it
 * requires is present. Flags are deliberately about the *token stream*, not about Bengali
 * output, so they can be computed before any candidate has been chosen.
 */
namespace Ctx {
enum Flags : uint32_t {
    NONE             = 0,
    WORD_START       = 1u << 0,
    WORD_END         = 1u << 1,
    AFTER_CONSONANT  = 1u << 2,
    AFTER_VOWEL      = 1u << 3,
    AFTER_MODIFIER   = 1u << 4,
    BEFORE_CONSONANT = 1u << 5,
    BEFORE_VOWEL     = 1u << 6,
    BEFORE_MODIFIER  = 1u << 7
};
}

/// Parses a context flag name such as "after_consonant". Returns Ctx::NONE if unknown.
uint32_t contextFlagFromString(const std::string& name);

/// Renders a context bitmask as a readable list, e.g. "word_start|before_consonant".
std::string describeContext(uint32_t mask);
