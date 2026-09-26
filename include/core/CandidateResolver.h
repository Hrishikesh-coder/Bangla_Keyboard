#pragma once

#include "Candidate.h"
#include <memory>
#include <vector>

/**
 * @brief Abstract interface for selecting the most appropriate candidate index.
 *
 * Designed as an educational extension point:
 * A future resolver could inspect surrounding context, word frequencies,
 * a dictionary, or a machine learning model, without altering the tokenizer,
 * symbol table, or keyboard hook.
 */
class ICandidateResolver {
public:
    virtual ~ICandidateResolver() = default;

    /**
     * @brief Determines which candidate index should be selected for the given token.
     * @param candidate The Candidate holding available options.
     * @param tokenIndex Index of this candidate within the word/sentence sequence.
     * @param allCandidates Full sequence of candidates for context inspection.
     * @return The chosen index into candidate.options.
     */
    virtual size_t resolve(const Candidate& candidate,
                           size_t tokenIndex,
                           const std::vector<Candidate>& allCandidates) = 0;
};

/**
 * @brief Baseline candidate resolver that consistently selects candidate index 0.
 */
class DefaultCandidateResolver : public ICandidateResolver {
public:
    size_t resolve(const Candidate& candidate,
                   size_t tokenIndex,
                   const std::vector<Candidate>& allCandidates) override;
};

class WordDictionary;

/**
 * @brief Dictionary-aware candidate resolver that inspects combinations of ambiguous
 *        candidates against WordDictionary to automatically resolve homophones
 *        (e.g., শ/ষ/স, ন/ণ, ই/ঈ) into valid, high-frequency Bengali words.
 *
 * Implements the documented extension point from docs/LIMITATIONS.md #2:
 * "The proper fix, which we have left as a documented extension point: implement
 *  ICandidateResolver over a frequency list... so the resolver chooses using
 *  the surrounding word rather than always taking index 0."
 */
class DictionaryCandidateResolver : public ICandidateResolver {
public:
    DictionaryCandidateResolver() = default;
    explicit DictionaryCandidateResolver(const WordDictionary* dictionary);

    void setDictionary(const WordDictionary* dictionary) {
        m_dictionary = dictionary;
        reset();
    }
    const WordDictionary* getDictionary() const { return m_dictionary; }

    size_t resolve(const Candidate& candidate,
                   size_t tokenIndex,
                   const std::vector<Candidate>& allCandidates) override;

    void reset() const;

private:
    const WordDictionary* m_dictionary = nullptr;

    mutable std::vector<std::string> m_cachedTokens;
    mutable std::vector<size_t> m_resolvedIndices;

    void evaluateWord(const std::vector<Candidate>& allCandidates) const;
};
