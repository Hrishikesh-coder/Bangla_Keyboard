#pragma once

#include "core/WordDictionary.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief A character trigram model over Bengali, used to spell words never seen before.
 *
 * DictionaryCandidateResolver answers "is this combination a word I know?". That works
 * beautifully when the word is in the dictionary and not at all when it is not: measured
 * on a held-out split, whole-word lookup scored 82.8% on words it had memorised and 50.0%
 * on words it had not. Dictionary coverage can always be grown, but it can never be
 * complete -- names, loanwords and inflections guarantee that.
 *
 * This closes part of that gap without needing the word. Bengali orthography is heavily
 * constrained at the character level: ষ follows ক্ and precedes ্ট far more often than
 * chance; ণ clusters after র; ঈ is rare outside tatsama vocabulary. Those regularities are
 * learnable from the same word list, and they hold for words the list does not contain.
 *
 * Trigram counts with stupid backoff -- trigram, then bigram, then unigram, each fallback
 * multiplied by a fixed penalty. Chosen over proper smoothing because it needs one pass,
 * no held-out tuning set, and the absolute probabilities never matter here: the resolver
 * only ever compares candidates against each other.
 *
 * Training weights each word by log(frequency) rather than by frequency. Raw counts let a
 * handful of pronouns dominate every statistic in the model; log flattens that while still
 * letting common spellings outrank rare ones.
 */
class NgramModel {
public:
    NgramModel() = default;

    /// Builds the model from a word list. Cheap enough to run at startup.
    void train(const WordDictionary& dictionary);

    void clear();

    /// True once trained with at least one word.
    bool trained() const { return m_totalTokens > 0; }

    /// Number of distinct trigram contexts observed; useful for reporting.
    size_t contextCount() const { return m_trigram.size(); }

    /**
     * @brief Log-likelihood of a Bengali string under the model.
     *
     * Higher is more plausible. Values are comparable between strings of the *same* length
     * only; the resolver always compares spellings of one word, so that holds.
     */
    double logProbability(const std::string& word) const;

private:
    /// Packs up to two codepoints into one key. Bengali fits in 16 bits with room to spare.
    static uint64_t key2(char32_t a, char32_t b) {
        return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
    }
    static uint64_t key3(char32_t a, char32_t b, char32_t c) {
        return (static_cast<uint64_t>(a) << 42) ^ (static_cast<uint64_t>(b) << 21)
             ^ static_cast<uint64_t>(c);
    }

    /// Sentinel codepoints for word start and end, outside the Bengali block.
    static constexpr char32_t kStart = 0x1;
    static constexpr char32_t kEnd = 0x2;

    std::unordered_map<uint64_t, double> m_trigram;   ///< (a,b,c) -> weight
    std::unordered_map<uint64_t, double> m_bigramCtx; ///< (a,b)   -> total weight after
    std::unordered_map<uint64_t, double> m_bigram;    ///< (b,c)   -> weight
    std::unordered_map<char32_t, double> m_unigramCtx;///< (b)     -> total weight after
    std::unordered_map<char32_t, double> m_unigram;   ///< (c)     -> weight
    double m_totalTokens = 0.0;
};
