#include "core/PhoneticEngine.h"

PhoneticEngine::PhoneticEngine()
    : m_tokenizer(std::make_unique<Tokenizer>(m_symbolTable)),
      m_resolver(std::make_unique<DefaultCandidateResolver>()) {
}

PhoneticEngine::PhoneticEngine(std::unique_ptr<ICandidateResolver> resolver)
    : m_tokenizer(std::make_unique<Tokenizer>(m_symbolTable)),
      m_resolver(std::move(resolver)) {
    if (!m_resolver) {
        m_resolver = std::make_unique<DefaultCandidateResolver>();
    }
}

bool PhoneticEngine::loadRules(const std::string& jsonFilePath) {
    bool ok = m_symbolTable.loadFromFile(jsonFilePath);
    if (ok) {
        m_tokenizer = std::make_unique<Tokenizer>(m_symbolTable);
    }
    return ok;
}

bool PhoneticEngine::loadRulesFromString(const std::string& jsonString) {
    bool ok = m_symbolTable.loadFromString(jsonString);
    if (ok) {
        m_tokenizer = std::make_unique<Tokenizer>(m_symbolTable);
    }
    return ok;
}

std::vector<Candidate> PhoneticEngine::generateCandidates(const std::string& romanInput) {
    std::vector<Candidate> candidates;
    if (romanInput.empty()) {
        return candidates;
    }

    std::vector<std::string> tokens = m_tokenizer->tokenize(romanInput);
    candidates.reserve(tokens.size());

    for (const auto& tok : tokens) {
        const auto* opts = m_symbolTable.lookup(tok);
        if (opts && !opts->empty()) {
            candidates.emplace_back(tok, *opts, 0);
        } else {
            // Unregistered token or punctuation/symbol: candidate option is the token itself
            candidates.emplace_back(tok, std::vector<std::string>{tok}, 0);
        }
    }

    // Resolve initial candidate indices
    for (size_t i = 0; i < candidates.size(); ++i) {
        size_t chosenIndex = m_resolver->resolve(candidates[i], i, candidates);
        candidates[i].selectedIndex = chosenIndex;
    }

    return candidates;
}

std::string PhoneticEngine::transliterate(const std::string& romanInput) {
    if (romanInput.empty()) {
        return "";
    }
    std::vector<Candidate> candidates = generateCandidates(romanInput);
    return m_composer.compose(candidates);
}

void PhoneticEngine::updateActiveBuffer(const std::string& romanBuffer) {
    m_activeCandidates = generateCandidates(romanBuffer);
}

bool PhoneticEngine::cycleActiveCandidate() {
    if (m_activeCandidates.empty()) {
        return false;
    }

    // Find the last ambiguous candidate in the active candidates list and cycle it
    for (auto it = m_activeCandidates.rbegin(); it != m_activeCandidates.rend(); ++it) {
        if (it->isAmbiguous()) {
            it->cycleNext();
            return true;
        }
    }

    // If none was ambiguous, cycle the very last candidate as fallback
    m_activeCandidates.back().cycleNext();
    return true;
}

std::string PhoneticEngine::getActiveComposedString() const {
    return m_composer.compose(m_activeCandidates);
}

void PhoneticEngine::clearActive() {
    m_activeCandidates.clear();
}
