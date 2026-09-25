#include "core/TokenTrie.h"

void TokenTrie::insert(const std::string& token) {
    if (token.empty()) {
        return;
    }

    Node* node = &m_root;
    for (char ch : token) {
        auto index = static_cast<unsigned char>(ch);
        if (index >= 128) {
            // Non-ASCII bytes cannot appear in a Roman phonetic token; ignore the entry
            // rather than corrupting the trie.
            return;
        }
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
            // Remember this match but keep walking: a longer token may still follow.
            bestLength = i - startPos + 1;
        }
    }

    return bestLength;
}

bool TokenTrie::contains(const std::string& token) const {
    const Node* node = &m_root;
    for (char ch : token) {
        auto index = static_cast<unsigned char>(ch);
        if (index >= 128) {
            return false;
        }
        node = node->children[index].get();
        if (!node) {
            return false;
        }
    }
    return node->terminal;
}
