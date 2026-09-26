#include "core/BanglaText.h"

namespace {

/// The three composition-excluded Bengali letters, as UTF-8, with their decompositions.
struct Mapping {
    const char* precomposed;
    const char* decomposed;
};

// U+09DC -> U+09A1 U+09BC, U+09DD -> U+09A2 U+09BC, U+09DF -> U+09AF U+09BC
const Mapping kNuktaLetters[] = {
    { "\xE0\xA7\x9C", "\xE0\xA6\xA1\xE0\xA6\xBC" },
    { "\xE0\xA7\x9D", "\xE0\xA6\xA2\xE0\xA6\xBC" },
    { "\xE0\xA7\x9F", "\xE0\xA6\xAF\xE0\xA6\xBC" }
};

} // namespace

namespace BanglaText {

std::string normalize(const std::string& text) {
    // All three precomposed letters are three bytes beginning E0 A7, so the common case --
    // text containing none of them -- costs one scan and no allocation beyond the copy.
    std::string out;
    out.reserve(text.size());

    size_t i = 0;
    while (i < text.size()) {
        bool replaced = false;
        if (i + 3 <= text.size()) {
            for (const auto& mapping : kNuktaLetters) {
                if (text.compare(i, 3, mapping.precomposed) == 0) {
                    out += mapping.decomposed;
                    i += 3;
                    replaced = true;
                    break;
                }
            }
        }
        if (!replaced) {
            out += text[i];
            ++i;
        }
    }
    return out;
}

bool needsNormalization(const std::string& text) {
    for (const auto& mapping : kNuktaLetters) {
        if (text.find(mapping.precomposed) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace BanglaText
