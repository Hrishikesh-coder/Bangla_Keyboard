#include "core/CandidateResolver.h"
#include "core/Morphology.h"
#include "core/WordDictionary.h"
#include "core/UnicodeComposer.h"
#include <algorithm>

size_t DefaultCandidateResolver::resolve(const Candidate& /*candidate*/,
                                         size_t /*tokenIndex*/,
                                         const std::vector<Candidate>& /*allCandidates*/) {
    // The baseline educational implementation always selects candidate index 0.
    return 0;
}

DictionaryCandidateResolver::DictionaryCandidateResolver(const WordDictionary* dictionary)
    : m_dictionary(dictionary) {
}

void DictionaryCandidateResolver::reset() const {
    m_cachedTokens.clear();
    m_resolvedIndices.clear();
}

size_t DictionaryCandidateResolver::resolve(const Candidate& candidate,
                                            size_t tokenIndex,
                                            const std::vector<Candidate>& allCandidates) {
    if (!m_dictionary || m_dictionary->size() == 0 || allCandidates.empty()) {
        return 0;
    }

    if (candidate.options.size() <= 1) {
        return 0;
    }

    // Check if the current cached resolution matches allCandidates
    bool match = (m_cachedTokens.size() == allCandidates.size());
    if (match) {
        for (size_t i = 0; i < allCandidates.size(); ++i) {
            if (m_cachedTokens[i] != allCandidates[i].romanToken) {
                match = false;
                break;
            }
        }
    }

    if (!match) {
        evaluateWord(allCandidates);
    }

    if (tokenIndex < m_resolvedIndices.size()) {
        return m_resolvedIndices[tokenIndex];
    }

    return 0;
}

void DictionaryCandidateResolver::evaluateWord(const std::vector<Candidate>& allCandidates) const {
    m_cachedTokens.clear();
    m_resolvedIndices.assign(allCandidates.size(), 0);

    for (const auto& c : allCandidates) {
        m_cachedTokens.push_back(c.romanToken);
    }

    if (!m_dictionary || m_dictionary->size() == 0) {
        return;
    }

    // Identify ambiguous tokens
    std::vector<size_t> ambiguousIndices;
    for (size_t i = 0; i < allCandidates.size(); ++i) {
        if (allCandidates[i].options.size() > 1) {
            ambiguousIndices.push_back(i);
        }
    }

    if (ambiguousIndices.empty()) {
        return;
    }

    // Cap the search to at most 5 ambiguous tokens and 64 total combinations to keep it ultra fast
    size_t activeCount = std::min<size_t>(ambiguousIndices.size(), m_maxTokens);
    size_t totalCombinations = 1;
    for (size_t i = 0; i < activeCount; ++i) {
        size_t optCount = allCandidates[ambiguousIndices[i]].options.size();
        totalCombinations *= optCount;
        if (totalCombinations > m_maxCombinations) {
            totalCombinations = m_maxCombinations;
            break;
        }
    }

    uint32_t bestExactFreq = 0;
    std::vector<size_t> bestExactSelection(allCandidates.size(), 0);

    uint32_t bestPrefixFreq = 0;
    std::vector<size_t> bestPrefixSelection(allCandidates.size(), 0);

    // Tier between exact and prefix: an inflected form whose stem is a known word.
    uint32_t bestStemFreq = 0;
    std::vector<size_t> bestStemSelection(allCandidates.size(), 0);

    // Last tier: how plausible the spelling is as Bengali, for words nothing else knows.
    double bestNgramScore = 0.0;
    bool haveNgramScore = false;
    std::vector<size_t> bestNgramSelection(allCandidates.size(), 0);

    UnicodeComposer composer;
    std::vector<Candidate> testCandidates = allCandidates;

    std::vector<size_t> currentOptions(activeCount, 0);
    bool done = false;
    size_t evaluatedCount = 0;

    while (!done && evaluatedCount < totalCombinations) {
        ++evaluatedCount;

        // Apply current option choices
        for (size_t i = 0; i < activeCount; ++i) {
            size_t tokenIdx = ambiguousIndices[i];
            testCandidates[tokenIdx].selectedIndex = currentOptions[i];
        }

        std::string composed = composer.compose(testCandidates);

        // 1. Exact match check
        uint32_t exactFreq = m_dictionary->getFrequency(composed);
        if (exactFreq > bestExactFreq) {
            bestExactFreq = exactFreq;
            for (size_t i = 0; i < allCandidates.size(); ++i) {
                bestExactSelection[i] = testCandidates[i].selectedIndex;
            }
        }

        // 2. Morphological match: strip a suffix and look the stem up. বাড়িতে is not in
        //    most word lists; বাড়ি is in all of them, and the ambiguous letters live in
        //    the stem, not the ending.
        if (bestExactFreq == 0 && m_morphology) {
            for (const auto& stem : Morphology::candidateStems(composed)) {
                uint32_t stemFreq = m_dictionary->getFrequency(stem);
                if (stemFreq > bestStemFreq) {
                    bestStemFreq = stemFreq;
                    for (size_t i = 0; i < allCandidates.size(); ++i) {
                        bestStemSelection[i] = testCandidates[i].selectedIndex;
                    }
                    break; // longest suffix stripped first, so the first hit is the best
                }
            }
        }

        // 3. Character-level plausibility, scored for every combination so that a ranking
        //    exists even when no lookup of any kind succeeds.
        if (m_ngram && m_ngram->trained()) {
            const double score = m_ngram->logProbability(composed);
            if (!haveNgramScore || score > bestNgramScore) {
                haveNgramScore = true;
                bestNgramScore = score;
                for (size_t i = 0; i < allCandidates.size(); ++i) {
                    bestNgramSelection[i] = testCandidates[i].selectedIndex;
                }
            }
        }

        // 4. Prefix match check (if no exact match yet)
        if (bestExactFreq == 0) {
            auto predictions = m_dictionary->predict(composed, 1);
            if (!predictions.empty() && predictions[0].frequency > bestPrefixFreq) {
                bestPrefixFreq = predictions[0].frequency;
                for (size_t i = 0; i < allCandidates.size(); ++i) {
                    bestPrefixSelection[i] = testCandidates[i].selectedIndex;
                }
            }
        }

        // Advance odometer of option combinations
        size_t pos = activeCount;
        while (pos > 0) {
            --pos;
            size_t tokenIdx = ambiguousIndices[pos];
            if (currentOptions[pos] + 1 < allCandidates[tokenIdx].options.size()) {
                currentOptions[pos]++;
                break;
            } else {
                currentOptions[pos] = 0;
                if (pos == 0) {
                    done = true;
                }
            }
        }
    }

    std::string defaultComposed = composer.compose(allCandidates);
    bool defaultIsPrefix = m_dictionary->hasPrefix(defaultComposed);

    // Tiers in descending order of evidence. A whole word beats a stem; a stem beats a
    // prefix; a prefix beats mere character plausibility. The n-gram tier only fires when
    // every lookup has failed, which is exactly the case where the old resolver returned
    // candidate zero and hoped.
    if (bestExactFreq > 0) {
        m_resolvedIndices = bestExactSelection;
    } else if (bestStemFreq > 0) {
        m_resolvedIndices = bestStemSelection;
    } else if (!defaultIsPrefix && bestPrefixFreq > 0) {
        m_resolvedIndices = bestPrefixSelection;
    } else if (haveNgramScore) {
        // Only override the default ordering when the model is clearly more confident.
        // Without this the model was a net loss: it replaced a hand-tuned prior with a
        // marginally-different guess, and lost more than it won.
        const double defaultScore = m_ngram->logProbability(defaultComposed);
        if (bestNgramScore > defaultScore + m_ngramMargin) {
            m_resolvedIndices = bestNgramSelection;
        }
    }
}
