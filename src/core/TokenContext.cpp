#include "core/TokenContext.h"

const char* tokenClassName(TokenClass cls) {
    switch (cls) {
        case TokenClass::VOWEL:       return "vowel";
        case TokenClass::CONSONANT:   return "consonant";
        case TokenClass::MODIFIER:    return "modifier";
        case TokenClass::PUNCTUATION: return "punctuation";
        default:                      return "unknown";
    }
}

TokenClass tokenClassFromString(const std::string& name) {
    if (name == "vowel")       return TokenClass::VOWEL;
    if (name == "consonant")   return TokenClass::CONSONANT;
    if (name == "modifier")    return TokenClass::MODIFIER;
    if (name == "punctuation") return TokenClass::PUNCTUATION;
    return TokenClass::UNKNOWN;
}

uint32_t contextFlagFromString(const std::string& name) {
    if (name == "word_start")       return Ctx::WORD_START;
    if (name == "word_end")         return Ctx::WORD_END;
    if (name == "after_consonant")  return Ctx::AFTER_CONSONANT;
    if (name == "after_vowel")      return Ctx::AFTER_VOWEL;
    if (name == "after_modifier")   return Ctx::AFTER_MODIFIER;
    if (name == "before_consonant") return Ctx::BEFORE_CONSONANT;
    if (name == "before_vowel")     return Ctx::BEFORE_VOWEL;
    if (name == "before_modifier")  return Ctx::BEFORE_MODIFIER;
    return Ctx::NONE;
}

std::string describeContext(uint32_t mask) {
    static const struct { uint32_t flag; const char* name; } kNames[] = {
        { Ctx::WORD_START,       "word_start" },
        { Ctx::WORD_END,         "word_end" },
        { Ctx::AFTER_CONSONANT,  "after_consonant" },
        { Ctx::AFTER_VOWEL,      "after_vowel" },
        { Ctx::AFTER_MODIFIER,   "after_modifier" },
        { Ctx::BEFORE_CONSONANT, "before_consonant" },
        { Ctx::BEFORE_VOWEL,     "before_vowel" },
        { Ctx::BEFORE_MODIFIER,  "before_modifier" }
    };

    std::string out;
    for (const auto& entry : kNames) {
        if (mask & entry.flag) {
            if (!out.empty()) out += "|";
            out += entry.name;
        }
    }
    return out.empty() ? "none" : out;
}
