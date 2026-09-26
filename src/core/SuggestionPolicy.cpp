#include "core/SuggestionPolicy.h"

namespace {
/// Below this many Bengali letters, an edit distance of two reaches most of the dictionary,
/// so a "correction" is indistinguishable from a random word.
constexpr size_t kMinimumLengthToCorrect = 3;
} // namespace

SuggestionSet suggestFor(const WordDictionary& dictionary,
                         const std::string& composed,
                         bool wordFinished,
                         size_t completionLimit,
                         size_t correctionLimit) {
    SuggestionSet result;

    if (composed.empty() || dictionary.size() == 0) {
        return result;
    }

    if (!wordFinished) {
        // Still typing: only ever extend what is there. Never correct a prefix.
        for (const auto& hit : dictionary.predict(composed, completionLimit)) {
            if (hit.word != composed) {
                result.words.push_back(hit.word);
            }
        }
        if (!result.words.empty()) {
            result.kind = SuggestionKind::Completion;
        }
        return result;
    }

    // The word is finished. If it is a real word there is nothing to say; silence is the
    // correct output, and the commonest case.
    if (dictionary.contains(composed)) {
        return result;
    }

    if (WordDictionary::toCodepoints(composed).size() < kMinimumLengthToCorrect) {
        return result;
    }

    for (const auto& hit : dictionary.correct(composed, 2, correctionLimit)) {
        if (hit.word != composed) {
            result.words.push_back(hit.word);
        }
    }
    if (!result.words.empty()) {
        result.kind = SuggestionKind::Correction;
    }
    return result;
}
