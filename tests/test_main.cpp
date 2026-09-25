#include "test_helpers.h"
#include "core/SymbolTable.h"
#include "core/Tokenizer.h"
#include "core/Candidate.h"
#include "core/CandidateResolver.h"
#include "core/UnicodeComposer.h"
#include "core/SpecialCharPicker.h"
#include "core/PhoneticEngine.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

int main() {
    // Enable UTF-8 console output for Bengali characters
    SetConsoleOutputCP(CP_UTF8);

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

    std::cout << "\n----------------------------------------\n";
    std::cout << "Results: " << g_testsPassed << "/" << g_testsRun << " passed";
    if (g_testsFailed > 0) {
        std::cout << " (" << g_testsFailed << " FAILED)";
    }
    std::cout << "\n========================================\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
