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
