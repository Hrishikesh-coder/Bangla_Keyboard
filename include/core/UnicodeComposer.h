#pragma once

#include "Candidate.h"
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Constructs valid Bengali Unicode strings from logical candidate components.
 *
 * Operates purely on logical Unicode characters. Does not draw glyphs or implement
 * font shaping. Correctly handles:
 * - Bengali virama / hasant (্, U+09CD) insertion between adjacent consonants to form conjuncts.
 * - Dependent vowel signs (matras/kar) when an independent vowel follows a consonant.
 * - Inherent vowel handling (e.g., "অ" after a consonant prevents virama without adding an invalid matra).
 * - Epsilon / empty candidates (produces no output).
 */
class UnicodeComposer {
public:
    UnicodeComposer() = default;

    /**
     * @brief Composes a sequence of candidates into a single composed Bengali UTF-8 string.
     * @param candidates Sequence of Candidate objects whose selected() strings are used.
     * @return Final composed Bengali UTF-8 string.
     */
    std::string compose(const std::vector<Candidate>& candidates) const;

    /**
     * @brief Composes a sequence of raw Bengali strings (e.g. selected candidate strings).
     * @param components Vector of Bengali Unicode UTF-8 strings.
     * @return Final composed Bengali UTF-8 string.
     */
    std::string composeStrings(const std::vector<std::string>& components) const;

    // Unicode classification helper methods (useful for tests and inspection)
    static bool isBengaliConsonant(char32_t codepoint);
    static bool isBengaliIndependentVowel(char32_t codepoint);
    static bool isBengaliDependentVowel(char32_t codepoint);
    static char32_t toDependentVowel(char32_t independentVowel);

    // UTF-8 conversion helpers
    static std::vector<char32_t> utf8ToCodepoints(const std::string& utf8Str);
    static std::string codepointsToUtf8(const std::vector<char32_t>& codepoints);
    static std::string codepointToUtf8(char32_t cp);
};
