#pragma once

#include "SymbolTable.h"
#include "Tokenizer.h"
#include "ContextAnalyzer.h"
#include "Candidate.h"
#include "CandidateResolver.h"
#include "ExceptionDictionary.h"
#include "UnicodeComposer.h"
#include <string>
#include <vector>
#include <memory>

/**
 * @brief Main engine orchestrating the transliteration pipeline.
 *
 * Full pipeline:
 *
 *   Roman input
 *     -> ExceptionDictionary  (whole-word override? then we are done)
 *     -> Tokenizer            (pass 1: longest-match-first segmentation, trie-backed)
 *     -> ContextAnalyzer      (pass 2: annotate each token with its surroundings)
 *     -> SymbolTable          (contextual candidate lookup)
 *     -> CandidateResolver    (pick an index within the chosen candidate list)
 *     -> UnicodeComposer      (viramas, matras, inherent vowel)
 *     -> Bengali Unicode
 *
 * Also maintains state for candidate cycling on the active in-progress buffer.
 */
class PhoneticEngine {
public:
    PhoneticEngine();
    explicit PhoneticEngine(std::unique_ptr<ICandidateResolver> resolver);

    /// Loads phonetic rules from a JSON file.
    bool loadRules(const std::string& jsonFilePath);

    /// Loads phonetic rules directly from a JSON string.
    bool loadRulesFromString(const std::string& jsonString);

    /// Loads whole-word exception overrides from a JSON file. Missing file is not an error.
    bool loadExceptions(const std::string& jsonFilePath);

    /// Loads whole-word exception overrides from a JSON string.
    bool loadExceptionsFromString(const std::string& jsonString);

    /**
     * @brief Transliterates a single Roman word into composed Bengali.
     *
     * Checks the exception dictionary first; falls back to the rule pipeline.
     */
    std::string transliterate(const std::string& romanInput);

    /**
     * @brief Transliterates a whole line, splitting it into words at unregistered characters.
     *
     * Needed because exception overrides are whole-word: running transliterate() on a full
     * sentence would never match "download" sitting between two spaces.
     */
    std::string transliterateText(const std::string& romanText);

    /**
     * @brief Generates Candidate objects for the input string without final composition.
     * Candidate options are already context-selected; selection indices are populated.
     */
    std::vector<Candidate> generateCandidates(const std::string& romanInput);

    /// Updates the active candidates for an ongoing input buffer.
    void updateActiveBuffer(const std::string& romanBuffer);

    /**
     * @brief Cycles the last ambiguous candidate in the active candidates list.
     * @return True if a candidate was successfully cycled.
     */
    bool cycleActiveCandidate();

    /// Returns the composed Bengali string from the current active candidates list.
    std::string getActiveComposedString() const;

    /**
     * @brief Index of the rightmost token in the active buffer that has alternatives.
     *
     * This is the token cycleActiveCandidate() acts on first, and therefore the one the
     * user is deciding about right now. The UI shows its options.
     *
     * @return The index, or -1 when nothing in the buffer is ambiguous.
     */
    int activeAmbiguousTokenIndex() const;

    /// The alternatives for activeAmbiguousTokenIndex(), or empty when there are none.
    std::vector<std::string> activeCandidateOptions() const;

    /// Which alternative is currently chosen for activeAmbiguousTokenIndex().
    size_t activeCandidateSelection() const;

    /**
     * @brief Chooses an alternative directly, rather than cycling to it.
     *
     * Needed because the candidate window lets the user click the option they want.
     * Cycling is fine for two options and tedious for five.
     *
     * @return False if either index is out of range, in which case nothing changes.
     */
    bool setActiveSelection(size_t tokenIndex, size_t optionIndex);

    /**
     * @brief Flushes the active candidates, returning the composed string and clearing active state.
     * Preserves any candidate selections made during typing or candidate cycling.
     */
    std::string flushActive();

    /// Clears the active candidate state.
    void clearActive();

    /// Human-readable derivation of a word: tokens, classes, contexts and chosen candidates.
    std::string explain(const std::string& romanInput);

    // Accessors for testing and inspection
    const SymbolTable& getSymbolTable() const { return m_symbolTable; }
    SymbolTable& getSymbolTable() { return m_symbolTable; }
    const ExceptionDictionary& getExceptions() const { return m_exceptions; }
    ExceptionDictionary& getExceptions() { return m_exceptions; }
    const std::vector<Candidate>& getActiveCandidates() const { return m_activeCandidates; }

private:
    /// Rebuilds the tokenizer and analyzer after the symbol table changes.
    void rebuildPipeline();

    SymbolTable m_symbolTable;
    ExceptionDictionary m_exceptions;
    std::unique_ptr<Tokenizer> m_tokenizer;
    std::unique_ptr<ContextAnalyzer> m_analyzer;
    std::unique_ptr<ICandidateResolver> m_resolver;
    UnicodeComposer m_composer;

    std::vector<Candidate> m_activeCandidates;

    /// Set when the active buffer matched the exception dictionary. Holds already-composed
    /// Bengali that must bypass UnicodeComposer entirely.
    std::string m_activeOverride;
};
