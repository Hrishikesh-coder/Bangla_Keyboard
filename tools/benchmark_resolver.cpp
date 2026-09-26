// Measures how often each candidate resolver recovers the correct Bengali spelling from
// the ambiguous Roman a real typist would produce.
//
// The question this answers
// ------------------------
// Bangla phonetic input is ambiguous by construction. শ, ষ and স are homophones; so are ন
// and ণ, and ই and ঈ. A typist does not type the "correct" disambiguating form -- they
// type the easy one, `sh` for both শ and ষ, `n` for both ন and ণ -- and expect the IME to
// work out which was meant. Resolving that is DictionaryCandidateResolver's entire job.
// This measures whether it does it.
//
// Method
// ------
// 1. Take the N most frequent words from the corpus word list.
// 2. Reverse-transliterate each into the Roman a *lazy* typist would produce: wherever
//    several Bengali letters share a Roman token, use the token, not the disambiguated
//    form. That deliberately throws away the information the resolver has to recover.
// 3. Run that Roman forward through the engine and compare with the original word.
// 4. Report accuracy for DefaultCandidateResolver (always candidate 0) and for
//    DictionaryCandidateResolver.
//
// Honesty about the denominator
// -----------------------------
// Some words cannot be reconstructed by *any* resolver, because the reverse-transliterator
// has no Roman spelling for part of them, or because the rule set genuinely cannot express
// the word. Those are excluded and reported separately: scoring a resolver on words that
// are unreachable in principle would flatter both of them equally and tell us nothing.
//
// The number that matters is the accuracy on *homophone-bearing* words -- those containing
// at least one letter whose Roman token is shared. On words with no ambiguity, both
// resolvers are identical by definition, and averaging those in would dilute the result
// towards a meaningless draw.

#include "core/PhoneticEngine.h"
#include "core/WordDictionary.h"
#include "core/BanglaText.h"
#include "core/CandidateResolver.h"
#include "core/UnicodeComposer.h"
#include "core/NgramModel.h"
#include <cctype>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

std::string findConfig(const std::string& name) {
    const std::string candidates[] = { "config/" + name, "../config/" + name, name };
    for (const auto& path : candidates) {
        std::ifstream probe(path);
        if (probe.is_open()) {
            return path;
        }
    }
    return "config/" + name;
}

/**
 * A "lazy typist" reverse transliterator: Bengali word -> the Roman a user would type.
 *
 * Built from the rule table rather than hand-written, so it stays in step with
 * config/phonetic_rules.json. Where several tokens reach the same Bengali output, the
 * shortest all-lowercase one wins -- exactly the key a typist reaches for. ষ therefore
 * comes out as the shared `sh`, not the unambiguous `Sh`, which is what makes this a test
 * of the resolver rather than of the rule table.
 *
 * The walk mirrors UnicodeComposer in reverse, and it has to: a flat longest-match over
 * strings cannot reconstruct the two things the composer *inserts* rather than copies --
 * the virama between stacked consonants, and the inherent vowel that is written as nothing
 * at all. A first attempt that ignored this failed on 92% of words, which measured the
 * reverse map and not the resolver.
 */
class LazyTypist {
public:
    LazyTypist() = default;

    void record(const std::string& bengali, const std::string& roman) {
        if (bengali.empty()) {
            return;
        }
        auto codepoints = UnicodeComposer::utf8ToCodepoints(bengali);
        if (codepoints.size() > 2) {
            return; // pre-composed clusters are reconstructed from their parts instead
        }
        auto it = m_map.find(codepoints);
        if (it == m_map.end() || better(roman, it->second)) {
            m_map[codepoints] = roman;
        }
    }

    /// Reverse-transliterates a word. False when some piece has no Roman spelling.
    bool toRoman(const std::string& bengali, std::string& out) const {
        out.clear();
        const auto cps = UnicodeComposer::utf8ToCodepoints(bengali);

        size_t i = 0;
        while (i < cps.size()) {
            // A consonant may carry a nukta; ড + ় is one letter and one token.
            size_t unitLength = 1;
            if (i + 1 < cps.size() && cps[i + 1] == 0x09BC) {
                unitLength = 2;
            }
            const std::vector<char32_t> unit(cps.begin() + static_cast<long>(i),
                                             cps.begin() + static_cast<long>(i + unitLength));

            auto it = m_map.find(unit);
            if (it == m_map.end()) {
                return false;
            }
            out += it->second;
            i += unitLength;

            if (!UnicodeComposer::isBengaliConsonant(unit[0])) {
                continue; // vowels and signs carry no inherent vowel
            }

            // What follows a consonant decides whether the typist writes a vowel at all.
            if (i < cps.size() && cps[i] == 0x09CD) {
                ++i;        // virama: the next consonant stacks, so nothing is typed
            } else if (i < cps.size() && UnicodeComposer::isBengaliDependentVowel(cps[i])) {
                auto matra = m_map.find({cps[i]});
                if (matra == m_map.end()) {
                    return false;
                }
                out += matra->second;
                ++i;
            } else {
                out += kInherentVowel;  // written as nothing, typed as 'o'
            }
        }
        return !out.empty();
    }

    /// True when this unit's Roman token is shared with a different Bengali unit.
    bool isAmbiguous(const std::vector<char32_t>& unit) const {
        auto it = m_map.find(unit);
        if (it == m_map.end()) {
            return false;
        }
        size_t sharing = 0;
        for (const auto& entry : m_map) {
            if (entry.second == it->second) {
                ++sharing;
            }
        }
        return sharing > 1;
    }

    /// True when any letter of the word is ambiguous, i.e. the resolver has real work.
    bool wordIsAmbiguous(const std::string& bengali) const {
        const auto cps = UnicodeComposer::utf8ToCodepoints(bengali);
        for (size_t i = 0; i < cps.size(); ++i) {
            size_t unitLength = (i + 1 < cps.size() && cps[i + 1] == 0x09BC) ? 2 : 1;
            std::vector<char32_t> unit(cps.begin() + static_cast<long>(i),
                                       cps.begin() + static_cast<long>(i + unitLength));
            if (isAmbiguous(unit)) {
                return true;
            }
            i += unitLength - 1;
        }
        return false;
    }

private:
    static constexpr const char* kInherentVowel = "o";

    /// Prefer short, then all-lowercase, then alphabetical: the keys a typist reaches for.
    static bool better(const std::string& a, const std::string& b) {
        auto lowercase = [](const std::string& s) {
            return std::all_of(s.begin(), s.end(),
                               [](unsigned char c) { return !std::isupper(c); });
        };
        if (a.size() != b.size())         return a.size() < b.size();
        if (lowercase(a) != lowercase(b)) return lowercase(a);
        return a < b;
    }

    std::map<std::vector<char32_t>, std::string> m_map;
};

struct Score {
    size_t correct = 0;
    size_t total = 0;
    double percent() const { return total ? (100.0 * correct / total) : 0.0; }
};

} // namespace

int main(int argc, char* argv[]) {
    size_t sampleSize = 2000;
    if (argc > 1) {
        sampleSize = static_cast<size_t>(std::stoul(argv[1]));
    }
    double ngramMargin = 4.0;
    if (argc > 2) {
        ngramMargin = std::stod(argv[2]);
    }
    // Which slice of the frequency distribution to test on. The margin is tuned on one
    // slice and reported on another, so the reported number is not fitted to itself.
    size_t offset = 2000;
    if (argc > 3) {
        offset = static_cast<size_t>(std::stoul(argv[3]));
    }
    size_t maxTokens = 8;
    size_t maxCombos = 512;
    if (argc > 5) {
        maxTokens = static_cast<size_t>(std::stoul(argv[4]));
        maxCombos = static_cast<size_t>(std::stoul(argv[5]));
    }

    PhoneticEngine reference;
    if (!reference.loadRules(findConfig("phonetic_rules.json"))) {
        std::cerr << "could not load phonetic rules\n";
        return 1;
    }
    reference.loadExceptions(findConfig("exceptions.json"));

    WordDictionary dictionary;
    if (!dictionary.loadFromFile(findConfig("words_bangla.json"))) {
        std::cerr << "could not load word list\n";
        return 1;
    }

    // Build the reverse map from the live rule table.
    LazyTypist typist;
    const SymbolTable& symbols = reference.getSymbolTable();
    for (const std::string& token : symbols.getTokensSortedByLengthDesc()) {
        const TokenRule* rule = symbols.lookupRule(token);
        if (!rule) {
            continue;
        }
        // Every output the token can produce -- default candidates and every contextual
        // variant -- is reachable from that token, which is what makes ষ come out as the
        // shared `sh` rather than the unambiguous `Sh`.
        for (const auto& candidate : rule->candidates) {
            const std::string canonical = BanglaText::normalize(candidate);
            typist.record(canonical, token);

            // The rule table stores only independent vowels; UnicodeComposer derives the
            // matra at compose time. The reverse direction has to derive it too, or every
            // word containing a matra -- which is nearly all of them -- looks unreachable.
            auto cps = UnicodeComposer::utf8ToCodepoints(canonical);
            if (cps.size() == 1 && UnicodeComposer::isBengaliIndependentVowel(cps[0])) {
                char32_t matra = UnicodeComposer::toDependentVowel(cps[0]);
                if (matra != 0 && matra != cps[0]) {
                    typist.record(UnicodeComposer::codepointToUtf8(matra), token);
                }
            }
        }
        for (const auto& variant : rule->variants) {
            for (const auto& candidate : variant.candidates) {
                typist.record(BanglaText::normalize(candidate), token);
            }
        }
    }

    // The sample is drawn from the middle of the frequency distribution rather than the
    // very top: the top few hundred words are pronouns and particles with little
    // ambiguity, and scoring on them would understate the problem.
    auto ranked = dictionary.topWords(offset + sampleSize);
    std::vector<std::string> sample;
    std::set<std::string> testSet;
    for (size_t i = offset; i < ranked.size(); ++i) {
        sample.push_back(ranked[i].word);
        testSet.insert(ranked[i].word);
    }

    // Held-out dictionary: every word EXCEPT the ones being tested.
    //
    // This is the difference between measuring whether the mechanism works and measuring
    // whether it generalises. Scoring the resolver on words that are in its own dictionary
    // asks it to recognise things it has memorised, which any lookup table passes. Removing
    // them asks the real question: can it spell a word it has never seen, using only the
    // statistics of the words it has?
    WordDictionary heldOut;
    for (const auto& hit : dictionary.topWords(0)) {
        if (!testSet.count(hit.word)) {
            heldOut.add(hit.word, hit.frequency);
        }
    }

    PhoneticEngine baseline;
    baseline.loadRules(findConfig("phonetic_rules.json"));
    baseline.loadExceptions(findConfig("exceptions.json"));

    PhoneticEngine smart;
    smart.loadRules(findConfig("phonetic_rules.json"));
    smart.loadExceptions(findConfig("exceptions.json"));
    {
        auto resolver = std::make_unique<DictionaryCandidateResolver>(&dictionary);
        resolver->setSearchLimits(maxTokens, maxCombos);
        smart.setCandidateResolver(std::move(resolver));
    }

    // Held-out, whole-word lookup only: the baseline the statistical tiers have to beat.
    PhoneticEngine generalising;
    generalising.loadRules(findConfig("phonetic_rules.json"));
    generalising.loadExceptions(findConfig("exceptions.json"));
    {
        auto resolver = std::make_unique<DictionaryCandidateResolver>(&heldOut);
        resolver->setMorphologyEnabled(false);
        resolver->setSearchLimits(maxTokens, maxCombos);
        generalising.setCandidateResolver(std::move(resolver));
    }

    // Held-out plus morphological stem lookup.
    PhoneticEngine withMorphology;
    withMorphology.loadRules(findConfig("phonetic_rules.json"));
    withMorphology.loadExceptions(findConfig("exceptions.json"));
    {
        auto resolver = std::make_unique<DictionaryCandidateResolver>(&heldOut);
        resolver->setMorphologyEnabled(true);
        resolver->setSearchLimits(maxTokens, maxCombos);
        withMorphology.setCandidateResolver(std::move(resolver));
    }

    // Held-out plus morphology plus the character model. The n-gram model is trained on
    // the held-out dictionary too, so it has never seen the test words either.
    NgramModel ngram;
    ngram.train(heldOut);

    PhoneticEngine withNgram;
    withNgram.loadRules(findConfig("phonetic_rules.json"));
    withNgram.loadExceptions(findConfig("exceptions.json"));
    {
        auto resolver = std::make_unique<DictionaryCandidateResolver>(&heldOut);
        resolver->setMorphologyEnabled(true);
        resolver->setNgramModel(&ngram);
        resolver->setNgramMargin(ngramMargin);
        resolver->setSearchLimits(maxTokens, maxCombos);
        withNgram.setCandidateResolver(std::move(resolver));
    }

    Score allDefault, allSmart, ambiguousDefault, ambiguousSmart;
    Score ambiguousHeldOut, ambiguousMorph, ambiguousNgram;
    size_t unreachable = 0;

    for (const auto& word : sample) {
        std::string roman;
        if (!typist.toRoman(word, roman) || roman.empty()) {
            ++unreachable;
            continue;
        }

        const bool ambiguous = typist.wordIsAmbiguous(word);
        const std::string byDefault = baseline.transliterate(roman);
        const std::string bySmart = smart.transliterate(roman);
        const std::string byHeldOut = generalising.transliterate(roman);
        const std::string byMorph = withMorphology.transliterate(roman);
        const std::string byNgram = withNgram.transliterate(roman);

        ++allDefault.total;
        ++allSmart.total;
        if (byDefault == word) ++allDefault.correct;
        if (bySmart == word)   ++allSmart.correct;

        if (ambiguous) {
            ++ambiguousDefault.total;
            ++ambiguousSmart.total;
            if (byDefault == word) ++ambiguousDefault.correct;
            if (bySmart == word)   ++ambiguousSmart.correct;
            ++ambiguousHeldOut.total;
            if (byHeldOut == word) ++ambiguousHeldOut.correct;
            ++ambiguousMorph.total;
            if (byMorph == word)   ++ambiguousMorph.correct;
            ++ambiguousNgram.total;
            if (byNgram == word)   ++ambiguousNgram.correct;
        }
    }

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "========================================================\n"
              << "  Candidate resolver accuracy\n"
              << "========================================================\n"
              << "Sample              : " << sample.size() << " words, ranked " << offset
              << "-" << (offset + sampleSize) << " by frequency\n"
              << "Unreachable         : " << unreachable
              << " (no Roman spelling; excluded from both scores)\n"
              << "Scored              : " << allDefault.total << "\n"
              << "  of which ambiguous: " << ambiguousDefault.total << "\n"
              << "--------------------------------------------------------\n"
              << "All scored words\n"
              << "  DefaultCandidateResolver    : " << allDefault.correct << "/"
              << allDefault.total << "  " << allDefault.percent() << "%\n"
              << "  DictionaryCandidateResolver : " << allSmart.correct << "/"
              << allSmart.total << "  " << allSmart.percent() << "%\n"
              << "--------------------------------------------------------\n"
              << "Homophone-bearing words (where the resolver has real work)\n"
              << "  DefaultCandidateResolver    : " << ambiguousDefault.correct << "/"
              << ambiguousDefault.total << "  " << ambiguousDefault.percent() << "%\n"
              << "  Dictionary (word in dict)   : " << ambiguousSmart.correct << "/"
              << ambiguousSmart.total << "  " << ambiguousSmart.percent() << "%\n"
              << "--------------------------------------------------------\n"
              << "HELD OUT -- test words removed from the resolver's dictionary\n"
              << "  whole-word lookup only      : " << ambiguousHeldOut.correct << "/"
              << ambiguousHeldOut.total << "  " << ambiguousHeldOut.percent() << "%\n"
              << "  + morphological stems       : " << ambiguousMorph.correct << "/"
              << ambiguousMorph.total << "  " << ambiguousMorph.percent() << "%\n"
              << "  + character trigram (margin " << ngramMargin << ") : " << ambiguousNgram.correct << "/"
              << ambiguousNgram.total << "  " << ambiguousNgram.percent() << "%\n"
              << "--------------------------------------------------------\n"
              << "  n-gram model trained on the held-out dictionary, so it has\n"
              << "  never seen the test words either.\n"
              << "  \"held out\" removes the test words from the resolver's own dictionary,\n"
              << "  so it must spell words it has never seen. That is the honest number.\n"
              << "========================================================\n";

    return 0;
}
