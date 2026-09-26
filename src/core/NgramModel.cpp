#include "core/NgramModel.h"
#include "core/BanglaText.h"

#include <cmath>

namespace {
/// Stupid-backoff penalties. Values are conventional; the resolver only compares
/// candidates with each other, so their absolute scale is irrelevant.
constexpr double kBigramBackoff = 0.4;
constexpr double kUnigramBackoff = 0.4 * 0.4;
/// Floor for a character never seen at all, so one unknown letter does not make a
/// candidate infinitely bad and swamp every other difference.
constexpr double kUnseenLogProb = -12.0;
} // namespace

void NgramModel::clear() {
    m_trigram.clear();
    m_bigramCtx.clear();
    m_bigram.clear();
    m_unigramCtx.clear();
    m_unigram.clear();
    m_totalTokens = 0.0;
}

void NgramModel::train(const WordDictionary& dictionary) {
    clear();

    for (const auto& entry : dictionary.topWords(0)) {
        // log(frequency) rather than frequency: raw counts let a handful of pronouns
        // dominate every statistic, which would make the model describe those words
        // rather than the language.
        const double weight = std::log(static_cast<double>(entry.frequency) + 1.0);
        if (weight <= 0.0) {
            continue;
        }

        std::vector<char32_t> cps = WordDictionary::toCodepoints(
            BanglaText::normalize(entry.word));
        if (cps.empty()) {
            continue;
        }

        // Boundary markers let the model learn what starts and ends a word, which is a
        // real constraint in Bengali: ং never begins one, ঁ never does either.
        cps.insert(cps.begin(), kStart);
        cps.insert(cps.begin(), kStart);
        cps.push_back(kEnd);

        for (size_t i = 2; i < cps.size(); ++i) {
            const char32_t a = cps[i - 2];
            const char32_t b = cps[i - 1];
            const char32_t c = cps[i];

            m_trigram[key3(a, b, c)] += weight;
            m_bigramCtx[key2(a, b)] += weight;
            m_bigram[key2(b, c)] += weight;
            m_unigramCtx[b] += weight;
            m_unigram[c] += weight;
            m_totalTokens += weight;
        }
    }
}

double NgramModel::logProbability(const std::string& word) const {
    if (!trained() || word.empty()) {
        return kUnseenLogProb;
    }

    std::vector<char32_t> cps = WordDictionary::toCodepoints(BanglaText::normalize(word));
    if (cps.empty()) {
        return kUnseenLogProb;
    }

    cps.insert(cps.begin(), kStart);
    cps.insert(cps.begin(), kStart);
    cps.push_back(kEnd);

    double total = 0.0;
    for (size_t i = 2; i < cps.size(); ++i) {
        const char32_t a = cps[i - 2];
        const char32_t b = cps[i - 1];
        const char32_t c = cps[i];

        double probability = 0.0;

        // Trigram, if this exact context was ever observed.
        auto triIt = m_trigram.find(key3(a, b, c));
        auto triCtx = m_bigramCtx.find(key2(a, b));
        if (triIt != m_trigram.end() && triCtx != m_bigramCtx.end() && triCtx->second > 0.0) {
            probability = triIt->second / triCtx->second;
        } else {
            // Back off to the bigram.
            auto biIt = m_bigram.find(key2(b, c));
            auto biCtx = m_unigramCtx.find(b);
            if (biIt != m_bigram.end() && biCtx != m_unigramCtx.end() && biCtx->second > 0.0) {
                probability = kBigramBackoff * (biIt->second / biCtx->second);
            } else {
                // And finally to how common the character is at all.
                auto uniIt = m_unigram.find(c);
                if (uniIt != m_unigram.end() && m_totalTokens > 0.0) {
                    probability = kUnigramBackoff * (uniIt->second / m_totalTokens);
                }
            }
        }

        total += (probability > 0.0) ? std::log(probability) : kUnseenLogProb;
    }

    return total;
}
