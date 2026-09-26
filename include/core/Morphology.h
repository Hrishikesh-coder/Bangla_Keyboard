#pragma once

#include <string>
#include <vector>

/**
 * @brief Crude suffix stripping, to find a known stem inside an unknown word.
 *
 * Bangla is agglutinative. বাড়ি is in any word list; বাড়িতে, বাড়িগুলো and বাড়িওয়ালা may
 * not be. করা is common; করছিলাম, করবেন and করিয়েছিল are each separate surface forms. A
 * whole-word dictionary therefore misses a large share of perfectly ordinary text, and the
 * miss is not random -- it falls hardest on inflected verbs, which are everywhere.
 *
 * This is deliberately *not* a morphological analyser. It does not know parts of speech,
 * it does not handle sandhi, and it will happily strip a suffix that is really part of the
 * stem. That is acceptable because of how it is used: the caller tries each candidate stem
 * against the dictionary and keeps a hit. A wrong split simply fails to match and costs
 * one lookup; it can never produce a wrong answer on its own.
 *
 * Suffixes are ordered longest first so that গুলোকে is tried before কে.
 */
namespace Morphology {

/**
 * @brief Candidate stems for a word, longest suffix removed first.
 *
 * The word itself is never included -- the caller has already tried that. Stems shorter
 * than two characters are dropped, since a one-letter "stem" matches far too much.
 */
std::vector<std::string> candidateStems(const std::string& word);

/// The suffix list, exposed for tests and for anyone extending it.
const std::vector<std::string>& suffixes();

} // namespace Morphology
