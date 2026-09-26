#pragma once

#include "core/NgramModel.h"

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
    /**
     * @brief Turns on the statistical fallback for words the dictionary does not contain.
     *
     * Whole-word lookup is all-or-nothing: measured on a held-out split it scored 82.8%
     * on words it had memorised and 50.0% on words it had not. The model supplies an
     * ordering for that second case, where previously the resolver fell back to candidate
     * zero and effectively guessed.
     *
     * Must outlive the resolver. Passing nullptr disables the tier.
     */
    void setNgramModel(const NgramModel* model) { m_ngram = model; reset(); }
    const NgramModel* getNgramModel() const { return m_ngram; }

    /// Enables morphological stem lookup for inflected forms. Default on.
    void setMorphologyEnabled(bool enabled) { m_morphology = enabled; reset(); }

    /**
     * @brief How much better than the default spelling an n-gram candidate must score
     *        before it is allowed to override it.
     *
     * Not a tuning knob for its own sake. Candidate order in phonetic_rules.json is itself
     * a linguistic prior -- শ before ষ before স is a statement about which is likelier --
     * and measurement showed an unconstrained trigram model overriding that prior made
     * accuracy *worse*, not better. A margin means the model only speaks when it has
     * something substantial to say.
     */
    void setNgramMargin(double margin) { m_ngramMargin = margin; reset(); }
    double getNgramMargin() const { return m_ngramMargin; }

    /**
     * @brief Bounds on the combination search.
     *
     * The search is exponential in the number of ambiguous tokens, so it must be bounded
     * -- it runs inside a keyboard hook callback that Windows unhooks without warning if
     * it overruns. But the original bounds (5 tokens, 64 combinations) were hit in 30.8%
     * of all resolution failures: the resolver was giving up before reaching the right
     * answer. Longer words are both more likely to be ambiguous in several places and more
     * likely to be unambiguous once assembled, which is exactly where truncation costs
     * most.
     */
    void setSearchLimits(size_t maxTokens, size_t maxCombinations) {
        m_maxTokens = maxTokens;
        m_maxCombinations = maxCombinations;
        reset();
    }

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
    const NgramModel* m_ngram = nullptr;
    bool m_morphology = true;
    double m_ngramMargin = 2.0;
    size_t m_maxTokens = 8;
    size_t m_maxCombinations = 512;

    mutable std::vector<std::string> m_cachedTokens;
    mutable std::vector<size_t> m_resolvedIndices;

    void evaluateWord(const std::vector<Candidate>& allCandidates) const;
};
