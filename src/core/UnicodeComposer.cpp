#include "core/UnicodeComposer.h"

// Bengali Unicode constants
static constexpr char32_t BENGALI_VIRAMA = 0x09CD; // ্ (hasant / virama)
static constexpr char32_t BENGALI_VOWEL_A = 0x0985; // অ (inherent vowel, no matra)

bool UnicodeComposer::isBengaliConsonant(char32_t cp) {
    // Standard consonants: ক (0x0995) to হ (0x09B9)
    if (cp >= 0x0995 && cp <= 0x09B9) {
        // 0x09BA and 0x09BB are unassigned in Bengali block
        if (cp == 0x09BA || cp == 0x09BB) return false;
        return true;
    }
    // Additional consonants: ড় (0x09DC), ঢ় (0x09DD), য় (0x09DF)
    if (cp == 0x09DC || cp == 0x09DD || cp == 0x09DF) {
        return true;
    }
    return false;
}

bool UnicodeComposer::isBengaliIndependentVowel(char32_t cp) {
    switch (cp) {
        case 0x0985: // অ
        case 0x0986: // আ
        case 0x0987: // ই
        case 0x0988: // ঈ
        case 0x0989: // উ
        case 0x098A: // ঊ
        case 0x098B: // ঋ
        case 0x098C: // ঌ
        case 0x098F: // এ
        case 0x0990: // ঐ
        case 0x0993: // ও
        case 0x0994: // ঔ
            return true;
        default:
            return false;
    }
}

bool UnicodeComposer::isBengaliDependentVowel(char32_t cp) {
    // া (0x09BE) to ৌ (0x09CC), including ৄ (0x09C4)
    return (cp >= 0x09BE && cp <= 0x09CC);
}

char32_t UnicodeComposer::toDependentVowel(char32_t independentVowel) {
    switch (independentVowel) {
        case 0x0986: return 0x09BE; // আ -> া
        case 0x0987: return 0x09BF; // ই -> ি
        case 0x0988: return 0x09C0; // ঈ -> ী
        case 0x0989: return 0x09C1; // উ -> ু
        case 0x098A: return 0x09C2; // ঊ -> ূ
        case 0x098B: return 0x09C3; // ঋ -> ৃ
        case 0x098F: return 0x09C7; // এ -> ে
        case 0x0990: return 0x09C8; // ঐ -> ৈ
        case 0x0993: return 0x09CB; // ও -> ো
        case 0x0994: return 0x09CC; // ঔ -> ৌ
        default:     return 0;      // 0 means no dependent form (e.g. অ)
    }
}

std::vector<char32_t> UnicodeComposer::utf8ToCodepoints(const std::string& utf8Str) {
    std::vector<char32_t> codepoints;
    size_t i = 0;
    const size_t len = utf8Str.size();

    while (i < len) {
        unsigned char c = static_cast<unsigned char>(utf8Str[i]);
        char32_t cp = 0;
        size_t extraBytes = 0;

        if (c <= 0x7F) {
            cp = c;
            extraBytes = 0;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            extraBytes = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            extraBytes = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            extraBytes = 3;
        } else {
            // Invalid leading byte, skip
            ++i;
            continue;
        }

        if (i + extraBytes >= len) {
            break; // Truncated UTF-8 sequence
        }

        for (size_t b = 1; b <= extraBytes; ++b) {
            unsigned char nextC = static_cast<unsigned char>(utf8Str[i + b]);
            if ((nextC & 0xC0) != 0x80) {
                // Invalid continuation byte
                cp = 0xFFFD;
                break;
            }
            cp = (cp << 6) | (nextC & 0x3F);
        }

        codepoints.push_back(cp);
        i += 1 + extraBytes;
    }

    return codepoints;
}

std::string UnicodeComposer::codepointToUtf8(char32_t cp) {
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

std::string UnicodeComposer::codepointsToUtf8(const std::vector<char32_t>& codepoints) {
    std::string result;
    for (char32_t cp : codepoints) {
        result += codepointToUtf8(cp);
    }
    return result;
}

std::string UnicodeComposer::compose(const std::vector<Candidate>& candidates) const {
    std::vector<std::string> components;
    components.reserve(candidates.size());
    for (const auto& c : candidates) {
        components.push_back(c.selected());
    }
    return composeStrings(components);
}

std::string UnicodeComposer::composeStrings(const std::vector<std::string>& components) const {
    std::vector<char32_t> output;
    bool previousWasConsonant = false;

    for (const auto& compStr : components) {
        // Epsilon / no-output candidate
        if (compStr.empty()) {
            continue;
        }

        std::vector<char32_t> codepoints = utf8ToCodepoints(compStr);
        for (char32_t cp : codepoints) {
            if (isBengaliConsonant(cp)) {
                if (previousWasConsonant) {
                    // Consonant immediately following consonant: insert Virama (্)
                    output.push_back(BENGALI_VIRAMA);
                }
                output.push_back(cp);
                previousWasConsonant = true;
            } else if (isBengaliIndependentVowel(cp)) {
                if (previousWasConsonant) {
                    // Independent vowel following a consonant
                    if (cp == BENGALI_VOWEL_A) {
                        // "অ" represents the inherent vowel. No matra glyph is added in Bengali.
                        // However, it satisfies the syllable's vowel, so subsequent consonants
                        // will not form a conjunct with this consonant.
                        previousWasConsonant = false;
                    } else {
                        char32_t matra = toDependentVowel(cp);
                        if (matra != 0) {
                            output.push_back(matra);
                        } else {
                            output.push_back(cp);
                        }
                        previousWasConsonant = false;
                    }
                } else {
                    // Independent vowel at start of word or after another vowel
                    output.push_back(cp);
                    previousWasConsonant = false;
                }
            } else if (isBengaliDependentVowel(cp)) {
                // Directly provided matra
                output.push_back(cp);
                previousWasConsonant = false;
            } else if (cp == 0x09BC) {
                // NUKTA. It is a modifier on the consonant it follows, not a character in
                // its own right: ড + ় is still the single letter ড়, and a vowel after it
                // must still become a matra. Clearing the flag here made "baRi" compose as
                // বাড়ই instead of বাড়ি, and "meye" as মেয়এ instead of মেয়ে -- the vowel
                // was emitted in its independent form because the composer had forgotten
                // it was standing on a consonant.
                output.push_back(cp);
                // previousWasConsonant deliberately left unchanged.
            } else {
                // Signs (Chandrabindu, Anusvara, Visarga), Khanda Ta (ৎ), numbers, punctuation
                output.push_back(cp);
                previousWasConsonant = false;
            }
        }
    }

    return codepointsToUtf8(output);
}
