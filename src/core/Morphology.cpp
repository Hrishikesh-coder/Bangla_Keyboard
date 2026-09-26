#include "core/Morphology.h"
#include "core/WordDictionary.h"

#include <algorithm>

namespace Morphology {

const std::vector<std::string>& suffixes() {
    // Case, plural and verb-inflection endings, by frequency of use rather than by any
    // grammatical taxonomy. Longest first: candidateStems relies on that ordering.
    static const std::vector<std::string> kSuffixes = {
        // plural + case, the longest and most specific
        "গুলোকে", "গুলোতে", "গুলোর", "দেরকে", "গুলির", "গুলো", "গুলি",
        "দিগের", "দেরও", "দের",
        // verb inflections
        "ছিলাম", "ছিলেন", "ছিলে", "ছিল", "য়েছিল", "য়েছে", "েছিলাম", "েছিল", "েছেন", "েছে",
        "বেন", "বাম", "বে", "বো", "ব",
        "চ্ছি", "চ্ছে", "ছি", "ছে", "ছ",
        "লাম", "লেন", "লে", "ল",
        "ইয়ে", "িয়ে", "য়ে",
        // case endings
        "তেও", "রাও", "কেও",
        "েতে", "তে", "েরা", "রা", "ের", "র", "কে", "য়", "ে",
        // emphatic and definite particles
        "টাকে", "টিকে", "খানা", "খানি", "টার", "টির", "টা", "টি", "টু",
        "ও", "ই"
    };
    return kSuffixes;
}

std::vector<std::string> candidateStems(const std::string& word) {
    std::vector<std::string> stems;
    if (word.empty()) {
        return stems;
    }

    for (const auto& suffix : suffixes()) {
        if (suffix.size() >= word.size()) {
            continue; // stripping it would leave nothing
        }
        if (word.compare(word.size() - suffix.size(), suffix.size(), suffix) != 0) {
            continue;
        }

        std::string stem = word.substr(0, word.size() - suffix.size());

        // A one-character stem matches far too much to be evidence of anything.
        if (WordDictionary::toCodepoints(stem).size() < 2) {
            continue;
        }

        // A stem must not end mid-cluster: a trailing hasant means the suffix took the
        // second half of a conjunct with it, which is never a real split.
        auto cps = WordDictionary::toCodepoints(stem);
        if (!cps.empty() && cps.back() == 0x09CD) {
            continue;
        }

        if (std::find(stems.begin(), stems.end(), stem) == stems.end()) {
            stems.push_back(stem);
        }
    }

    return stems;
}

} // namespace Morphology
