#include "core/TokenTrie.h"

void TokenTrie::insert(const std::string& token) {
    if (token.empty()) {
        return;
    }

    // Validate upfront to prevent creating dead branches on invalid tokens
    for (char ch : token) {
        if (static_cast<unsigned char>(ch) >= 128) {
            return;
        }
    }

    Node* node = &m_root;
    for (char ch : token) {
        auto index = static_cast<unsigned char>(ch);
        if (!node->children[index]) {
            node->children[index] = std::make_unique<Node>();
        }
        node = node->children[index].get();
    }

    if (!node->terminal) {
        node->terminal = true;
        ++m_size;
    }
}

void TokenTrie::clear() {
    for (auto& child : m_root.children) {
        child.reset();
    }
    m_root.terminal = false;
    m_size = 0;
}

size_t TokenTrie::longestMatchLength(const std::string& input, size_t startPos) const {
    const Node* node = &m_root;
    size_t bestLength = 0;

    for (size_t i = startPos; i < input.size(); ++i) {
        auto index = static_cast<unsigned char>(input[i]);
        if (index >= 128) {
            break;
        }
        node = node->children[index].get();
        if (!node) {
            break;
        }
        if (node->terminal) {
            bestLength = i - startPos + 1;
        }
    }

    return bestLength;
}

bool TokenTrie::contains(const std::string& token) const {
    const Node* node = &m_root;
    for (char ch : token) {
        auto index = static_cast<unsigned char>(ch);
        if (index >= 128 || !node->children[index]) {
            return false;
        }
        node = node->children[index].get();
    }
    return node->terminal;
}

void TokenTrie::collectCompletions(const Node* node, std::string currentPrefix, std::vector<std::string>& results, size_t limit) const {
    if (!node || (limit > 0 && results.size() >= limit)) {
        return;
    }
    
    if (node->terminal) {
        results.push_back(currentPrefix);
    }
    
    for (size_t i = 0; i < 128; ++i) {
        if (node->children[i]) {
            collectCompletions(node->children[i].get(), currentPrefix + static_cast<char>(i), results, limit);
            if (limit > 0 && results.size() >= limit) {
                return;
            }
        }
    }
}

std::vector<std::string> TokenTrie::getCompletions(const std::string& prefix, size_t limit) const {
    std::vector<std::string> results;
    
    const Node* node = &m_root;
    for (char ch : prefix) {
        auto index = static_cast<unsigned char>(ch);
        if (index >= 128 || !node->children[index]) {
            return results; // Prefix not found
        }
        node = node->children[index].get();
    }
    
    collectCompletions(node, prefix, results, limit);
    return results;
}
