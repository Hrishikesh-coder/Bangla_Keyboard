#pragma once

#include "core/WordDictionary.h"

#include <string>
#include <vector>

/// What the suggestion row is currently offering.
enum class SuggestionKind {
    None,
    /// The word is unfinished; these extend it. Clicking one completes what you started.
    Completion,
    /// The word is finished and unrecognised; these replace it. Clicking one discards it.
    Correction
};

struct SuggestionSet {
    SuggestionKind kind = SuggestionKind::None;
    std::vector<std::string> words;

    bool empty() const { return words.empty(); }
};

/**
 * @brief Decides what to offer for the current composition, and of which kind.
 *
 * This lives here, apart from the keyboard hook, because it is a policy decision rather
 * than a Win32 one, and because getting it wrong is easy and invisible without tests.
 *
 * The rule that matters: **a prefix is never corrected.** While a word is still being
 * typed, most of its prefixes match no dictionary entry — typing বাংলা passes through বান,
 * which starts no word. Treating that as a misspelling offers "corrections" for a word the
 * user is typing perfectly, and the row then flips back to completions on the next
 * keystroke. A prefix that matches nothing is an unfinished word, not a wrong one.
 *
 * So:
 *   - unfinished  -> completions only, or nothing;
 *   - finished    -> nothing if the word is real; corrections only if it is not.
 *
 * Correction also needs a minimum length. Below three Bengali letters almost everything is
 * within two edits of almost everything else, so the suggestions carry no information.
 */
SuggestionSet suggestFor(const WordDictionary& dictionary,
                         const std::string& composed,
                         bool wordFinished,
                         size_t completionLimit = 4,
                         size_t correctionLimit = 3);
