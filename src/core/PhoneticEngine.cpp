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
    std::vector<Candidate> oldCandidates = std::move(m_activeCandidates);
    m_activeCandidates = generateCandidates(romanBuffer);

    // Retain candidate selections from previous keystrokes for matching prefix tokens.
    // This ensures that when a user cycles a candidate (e.g. 'a' -> 'আ') and continues
    // typing (e.g. 'l' -> 'al'), the transliteration builds directly on top of the
    // cycled candidate rather than resetting to candidate 0.
    const size_t minCount = std::min(oldCandidates.size(), m_activeCandidates.size());
    for (size_t i = 0; i < minCount; ++i) {
        if (m_activeCandidates[i].romanToken == oldCandidates[i].romanToken &&
            m_activeCandidates[i].options == oldCandidates[i].options) {
            m_activeCandidates[i].selectedIndex = oldCandidates[i].selectedIndex;
        } else {
            // Token boundary shifted (e.g. 's' joined with 'h' to become 'sh')
            break;
        }
    }
}

bool PhoneticEngine::cycleActiveCandidate() {
    if (m_activeCandidates.empty()) {
        return false;
    }

    // Odometer-style multi-token cycling across ambiguous candidates from right to left:
    // Cycles the rightmost ambiguous candidate. If it wraps around back to index 0,
    // it carries over to cycle the next ambiguous candidate to its left.
    // This allows cycling both the current token and earlier ambiguous tokens in the word!
    for (auto it = m_activeCandidates.rbegin(); it != m_activeCandidates.rend(); ++it) {
        if (it->isAmbiguous()) {
            size_t oldIdx = it->selectedIndex;
            it->cycleNext();
            // If the candidate advanced without wrapping around to 0, stop carry
            if (it->selectedIndex > oldIdx) {
                return true;
            }
            // If it wrapped around, continue to carry over to the preceding ambiguous candidate
        }
    }

    // If all ambiguous candidates completed full rotation or none was ambiguous
    if (!m_activeCandidates.empty() && !m_activeCandidates.back().options.empty()) {
        return true;
    }

    return false;
}

std::string PhoneticEngine::getActiveComposedString() const {
    return m_composer.compose(m_activeCandidates);
}

std::string PhoneticEngine::flushActive() {
    std::string result = getActiveComposedString();
    clearActive();
    return result;
}

void PhoneticEngine::clearActive() {
    m_activeCandidates.clear();
}
