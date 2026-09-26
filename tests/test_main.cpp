#include "test_helpers.h"
#include "core/SymbolTable.h"
#include "core/Tokenizer.h"
#include "core/Candidate.h"
#include "core/CandidateResolver.h"
#include "core/UnicodeComposer.h"
#include "core/SpecialCharPicker.h"
#include "core/PhoneticEngine.h"
#include "core/TokenTrie.h"
#include "core/ContextAnalyzer.h"
#include "core/ExceptionDictionary.h"
#include "core/FixedLayoutEngine.h"
#include "core/InputBuffer.h"
#include "core/SpecialCharPicker.h"

#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Locates a config file whether the tests run from the repo root or from bin/.
static std::string findConfig(const std::string& name) {
    const std::string candidates[] = {
        "config/" + name,
        "../config/" + name,
        "../../config/" + name,
        name
    };
    for (const auto& path : candidates) {
        std::ifstream probe(path);
        if (probe.is_open()) {
            return path;
        }
    }
    return "config/" + name;
}

/// Builds an expected UTF-8 string from an explicit list of Unicode codepoints.
static std::string cp(std::initializer_list<char32_t> codepoints) {
    std::string out;
    for (char32_t c : codepoints) {
        out += UnicodeComposer::codepointToUtf8(c);
    }
    return out;
}

/// Loads the shipped rules and exceptions, as the real application does.
static bool loadProductionConfig(PhoneticEngine& engine) {
    if (!engine.loadRules(findConfig("phonetic_rules.json"))) {
        return false;
    }
    engine.loadExceptions(findConfig("exceptions.json"));
    return true;
}

static bool test_symbol_table_lookup() {
    SymbolTable table;
    table.addRule("sh", {"শ", "ষ", "স"});
    table.addRule("kh", {"খ"});
    table.addRule("a", {"অ", "আ", ""});

    TEST_ASSERT(table.hasToken("sh"));
    TEST_ASSERT(table.hasToken("kh"));
    TEST_ASSERT(table.hasToken("a"));
    TEST_ASSERT(!table.hasToken("xyz"));

    const auto* shCandidates = table.lookup("sh");
    TEST_ASSERT(shCandidates != nullptr);
    TEST_ASSERT_EQ(shCandidates->size(), 3);
    TEST_ASSERT_EQ((*shCandidates)[0], "শ");
    TEST_ASSERT_EQ((*shCandidates)[1], "ষ");
    TEST_ASSERT_EQ((*shCandidates)[2], "স");

    const auto* missing = table.lookup("nonexistent");
    TEST_ASSERT(missing == nullptr);

    return true;
}

static bool test_longest_match_tokenization() {
    SymbolTable table;
    table.addRule("s", {"স"});
    table.addRule("h", {"হ"});
    table.addRule("sh", {"শ", "ষ", "স"});
    table.addRule("a", {"অ", "আ", ""});
    table.addRule("n", {"ন", "ণ"});
    table.addRule("t", {"ত", "ট"});
    table.addRule("i", {"ই", "ঈ"});

    Tokenizer tokenizer(table);

    // "shanti" must be tokenized as ["sh", "a", "n", "t", "i"] rather than ["s", "h", "a", "n", "t", "i"]
    auto tokens = tokenizer.tokenize("shanti");
    TEST_ASSERT_EQ(tokens.size(), 5);
    TEST_ASSERT_EQ(tokens[0], "sh");
    TEST_ASSERT_EQ(tokens[1], "a");
    TEST_ASSERT_EQ(tokens[2], "n");
    TEST_ASSERT_EQ(tokens[3], "t");
    TEST_ASSERT_EQ(tokens[4], "i");

    // Single multi-letter vs single letter prefix
    table.addRule("chh", {"ছ"});
    table.addRule("ch", {"চ"});
    table.addRule("c", {"চ"});
    auto tokens2 = tokenizer.tokenize("chhoti");
    TEST_ASSERT_EQ(tokens2.size(), 4);
    TEST_ASSERT_EQ(tokens2[0], "chh");

    return true;
}

static bool test_candidate_selection_and_cycling() {
    Candidate c("sh", {"শ", "ষ", "স"}, 0);

    // Initial default candidate index 0
    TEST_ASSERT_EQ(c.selected(), "শ");

    // Cycle to index 1
    c.cycleNext();
    TEST_ASSERT_EQ(c.selected(), "ষ");

    // Cycle to index 2
    c.cycleNext();
    TEST_ASSERT_EQ(c.selected(), "স");

    // Wrap around back to index 0
    c.cycleNext();
    TEST_ASSERT_EQ(c.selected(), "শ");

    // DefaultCandidateResolver always selects 0
    DefaultCandidateResolver resolver;
    std::vector<Candidate> all = { c };
    TEST_ASSERT_EQ(resolver.resolve(c, 0, all), 0);

    return true;
}

static bool test_unicode_composition_simple() {
    UnicodeComposer composer;

    // Consonant 'k' (ক) followed by vowel 'i' (ই) -> কি
    std::vector<std::string> parts1 = {"ক", "ই"};
    std::string res1 = composer.composeStrings(parts1);
    // Expected: ক (U+0995) + ি (U+09BF)
    std::string expected1 = UnicodeComposer::codepointToUtf8(0x0995) + UnicodeComposer::codepointToUtf8(0x09BF);
    TEST_ASSERT_EQ(res1, expected1);

    // Consonant 'm' (ম) followed by vowel 'aa' (আ) -> মা
    std::vector<std::string> parts2 = {"ম", "আ"};
    std::string res2 = composer.composeStrings(parts2);
    std::string expected2 = UnicodeComposer::codepointToUtf8(0x09AE) + UnicodeComposer::codepointToUtf8(0x09BE);
    TEST_ASSERT_EQ(res2, expected2);

    return true;
}

static bool test_virama_conjunct_sequence() {
    UnicodeComposer composer;

    // Consonant 'k' (ক) followed directly by consonant 'sh' (ষ)
    // Must form conjunct with virama (্): ক + ্ + ষ (ক্ষ)
    std::vector<std::string> parts = {"ক", "ষ"};
    std::string res = composer.composeStrings(parts);

    std::string expectedViramaSeq = UnicodeComposer::codepointToUtf8(0x0995) +
                                   UnicodeComposer::codepointToUtf8(0x09CD) + // Virama
                                   UnicodeComposer::codepointToUtf8(0x09B7);

    TEST_ASSERT_EQ(res, expectedViramaSeq);

    // Three consecutive consonants: s + t + r -> স + ্ + ত + ্ + র
    std::vector<std::string> parts3 = {"স", "ত", "র"};
    std::string res3 = composer.composeStrings(parts3);
    std::string expected3 = UnicodeComposer::codepointToUtf8(0x09B8) +
                            UnicodeComposer::codepointToUtf8(0x09CD) +
                            UnicodeComposer::codepointToUtf8(0x09A4) +
                            UnicodeComposer::codepointToUtf8(0x09CD) +
                            UnicodeComposer::codepointToUtf8(0x09B0);
    TEST_ASSERT_EQ(res3, expected3);

    return true;
}

static bool test_epsilon_candidate() {
    UnicodeComposer composer;

    // Epsilon represented by empty string: {"ক", "", "ল"}
    std::vector<std::string> parts = {"ক", "", "ল"};
    std::string res = composer.composeStrings(parts);

    // Since "" has no characters, it does not output anything
    TEST_ASSERT(!res.empty());

    return true;
}

static bool test_special_char_picker() {
    SpecialCharPicker picker;
    TEST_ASSERT(!picker.isActive());

    picker.activate();
    TEST_ASSERT(picker.isActive());

    // Key '1' -> ৎ (Khanda Ta)
    auto c1 = picker.handleKey('1');
    TEST_ASSERT(c1.has_value());
    TEST_ASSERT_EQ(c1.value(), "ৎ");
    TEST_ASSERT(!picker.isActive()); // Must deactivate after handling

    // Key '2' -> ং (Anusvara)
    picker.activate();
    auto c2 = picker.handleKey('2');
    TEST_ASSERT(c2.has_value());
    TEST_ASSERT_EQ(c2.value(), "ং");

    // Key '3' -> ঃ (Visarga)
    picker.activate();
    auto c3 = picker.handleKey('3');
    TEST_ASSERT(c3.has_value());
    TEST_ASSERT_EQ(c3.value(), "ঃ");

    // Key '4' -> ঁ (Chandrabindu)
    picker.activate();
    auto c4 = picker.handleKey('4');
    TEST_ASSERT(c4.has_value());
    TEST_ASSERT_EQ(c4.value(), "ঁ");

    // Key '5' -> ঞ (Nya)
    picker.activate();
    auto c5 = picker.handleKey('5');
    TEST_ASSERT(c5.has_value());
    TEST_ASSERT_EQ(c5.value(), "ঞ");

    // Invalid key (e.g. '9' or 'X') should cancel and return nullopt
    picker.activate();
    auto cInvalid = picker.handleKey('X');
    TEST_ASSERT(!cInvalid.has_value());
    TEST_ASSERT(!picker.isActive());

    return true;
}

static bool test_end_to_end_transliteration() {
    PhoneticEngine engine;
    engine.loadRulesFromString(R"({
        "sh": ["শ"],
        "aa": ["আ"],
        "n": ["ন"],
        "t": ["ত"],
        "i": ["ই"],
        "k": ["ক"],
        "l": ["ল"],
        "a": ["অ"]
    })");

    // "shaanti" -> শ + া + ন + ্ + ত + ি (শান্তি)
    std::string res = engine.transliterate("shaanti");
    std::string expectedShanti = UnicodeComposer::codepointToUtf8(0x09B6) + // শ
                                 UnicodeComposer::codepointToUtf8(0x09BE) + // া
                                 UnicodeComposer::codepointToUtf8(0x09A8) + // ন
                                 UnicodeComposer::codepointToUtf8(0x09CD) + // ্
                                 UnicodeComposer::codepointToUtf8(0x09A4) + // ত
                                 UnicodeComposer::codepointToUtf8(0x09BF);  // ি
    TEST_ASSERT_EQ(res, expectedShanti);

    // "kal": "k" + "a" ("অ" inherent vowel) + "l" ("ল") -> "কল" (no virama between k and l)
    std::string resKal = engine.transliterate("kal");
    std::string expectedKal = UnicodeComposer::codepointToUtf8(0x0995) +
                              UnicodeComposer::codepointToUtf8(0x09B2);
    TEST_ASSERT_EQ(resKal, expectedKal);

    return true;
}

static bool test_retaining_cycled_candidate_across_typing() {
    PhoneticEngine engine;
    engine.loadRulesFromString(R"({
        "a": ["অ", "আ", ""],
        "l": ["ল"],
        "u": ["উ", "ঊ"]
    })");

    // 1. User types 'a' -> default candidate is "অ"
    engine.updateActiveBuffer("a");
    TEST_ASSERT_EQ(engine.getActiveComposedString(), "অ");

    // 2. User cycles candidate: 'a' -> 'আ'
    bool cycled = engine.cycleActiveCandidate();
    TEST_ASSERT(cycled);
    TEST_ASSERT_EQ(engine.getActiveComposedString(), "আ");

    // 3. User continues typing 'l' -> buffer is "al"
    // The engine MUST retain 'আ' from previous candidate selection rather than resetting to 'অ'!
    engine.updateActiveBuffer("al");
    TEST_ASSERT_EQ(engine.getActiveComposedString(), "আল");

    // 4. User types 'u' -> buffer is "alu"
    // Candidate selections must be preserved: 'আ' + 'ল' + 'ু' = "আলু"
    engine.updateActiveBuffer("alu");
    TEST_ASSERT_EQ(engine.getActiveComposedString(), "আলু");

    // 5. Flushing the active buffer on space/enter must retain "আলু"
    std::string flushed = engine.flushActive();
    TEST_ASSERT_EQ(flushed, "আলু");
    TEST_ASSERT_EQ(engine.getActiveComposedString(), ""); // active state cleared

    // 6. Test full-word candidate rotation on "alu" starting from default "অলু"
    engine.updateActiveBuffer("alu");
    TEST_ASSERT_EQ(engine.getActiveComposedString(), "অলু");

    // Cycle 1: cycles 'u' from 'উ' to 'ঊ' -> "অলূ"
    engine.cycleActiveCandidate();
    TEST_ASSERT_EQ(engine.getActiveComposedString(), "অলূ");

    // Cycle 2: 'u' wraps, carries over to cycle 'a' from 'অ' to 'আ' -> "আলু"
    engine.cycleActiveCandidate();
    TEST_ASSERT_EQ(engine.getActiveComposedString(), "আলু");

    return true;
}

// ---------------------------------------------------------------------------
// Tokenization: trie
// ---------------------------------------------------------------------------

static bool test_token_trie_longest_match() {
    TokenTrie trie;
    trie.insert("c");
    trie.insert("ch");
    trie.insert("chh");
    trie.insert("a");

    TEST_ASSERT(trie.contains("chh"));
    TEST_ASSERT(!trie.contains("chhh"));
    TEST_ASSERT_EQ(trie.size(), 4);

    // Must prefer the longest registered prefix, not the first one found.
    TEST_ASSERT_EQ(trie.longestMatchLength("chhoti", 0), 3);
    TEST_ASSERT_EQ(trie.longestMatchLength("chati", 0), 2);
    TEST_ASSERT_EQ(trie.longestMatchLength("cati", 0), 1);

    // No match at all.
    TEST_ASSERT_EQ(trie.longestMatchLength("xyz", 0), 0);

    // Matching from an offset.
    TEST_ASSERT_EQ(trie.longestMatchLength("xchh", 1), 3);

    // Re-inserting must not double-count.
    trie.insert("ch");
    TEST_ASSERT_EQ(trie.size(), 4);

    return true;
}

static bool test_trie_matches_bruteforce_scan() {
    // The trie replaced a "try every substring length" scan. Both must agree exactly,
    // otherwise the optimisation silently changed tokenization behaviour.
    SymbolTable table;
    table.addRule("a", {"আ"});
    table.addRule("b", {"ব"});
    table.addRule("bh", {"ভ"});
    table.addRule("s", {"স"});
    table.addRule("sh", {"শ"});
    table.addRule("t", {"ত"});
    table.addRule("th", {"থ"});
    table.addRule("i", {"ই"});
    table.addRule("n", {"ন"});

    Tokenizer tokenizer(table);
    const std::vector<std::string> inputs = {
        "shanti", "bhat", "abhisthan", "bbb", "sth", "zzz", "ashathi", ""
    };

    for (const auto& input : inputs) {
        // Reference implementation: the original longest-match-first substring scan.
        std::vector<std::string> reference;
        size_t pos = 0;
        while (pos < input.size()) {
            bool matched = false;
            size_t checkLen = std::min(table.getMaxTokenLength(), input.size() - pos);
            for (size_t len = checkLen; len >= 1; --len) {
                std::string sub = input.substr(pos, len);
                if (table.hasToken(sub)) {
                    reference.push_back(sub);
                    pos += len;
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                reference.push_back(input.substr(pos, 1));
                pos += 1;
            }
        }

        std::vector<std::string> actual = tokenizer.tokenize(input);
        TEST_ASSERT_EQ(actual.size(), reference.size());
        for (size_t i = 0; i < actual.size(); ++i) {
            TEST_ASSERT_EQ(actual[i], reference[i]);
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Context analysis
// ---------------------------------------------------------------------------

static bool test_context_analyzer_flags() {
    SymbolTable table;
    table.addRule("b", {"ব"});
    table.addRule("a", {"আ"});
    table.addRule("l", {"ল"});

    ContextAnalyzer analyzer(table);
    std::vector<std::string> tokens = {"b", "a", "l"};
    std::vector<uint32_t> contexts = analyzer.analyze(tokens);

    TEST_ASSERT_EQ(contexts.size(), 3);

    // First token: word start, followed by a vowel.
    TEST_ASSERT((contexts[0] & Ctx::WORD_START) != 0);
    TEST_ASSERT((contexts[0] & Ctx::BEFORE_VOWEL) != 0);
    TEST_ASSERT((contexts[0] & Ctx::WORD_END) == 0);
    TEST_ASSERT((contexts[0] & Ctx::AFTER_CONSONANT) == 0);

    // Middle token: between two consonants, neither start nor end.
    TEST_ASSERT((contexts[1] & Ctx::AFTER_CONSONANT) != 0);
    TEST_ASSERT((contexts[1] & Ctx::BEFORE_CONSONANT) != 0);
    TEST_ASSERT((contexts[1] & Ctx::WORD_START) == 0);
    TEST_ASSERT((contexts[1] & Ctx::WORD_END) == 0);

    // Last token: word end, after a vowel.
    TEST_ASSERT((contexts[2] & Ctx::WORD_END) != 0);
    TEST_ASSERT((contexts[2] & Ctx::AFTER_VOWEL) != 0);

    // A single token is both the start and the end of its word.
    std::vector<uint32_t> single = analyzer.analyze({"b"});
    TEST_ASSERT((single[0] & Ctx::WORD_START) != 0);
    TEST_ASSERT((single[0] & Ctx::WORD_END) != 0);

    return true;
}

static bool test_contextual_rule_selection() {
    // "ng" is ঙ between vowels but ং before a consonant or at the end of a word.
    // Getting this wrong is why the original prototype turned "bangla" into বঙ্ল.
    SymbolTable table;
    TEST_ASSERT(table.loadFromString(R"({
        "a": { "class": "vowel", "candidates": ["আ"] },
        "l": { "class": "consonant", "candidates": ["ল"] },
        "i": { "class": "vowel", "candidates": ["ই"] },
        "ng": {
            "class": "consonant",
            "candidates": ["ঙ", "ং"],
            "context": [
                { "when": ["before_consonant"], "candidates": ["ং", "ঙ"] },
                { "when": ["word_end"],         "candidates": ["ং", "ঙ"] }
            ]
        }
    })"));

    TEST_ASSERT_EQ(table.contextualRuleCount(), 1);

    const auto* beforeConsonant = table.lookupContextual("ng", Ctx::BEFORE_CONSONANT);
    TEST_ASSERT(beforeConsonant != nullptr);
    TEST_ASSERT_EQ((*beforeConsonant)[0], "ং");

    const auto* atWordEnd = table.lookupContextual("ng", Ctx::WORD_END | Ctx::AFTER_VOWEL);
    TEST_ASSERT(atWordEnd != nullptr);
    TEST_ASSERT_EQ((*atWordEnd)[0], "ং");

    const auto* beforeVowel = table.lookupContextual("ng", Ctx::BEFORE_VOWEL | Ctx::AFTER_VOWEL);
    TEST_ASSERT(beforeVowel != nullptr);
    TEST_ASSERT_EQ((*beforeVowel)[0], "ঙ");

    // Default candidates are still reachable through the non-contextual lookup.
    const auto* plain = table.lookup("ng");
    TEST_ASSERT(plain != nullptr);
    TEST_ASSERT_EQ((*plain)[0], "ঙ");

    return true;
}

static bool test_legacy_rule_format_still_loads() {
    // Rule files written for the original prototype must keep working unchanged.
    SymbolTable table;
    TEST_ASSERT(table.loadFromString(R"({
        "sh": ["শ", "ষ", "স"],
        "kh": "খ",
        "a": ["আ", "অ", ""]
    })"));

    TEST_ASSERT_EQ(table.size(), 3);
    TEST_ASSERT_EQ(table.contextualRuleCount(), 0);

    const auto* sh = table.lookup("sh");
    TEST_ASSERT(sh != nullptr);
    TEST_ASSERT_EQ(sh->size(), 3);

    const auto* kh = table.lookup("kh");
    TEST_ASSERT(kh != nullptr);
    TEST_ASSERT_EQ(kh->size(), 1);
    TEST_ASSERT_EQ((*kh)[0], "খ");

    // Classes are inferred from the candidates when the JSON does not declare them.
    TEST_ASSERT(table.classOf("sh") == TokenClass::CONSONANT);
    TEST_ASSERT(table.classOf("a") == TokenClass::VOWEL);

    return true;
}

// ---------------------------------------------------------------------------
// Exception dictionary
// ---------------------------------------------------------------------------

static bool test_exception_dictionary() {
    ExceptionDictionary dict;
    TEST_ASSERT(dict.loadFromString(R"({
        "words": {
            "dhonnobad": "ধন্যবাদ",
            "download": "download"
        }
    })"));

    TEST_ASSERT_EQ(dict.size(), 2);

    std::string out;
    TEST_ASSERT(dict.lookup("dhonnobad", out));
    TEST_ASSERT_EQ(out, "ধন্যবাদ");

    // Case-insensitive fallback.
    TEST_ASSERT(dict.lookup("Dhonnobad", out));
    TEST_ASSERT_EQ(out, "ধন্যবাদ");

    // English words map to themselves and so survive transliteration.
    TEST_ASSERT(dict.lookup("download", out));
    TEST_ASSERT_EQ(out, "download");

    TEST_ASSERT(!dict.lookup("bangla", out));

    return true;
}

static bool test_exception_overrides_rules() {
    PhoneticEngine engine;
    TEST_ASSERT(engine.loadRulesFromString(R"({
        "d": { "class": "consonant", "candidates": ["দ"] },
        "o": { "class": "vowel", "candidates": ["অ"] }
    })"));
    TEST_ASSERT(engine.loadExceptionsFromString(R"({ "words": { "do": "ডু" } })"));

    // The override must win over the rule pipeline.
    TEST_ASSERT_EQ(engine.transliterate("do"), std::string("ডু"));

    // A word with no override still goes through the rules.
    TEST_ASSERT_EQ(engine.transliterate("d"), std::string("দ"));

    return true;
}

// ---------------------------------------------------------------------------
// End-to-end behaviour against the shipped rule set
// ---------------------------------------------------------------------------

static bool test_shipped_rules_load() {
    PhoneticEngine engine;
    TEST_ASSERT(loadProductionConfig(engine));
    TEST_ASSERT(engine.getSymbolTable().size() > 50);
    TEST_ASSERT(engine.getSymbolTable().contextualRuleCount() > 0);
    TEST_ASSERT(engine.getExceptions().size() > 0);
    return true;
}

static bool test_readme_examples() {
    // Every one of these was wrong before contextual resolution existed:
    //   shanti -> শন্তি, amar -> অমর, bangla -> বঙ্ল, gai -> গৈ
    PhoneticEngine engine;
    TEST_ASSERT(loadProductionConfig(engine));

    TEST_ASSERT_EQ(engine.transliterate("shanti"), std::string("শান্তি"));
    TEST_ASSERT_EQ(engine.transliterate("amar"),   std::string("আমার"));
    TEST_ASSERT_EQ(engine.transliterate("bangla"), std::string("বাংলা"));
    TEST_ASSERT_EQ(engine.transliterate("kol"),    std::string("কল"));
    TEST_ASSERT_EQ(engine.transliterate("ami"),    std::string("আমি"));
    TEST_ASSERT_EQ(engine.transliterate("gan"),    std::string("গান"));
    TEST_ASSERT_EQ(engine.transliterate("gai"),    std::string("গাই"));

    // The Avro tagline, end to end, through the sentence-level entry point.
    TEST_ASSERT_EQ(engine.transliterateText("ami banglay gan gai"),
                   std::string("আমি বাংলায় গান গাই"));

    return true;
}

static bool test_inherent_vowel_handling() {
    PhoneticEngine engine;
    TEST_ASSERT(loadProductionConfig(engine));

    // Lowercase 'o' is the inherent vowel: it emits nothing but does break the conjunct.
    TEST_ASSERT_EQ(engine.transliterate("rong"),  std::string("রং"));
    TEST_ASSERT_EQ(engine.transliterate("uttor"), std::string("উত্তর"));
    TEST_ASSERT_EQ(engine.transliterate("boi"),   std::string("বই"));

    // Without the inherent vowel the consonants stack into a conjunct instead.
    TEST_ASSERT_EQ(engine.transliterate("kl"), std::string("ক্ল"));

    // Capital O forces the explicit ও / ো form.
    TEST_ASSERT_EQ(engine.transliterate("sOnar"), std::string("সোনার"));

    return true;
}

static bool test_case_sensitive_retroflex_tokens() {
    PhoneticEngine engine;
    TEST_ASSERT(loadProductionConfig(engine));

    // Capitals select the retroflex series, as in Avro.
    TEST_ASSERT_EQ(engine.transliterate("T"),  std::string("ট"));
    TEST_ASSERT_EQ(engine.transliterate("t"),  std::string("ত"));
    TEST_ASSERT_EQ(engine.transliterate("D"),  std::string("ড"));
    TEST_ASSERT_EQ(engine.transliterate("d"),  std::string("দ"));
    TEST_ASSERT_EQ(engine.transliterate("N"),  std::string("ণ"));
    TEST_ASSERT_EQ(engine.transliterate("R"),  std::string("ড়"));
    TEST_ASSERT_EQ(engine.transliterate("Sh"), std::string("ষ"));

    return true;
}

static bool test_unicode_conformance_sequences() {
    // Assert exact codepoint sequences, not just visually equal strings. Bengali has
    // several ways to look right and be wrong: a standalone ো instead of a real matra,
    // a missing virama, or an independent vowel where a kar belongs.
    PhoneticEngine engine;
    TEST_ASSERT(loadProductionConfig(engine));

    // বাংলা = ব U+09AC, া U+09BE, ং U+0982, ল U+09B2, া U+09BE
    TEST_ASSERT_EQ(engine.transliterate("bangla"),
                   cp({0x09AC, 0x09BE, 0x0982, 0x09B2, 0x09BE}));

    // শান্তি = শ U+09B6, া U+09BE, ন U+09A8, ্ U+09CD, ত U+09A4, ি U+09BF
    TEST_ASSERT_EQ(engine.transliterate("shanti"),
                   cp({0x09B6, 0x09BE, 0x09A8, 0x09CD, 0x09A4, 0x09BF}));

    // বই must be two independent letters, NOT ব + ি (বি).
    TEST_ASSERT_EQ(engine.transliterate("boi"), cp({0x09AC, 0x0987}));

    // ক্ষমা exercises a pre-composed conjunct token followed by the inherent vowel.
    TEST_ASSERT_EQ(engine.transliterate("kkhoma"),
                   cp({0x0995, 0x09CD, 0x09B7, 0x09AE, 0x09BE}));

    // য-fola: "bidya" = ব ি দ ্ য া
    TEST_ASSERT_EQ(engine.transliterate("bidya"),
                   cp({0x09AC, 0x09BF, 0x09A6, 0x09CD, 0x09AF, 0x09BE}));

    return true;
}

static bool test_candidate_cycling_end_to_end() {
    PhoneticEngine engine;
    TEST_ASSERT(loadProductionConfig(engine));

    engine.updateActiveBuffer("sh");
    std::string first = engine.getActiveComposedString();
    TEST_ASSERT_EQ(first, std::string("শ"));

    TEST_ASSERT(engine.cycleActiveCandidate());
    TEST_ASSERT_EQ(engine.getActiveComposedString(), std::string("ষ"));

    TEST_ASSERT(engine.cycleActiveCandidate());
    TEST_ASSERT_EQ(engine.getActiveComposedString(), std::string("স"));

    // Wrap-around.
    TEST_ASSERT(engine.cycleActiveCandidate());
    TEST_ASSERT_EQ(engine.getActiveComposedString(), first);

    engine.clearActive();
    TEST_ASSERT(engine.getActiveCandidates().empty());

    return true;
}

static bool test_punctuation_and_unknown_passthrough() {
    PhoneticEngine engine;
    TEST_ASSERT(loadProductionConfig(engine));

    // Characters with no rule must survive untouched rather than being dropped.
    TEST_ASSERT_EQ(engine.transliterateText("ami, tumi!"), std::string("আমি, তুমি!"));
    TEST_ASSERT_EQ(engine.transliterateText("123"), std::string("123"));

    // An English word in the exception dictionary passes through inside a sentence.
    TEST_ASSERT_EQ(engine.transliterateText("tumi download koro"),
                   std::string("তুমি download কর"));

    // Note the convention this depends on: adjacent consonants conjoin, so "korbo"
    // is কর্ব and করব must be typed "korobo". Documented in config/phonetic_rules.json.
    TEST_ASSERT_EQ(engine.transliterate("korbo"),  std::string("কর্ব"));
    TEST_ASSERT_EQ(engine.transliterate("korobo"), std::string("করব"));

    return true;
}

static bool test_direct_candidate_selection() {
    // The candidate window lets the user click an option instead of cycling to it, so the
    // engine has to expose which token is being decided and allow setting it directly.
    PhoneticEngine engine;
    TEST_ASSERT(loadProductionConfig(engine));

    engine.updateActiveBuffer("sh");
    TEST_ASSERT_EQ(engine.activeAmbiguousTokenIndex(), 0);

    std::vector<std::string> options = engine.activeCandidateOptions();
    TEST_ASSERT_EQ(options.size(), 3);
    TEST_ASSERT_EQ(options[0], "শ");
    TEST_ASSERT_EQ(engine.activeCandidateSelection(), 0);

    // Jump straight to the third option rather than cycling twice.
    TEST_ASSERT(engine.setActiveSelection(0, 2));
    TEST_ASSERT_EQ(engine.getActiveComposedString(), std::string("স"));
    TEST_ASSERT_EQ(engine.activeCandidateSelection(), 2);

    // Out-of-range indices must be rejected without changing anything.
    TEST_ASSERT(!engine.setActiveSelection(0, 99));
    TEST_ASSERT(!engine.setActiveSelection(99, 0));
    TEST_ASSERT_EQ(engine.getActiveComposedString(), std::string("স"));

    // A buffer with nothing ambiguous reports no active token.
    engine.updateActiveBuffer("k");
    TEST_ASSERT_EQ(engine.activeAmbiguousTokenIndex(), -1);
    TEST_ASSERT(engine.activeCandidateOptions().empty());

    // The rightmost ambiguous token is the one being decided.
    engine.updateActiveBuffer("shot");
    const int index = engine.activeAmbiguousTokenIndex();
    TEST_ASSERT(index > 0);

    return true;
}


// ---------------------------------------------------------------------------
// UTF-8 correctness and trie completions
// ---------------------------------------------------------------------------

static bool test_trie_completions() {
    TokenTrie trie;
    trie.insert("k");
    trie.insert("kh");
    trie.insert("kkh");
    trie.insert("g");
    trie.insert("gh");

    // Completions include the prefix itself when it is a registered token; the UI filters
    // that out, because echoing back what the user already typed says nothing.
    std::vector<std::string> fromK = trie.getCompletions("k");
    TEST_ASSERT_EQ(fromK.size(), 3);

    bool hasK = false, hasKh = false, hasKkh = false;
    for (const auto& token : fromK) {
        if (token == "k")   hasK = true;
        if (token == "kh")  hasKh = true;
        if (token == "kkh") hasKkh = true;
    }
    TEST_ASSERT(hasK && hasKh && hasKkh);

    // limit caps the result set.
    TEST_ASSERT_EQ(trie.getCompletions("k", 2).size(), 2);

    // limit 0 means unlimited, not zero results.
    TEST_ASSERT_EQ(trie.getCompletions("k", 0).size(), 3);

    // A prefix that is not in the trie yields nothing rather than misbehaving.
    TEST_ASSERT(trie.getCompletions("z").empty());
    TEST_ASSERT(trie.getCompletions("kx").empty());

    // A prefix that exists only as an interior path still completes.
    TEST_ASSERT_EQ(trie.getCompletions("gh").size(), 1);

    // Non-ASCII input must not walk the fixed-size child array out of range.
    TEST_ASSERT(trie.getCompletions("\xE0\xA6\x95").empty());

    return true;
}

static bool test_trie_rejects_non_ascii_tokens() {
    TokenTrie trie;
    trie.insert("k");
    // A Bengali string is not a Roman token; inserting one must be a no-op rather than
    // building a branch that can never be matched.
    trie.insert("\xE0\xA6\x95");
    TEST_ASSERT_EQ(trie.size(), 1);
    TEST_ASSERT(!trie.contains("\xE0\xA6\x95"));
    TEST_ASSERT(trie.contains("k"));
    return true;
}

static bool test_input_buffer_utf8_backspace() {
    InputBuffer buffer;

    // ASCII: one byte, one backspace.
    buffer.append("kal");
    TEST_ASSERT(buffer.backspace());
    TEST_ASSERT_EQ(buffer.content(), std::string("ka"));

    // A three-byte Bengali character must be removed whole, not one byte at a time.
    // Leaving a truncated sequence behind would make every later lookup operate on
    // invalid UTF-8.
    buffer.clear();
    buffer.append("\xE0\xA6\x95");  // ক
    TEST_ASSERT_EQ(buffer.content().size(), 3);
    TEST_ASSERT(buffer.backspace());
    TEST_ASSERT(buffer.empty());

    // Mixed content: remove only the trailing multi-byte character.
    buffer.clear();
    buffer.append("a\xE0\xA6\x95");
    TEST_ASSERT(buffer.backspace());
    TEST_ASSERT_EQ(buffer.content(), std::string("a"));

    // Backspacing an empty buffer reports that there was nothing to remove.
    buffer.clear();
    TEST_ASSERT(!buffer.backspace());

    return true;
}

static bool test_tokenizer_preserves_utf8_characters() {
    // Characters with no rule pass through. Before the UTF-8 fix a three-byte character
    // was split into three separate one-byte tokens, so the output was mojibake.
    SymbolTable table;
    table.addRule("a", {"আ"});

    Tokenizer tokenizer(table);

    std::vector<std::string> tokens = tokenizer.tokenize("a\xE0\xA6\x95");
    TEST_ASSERT_EQ(tokens.size(), 2);
    TEST_ASSERT_EQ(tokens[0], std::string("a"));
    TEST_ASSERT_EQ(tokens[1], std::string("\xE0\xA6\x95"));

    // A two-byte sequence.
    tokens = tokenizer.tokenize("\xC3\xA9");
    TEST_ASSERT_EQ(tokens.size(), 1);
    TEST_ASSERT_EQ(tokens[0], std::string("\xC3\xA9"));

    // A truncated sequence at the end of input must not read past the buffer.
    tokens = tokenizer.tokenize("a\xE0\xA6");
    TEST_ASSERT_EQ(tokens.size(), 2);
    TEST_ASSERT_EQ(tokens[1], std::string("\xE0\xA6"));

    return true;
}

static bool test_special_picker_survives_modifier_release() {
    // The picker is opened with Ctrl+Shift+D. Releasing Ctrl or Shift afterwards used to
    // count as "any other key" and cancel it, so the menu closed before a digit could be
    // pressed - the shortcut cancelled itself.
    SpecialCharPicker picker;
    picker.activate();
    TEST_ASSERT(picker.isActive());

    TEST_ASSERT(!picker.handleKey(0x10).has_value()); // VK_SHIFT
    TEST_ASSERT(picker.isActive());
    TEST_ASSERT(!picker.handleKey(0x11).has_value()); // VK_CONTROL
    TEST_ASSERT(picker.isActive());
    TEST_ASSERT(!picker.handleKey(0xA0).has_value()); // VK_LSHIFT
    TEST_ASSERT(picker.isActive());
    TEST_ASSERT(!picker.handleKey(0xA2).has_value()); // VK_LCONTROL
    TEST_ASSERT(picker.isActive());

    // The digit still works after all that.
    auto picked = picker.handleKey('1');
    TEST_ASSERT(picked.has_value());
    TEST_ASSERT_EQ(picked.value(), std::string("\u09CE")); // ৎ
    TEST_ASSERT(!picker.isActive());

    // A genuinely unrelated key still cancels.
    picker.activate();
    TEST_ASSERT(!picker.handleKey('X').has_value());
    TEST_ASSERT(!picker.isActive());

    return true;
}

static bool test_exception_override_reaches_live_preview() {
    // The overlay composes from the active candidate list, so an override that only
    // applied inside transliterate() would show the rule-based guess while typing and
    // silently change on commit.
    PhoneticEngine engine;
    TEST_ASSERT(loadProductionConfig(engine));

    engine.updateActiveBuffer("dhonnobad");
    TEST_ASSERT_EQ(engine.getActiveComposedString(), std::string("ধন্যবাদ"));
    TEST_ASSERT_EQ(engine.getActiveComposedString(), engine.transliterate("dhonnobad"));

    // English passthrough behaves the same way.
    engine.updateActiveBuffer("download");
    TEST_ASSERT_EQ(engine.getActiveComposedString(), std::string("download"));

    // A word with no override is still composed from the rules.
    engine.updateActiveBuffer("bangla");
    TEST_ASSERT_EQ(engine.getActiveComposedString(), std::string("বাংলা"));

    return true;
}

static bool test_exception_dictionary_case_collision() {
    // Two entries differing only in case: the later one must win in both maps, so exact
    // and case-insensitive lookup cannot disagree with each other.
    ExceptionDictionary dict;
    dict.add("ok", "ঠিক");
    dict.add("OK", "ওকে");

    std::string out;
    TEST_ASSERT(dict.lookup("ok", out));
    TEST_ASSERT_EQ(out, "ঠিক");        // exact match still wins

    TEST_ASSERT(dict.lookup("OK", out));
    TEST_ASSERT_EQ(out, "ওকে");        // exact match still wins

    TEST_ASSERT(dict.lookup("Ok", out)); // falls through to the lowercase map
    TEST_ASSERT_EQ(out, "ওকে");

    return true;
}

// ---------------------------------------------------------------------------
// Fixed layout engine
// ---------------------------------------------------------------------------

static bool test_fixed_layout_engine() {
    FixedLayoutEngine layout;
    TEST_ASSERT(layout.loadFromString(R"({
        "name": "test layout",
        "base":  { "k": "ক", "a": "া", "/": "্" },
        "shift": { "S": "ষ" },
        "altgr": { ".": "়" }
    })"));

    TEST_ASSERT_EQ(layout.layoutName(), std::string("test layout"));
    TEST_ASSERT(layout.isMapped('k'));
    TEST_ASSERT(!layout.isMapped('Q'));

    TEST_ASSERT_EQ(layout.mapKey('k'), std::string("ক"));

    // Shift shares the base map: the keyboard has already applied it, so 'S' is just
    // another key as far as the engine is concerned.
    TEST_ASSERT(layout.isMapped('S'));
    TEST_ASSERT_EQ(layout.mapKey('S'), std::string("ষ"));

    // AltGr is a genuinely separate level on the same physical key.
    TEST_ASSERT(!layout.isMapped('.', FixedLayoutEngine::Level::Base));
    TEST_ASSERT(layout.isMapped('.', FixedLayoutEngine::Level::AltGr));
    TEST_ASSERT_EQ(layout.mapKey('.', FixedLayoutEngine::Level::AltGr), std::string("়"));

    // Unmapped keys pass through unchanged, at every level.
    TEST_ASSERT_EQ(layout.mapKey('Q'), std::string("Q"));
    TEST_ASSERT_EQ(layout.mapKey('k', FixedLayoutEngine::Level::AltGr), std::string("k"));

    // The typist builds conjuncts explicitly with the hasant key: k / S -> ক্ষ
    TEST_ASSERT_EQ(layout.mapText("k/S"), cp({0x0995, 0x09CD, 0x09B7}));

    // Fixed layouts are stateless: the same key always gives the same glyph.
    TEST_ASSERT_EQ(layout.mapText("kaka"), layout.mapText("ka") + layout.mapText("ka"));

    return true;
}

static bool test_legacy_single_map_layout_still_loads() {
    // Layouts written for the two-level engine used one flat "map" object.
    FixedLayoutEngine layout;
    TEST_ASSERT(layout.loadFromString(R"({
        "name": "legacy",
        "map": { "k": "ক", "K": "খ" }
    })"));
    TEST_ASSERT_EQ(layout.mapKey('k'), std::string("ক"));
    TEST_ASSERT_EQ(layout.mapKey('K'), std::string("খ"));
    return true;
}

static bool test_shipped_layout_matches_probhat() {
    FixedLayoutEngine layout;
    TEST_ASSERT(layout.loadFromFile(findConfig("layout_probhat.json")));
    TEST_ASSERT_EQ(layout.layoutName(), std::string("Probhat"));

    // Spot-checks against the canonical X11 ben_probhat definition. These are exactly the
    // keys an earlier draft of this file got wrong, so they are the ones worth pinning.
    TEST_ASSERT_EQ(layout.mapKey('/'), cp({0x09CD}));  // hasant, NOT on 'z'
    TEST_ASSERT_EQ(layout.mapKey('z'), cp({0x09DF}));  // য়
    TEST_ASSERT_EQ(layout.mapKey('x'), cp({0x09B6}));  // শ
    TEST_ASSERT_EQ(layout.mapKey('Z'), cp({0x09AF}));  // য
    TEST_ASSERT_EQ(layout.mapKey('S'), cp({0x09B7}));  // ষ
    TEST_ASSERT_EQ(layout.mapKey('H'), cp({0x0983}));  // ঃ
    TEST_ASSERT_EQ(layout.mapKey('>'), cp({0x0981}));  // ঁ
    TEST_ASSERT_EQ(layout.mapKey('E'), cp({0x0988}));  // ঈ
    TEST_ASSERT_EQ(layout.mapKey('Y'), cp({0x0990}));  // ঐ
    TEST_ASSERT_EQ(layout.mapKey('&'), cp({0x099E}));  // ঞ
    TEST_ASSERT_EQ(layout.mapKey('*'), cp({0x09CE}));  // ৎ

    // Probhat specifies the precomposed codepoints, not consonant + nukta.
    TEST_ASSERT_EQ(layout.mapKey('R'), cp({0x09DC}));  // ড়
    TEST_ASSERT_EQ(layout.mapKey('X'), cp({0x09DD}));  // ঢ়

    // AltGr level.
    TEST_ASSERT_EQ(layout.mapKey('.', FixedLayoutEngine::Level::AltGr), cp({0x09BC})); // ়
    TEST_ASSERT_EQ(layout.mapKey(']', FixedLayoutEngine::Level::AltGr), cp({0x09D7})); // ৗ
    TEST_ASSERT_EQ(layout.mapKey('h', FixedLayoutEngine::Level::AltGr), cp({0x09BD})); // ঽ

    // Zero-width joiner and non-joiner, used to force or forbid a ligature.
    TEST_ASSERT_EQ(layout.mapKey('`'),  cp({0x200D}));
    TEST_ASSERT_EQ(layout.mapKey('\\'), cp({0x200C}));

    // Bengali digits.
    TEST_ASSERT_EQ(layout.mapText("2026"), std::string("২০২৬"));

    // Every Bengali letter, matra and sign must be reachable from some key at some level,
    // or the layout cannot type the language.
    // Spelled as explicit codepoints, not as source literals. ড় can be written either as
    // U+09DC or as U+09A1 U+09BC, and the two are different strings even though an editor
    // renders them identically -- which is precisely the distinction this file exists to
    // get right.
    const std::vector<std::string> required = {
        cp({0x0995}), cp({0x0996}), cp({0x0997}), cp({0x0998}), cp({0x0999}),
        cp({0x099A}), cp({0x099B}), cp({0x099C}), cp({0x099D}), cp({0x099E}),
        cp({0x099F}), cp({0x09A0}), cp({0x09A1}), cp({0x09A2}), cp({0x09A3}),
        cp({0x09A4}), cp({0x09A5}), cp({0x09A6}), cp({0x09A7}), cp({0x09A8}),
        cp({0x09AA}), cp({0x09AB}), cp({0x09AC}), cp({0x09AD}), cp({0x09AE}),
        cp({0x09AF}), cp({0x09B0}), cp({0x09B2}), cp({0x09B6}), cp({0x09B7}),
        cp({0x09B8}), cp({0x09B9}),
        cp({0x09DC}), cp({0x09DD}), cp({0x09DF}), cp({0x09CE}),
        cp({0x0985}), cp({0x0986}), cp({0x0987}), cp({0x0988}), cp({0x0989}),
        cp({0x098A}), cp({0x098B}), cp({0x098F}), cp({0x0990}), cp({0x0993}),
        cp({0x0994}),
        cp({0x09BE}), cp({0x09BF}), cp({0x09C0}), cp({0x09C1}), cp({0x09C2}),
        cp({0x09C3}), cp({0x09C7}), cp({0x09C8}), cp({0x09CB}), cp({0x09CC}),
        cp({0x09CD}), cp({0x0982}), cp({0x0983}), cp({0x0981}), cp({0x09BC})
    };

    for (const auto& glyph : required) {
        bool found = false;
        for (auto level : { FixedLayoutEngine::Level::Base, FixedLayoutEngine::Level::AltGr }) {
            for (const auto& [key, value] : layout.keyMap(level)) {
                (void)key;
                if (value == glyph) { found = true; break; }
            }
            if (found) break;
        }
        if (!found) {
            std::cerr << "  missing glyph in layout: " << glyph
                      << " (" << UnicodeComposer::utf8ToCodepoints(glyph).size()
                      << " codepoints)\n";
            return false;
        }
    }

    return true;
}

int main() {
#ifdef _WIN32
    // Enable UTF-8 console output for Bengali characters
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "========================================\n"
              << "   Running Shobdomala Unit Tests\n"
              << "========================================\n";

    RUN_TEST(test_symbol_table_lookup);
    RUN_TEST(test_longest_match_tokenization);
    RUN_TEST(test_candidate_selection_and_cycling);
    RUN_TEST(test_unicode_composition_simple);
    RUN_TEST(test_virama_conjunct_sequence);
    RUN_TEST(test_epsilon_candidate);
    RUN_TEST(test_special_char_picker);
    RUN_TEST(test_end_to_end_transliteration);
    RUN_TEST(test_retaining_cycled_candidate_across_typing);

    // --- Added with the contextual engine -----------------------------------
    RUN_TEST(test_token_trie_longest_match);
    RUN_TEST(test_trie_matches_bruteforce_scan);
    RUN_TEST(test_context_analyzer_flags);
    RUN_TEST(test_contextual_rule_selection);
    RUN_TEST(test_legacy_rule_format_still_loads);
    RUN_TEST(test_exception_dictionary);
    RUN_TEST(test_exception_overrides_rules);
    RUN_TEST(test_shipped_rules_load);
    RUN_TEST(test_readme_examples);
    RUN_TEST(test_inherent_vowel_handling);
    RUN_TEST(test_case_sensitive_retroflex_tokens);
    RUN_TEST(test_unicode_conformance_sequences);
    RUN_TEST(test_candidate_cycling_end_to_end);
    RUN_TEST(test_punctuation_and_unknown_passthrough);
    RUN_TEST(test_direct_candidate_selection);
    RUN_TEST(test_trie_completions);
    RUN_TEST(test_trie_rejects_non_ascii_tokens);
    RUN_TEST(test_input_buffer_utf8_backspace);
    RUN_TEST(test_tokenizer_preserves_utf8_characters);
    RUN_TEST(test_special_picker_survives_modifier_release);
    RUN_TEST(test_exception_override_reaches_live_preview);
    RUN_TEST(test_exception_dictionary_case_collision);
    RUN_TEST(test_fixed_layout_engine);
    RUN_TEST(test_legacy_single_map_layout_still_loads);
    RUN_TEST(test_shipped_layout_matches_probhat);

    std::cout << "\n----------------------------------------\n";
    std::cout << "Results: " << g_testsPassed << "/" << g_testsRun << " passed";
    if (g_testsFailed > 0) {
        std::cout << " (" << g_testsFailed << " FAILED)";
    }
    std::cout << "\n========================================\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
