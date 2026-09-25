#include "core/PhoneticEngine.h"

#include <cctype>
#include <sstream>

PhoneticEngine::PhoneticEngine()
    : m_tokenizer(std::make_unique<Tokenizer>(m_symbolTable)),
      m_analyzer(std::make_unique<ContextAnalyzer>(m_symbolTable)),
      m_resolver(std::make_unique<DefaultCandidateResolver>()) {
}

PhoneticEngine::PhoneticEngine(std::unique_ptr<ICandidateResolver> resolver)
    : m_tokenizer(std::make_unique<Tokenizer>(m_symbolTable)),
      m_analyzer(std::make_unique<ContextAnalyzer>(m_symbolTable)),
      m_resolver(std::move(resolver)) {
    if (!m_resolver) {
        m_resolver = std::make_unique<DefaultCandidateResolver>();
    }
}

void PhoneticEngine::rebuildPipeline() {
    m_tokenizer = std::make_unique<Tokenizer>(m_symbolTable);
    m_analyzer = std::make_unique<ContextAnalyzer>(m_symbolTable);
}

bool PhoneticEngine::loadRules(const std::string& jsonFilePath) {
    bool ok = m_symbolTable.loadFromFile(jsonFilePath);
    if (ok) {
        rebuildPipeline();
    }
    return ok;
}

bool PhoneticEngine::loadRulesFromString(const std::string& jsonString) {
    bool ok = m_symbolTable.loadFromString(jsonString);
    if (ok) {
        rebuildPipeline();
    }
    return ok;
}

bool PhoneticEngine::loadExceptions(const std::string& jsonFilePath) {
    return m_exceptions.loadFromFile(jsonFilePath);
}

bool PhoneticEngine::loadExceptionsFromString(const std::string& jsonString) {
    return m_exceptions.loadFromString(jsonString);
}

std::vector<Candidate> PhoneticEngine::generateCandidates(const std::string& romanInput) {
    std::vector<Candidate> candidates;
    if (romanInput.empty()) {
        return candidates;
    }

    // Pass 1: longest-match-first segmentation.
    std::vector<std::string> tokens = m_tokenizer->tokenize(romanInput);

    // Pass 2: annotate each token with its surrounding context, before any candidate is chosen.
    std::vector<uint32_t> contexts = m_analyzer->analyze(tokens);

    candidates.reserve(tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto* opts = m_symbolTable.lookupContextual(tokens[i], contexts[i]);
        if (opts && !opts->empty()) {
            candidates.emplace_back(tokens[i], *opts, 0);
        } else {
            // Unregistered token, or a rule whose contextual variant is empty: emit the
            // Roman text unchanged so punctuation and digits survive.
            candidates.emplace_back(tokens[i], std::vector<std::string>{tokens[i]}, 0);
        }
    }

    // The resolver still owns the final index choice within the context-selected list,
    // so a future dictionary- or frequency-driven resolver plugs in here unchanged.
    for (size_t i = 0; i < candidates.size(); ++i) {
        candidates[i].selectedIndex = m_resolver->resolve(candidates[i], i, candidates);
    }

    return candidates;
}

std::string PhoneticEngine::transliterate(const std::string& romanInput) {
    if (romanInput.empty()) {
        return "";
    }

    // Whole-word overrides win over the rules. This is also how English words are kept
    // verbatim: they map to themselves in exceptions.json.
    std::string override;
    if (m_exceptions.lookup(romanInput, override)) {
        return override;
    }

    std::vector<Candidate> candidates = generateCandidates(romanInput);
    return m_composer.compose(candidates);
}

std::string PhoneticEngine::transliterateText(const std::string& romanText) {
    std::string out;
    std::string word;

    auto flush = [&]() {
        if (!word.empty()) {
            out += transliterate(word);
            word.clear();
        }
    };

    for (char ch : romanText) {
        // A character that starts no registered token is a word boundary.
        std::string single(1, ch);
        if (m_symbolTable.hasToken(single) || std::isalpha(static_cast<unsigned char>(ch))) {
            word += ch;
        } else {
            flush();
            out += ch;
        }
    }
    flush();
    return out;
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

int PhoneticEngine::activeAmbiguousTokenIndex() const {
    for (size_t i = m_activeCandidates.size(); i > 0; --i) {
        if (m_activeCandidates[i - 1].isAmbiguous()) {
            return static_cast<int>(i - 1);
        }
    }
    return -1;
}

std::vector<std::string> PhoneticEngine::activeCandidateOptions() const {
    const int index = activeAmbiguousTokenIndex();
    if (index < 0) {
        return {};
    }
    return m_activeCandidates[static_cast<size_t>(index)].options;
}

size_t PhoneticEngine::activeCandidateSelection() const {
    const int index = activeAmbiguousTokenIndex();
    if (index < 0) {
        return 0;
    }
    return m_activeCandidates[static_cast<size_t>(index)].selectedIndex;
}

bool PhoneticEngine::setActiveSelection(size_t tokenIndex, size_t optionIndex) {
    if (tokenIndex >= m_activeCandidates.size()) {
        return false;
    }
    Candidate& candidate = m_activeCandidates[tokenIndex];
    if (optionIndex >= candidate.options.size()) {
        return false;
    }
    candidate.selectedIndex = optionIndex;
    return true;
}

std::string PhoneticEngine::flushActive() {
    std::string result = getActiveComposedString();
    clearActive();
    return result;
}

void PhoneticEngine::clearActive() {
    m_activeCandidates.clear();
}

std::string PhoneticEngine::explain(const std::string& romanInput) {
    std::ostringstream out;

    std::string override;
    if (m_exceptions.lookup(romanInput, override)) {
        out << "  exception dictionary override: \"" << romanInput << "\" => \"" << override << "\"\n";
        return out.str();
    }

    std::vector<std::string> tokens = m_tokenizer->tokenize(romanInput);
    std::vector<uint32_t> contexts = m_analyzer->analyze(tokens);
    std::vector<Candidate> candidates = generateCandidates(romanInput);

    for (size_t i = 0; i < tokens.size(); ++i) {
        out << "  \"" << tokens[i] << "\""
            << "  class=" << tokenClassName(m_symbolTable.classOf(tokens[i]))
            << "  context=" << describeContext(contexts[i])
            << "  -> \"" << (i < candidates.size() ? candidates[i].selected() : std::string()) << "\"";
        if (i < candidates.size() && candidates[i].isAmbiguous()) {
            out << "  (" << candidates[i].options.size() << " candidates)";
        }
        out << "\n";
    }
    out << "  composed: " << m_composer.compose(candidates) << "\n";
    return out.str();
}
