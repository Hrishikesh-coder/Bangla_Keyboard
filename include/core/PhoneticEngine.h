#pragma once

#include "SymbolTable.h"
#include "Tokenizer.h"
#include "Candidate.h"
#include "CandidateResolver.h"
#include "UnicodeComposer.h"
#include <string>
#include <vector>
#include <memory>

/**
 * @brief Main engine orchestrating Tokenizer, SymbolTable, CandidateResolver, and UnicodeComposer.
 *
 * Provides high-level transliteration of Roman input to Bengali Unicode text.
 * Also maintains state for candidate cycling on the active buffer.
 */
class PhoneticEngine {
public:
    PhoneticEngine();
    explicit PhoneticEngine(std::unique_ptr<ICandidateResolver> resolver);

    /// Loads phonetic rules from a JSON file.
    bool loadRules(const std::string& jsonFilePath);

    /// Loads phonetic rules directly from a JSON string.
    bool loadRulesFromString(const std::string& jsonString);

    /**
     * @brief High-level one-shot transliteration.
     * Takes a Roman input string (e.g. "shanti") and returns the composed Bengali string.
     */
    std::string transliterate(const std::string& romanInput);

    /**
     * @brief Generates Candidate objects for the input string without final composition.
     * Candidate options and initial selection indices are populated.
     */
    std::vector<Candidate> generateCandidates(const std::string& romanInput);

    /**
     * @brief Updates the active candidates for an ongoing input buffer.
     */
    void updateActiveBuffer(const std::string& romanBuffer);

    /**
     * @brief Cycles the last ambiguous candidate in the active candidates list.
     * @return True if a candidate was successfully cycled; false if no ambiguous candidate existed.
     */
    bool cycleActiveCandidate();

    /**
     * @brief Returns the composed Bengali string from the current active candidates list.
     */
    std::string getActiveComposedString() const;

    /// Clears the active candidate state.
    void clearActive();

    // Accessors for testing and inspection
    const SymbolTable& getSymbolTable() const { return m_symbolTable; }
    SymbolTable& getSymbolTable() { return m_symbolTable; }
    const std::vector<Candidate>& getActiveCandidates() const { return m_activeCandidates; }

private:
    SymbolTable m_symbolTable;
    std::unique_ptr<Tokenizer> m_tokenizer;
    std::unique_ptr<ICandidateResolver> m_resolver;
    UnicodeComposer m_composer;

    std::vector<Candidate> m_activeCandidates;
};
