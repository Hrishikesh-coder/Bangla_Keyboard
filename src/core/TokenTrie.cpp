#include "core/TokenTrie.h"
#include <algorithm>

void TokenTrie::insert(const std::string& token, int weight) {
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
    // Set or update the frequency weight (higher is better)
    node->weight = std::max(node->weight, weight);
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

void TokenTrie::collectCompletions(const Node* node, std::string currentPrefix, 
                                   std::unordered_map<std::string, int>& foundCompletions, int penalty) const {
    if (!node) return;
    
    if (node->terminal) {
        int score = node->weight - penalty;
        // Keep highest score if reached multiple ways
        auto it = foundCompletions.find(currentPrefix);
        if (it == foundCompletions.end() || it->second < score) {
            foundCompletions[currentPrefix] = score;
        }
    }
    
    for (size_t i = 0; i < 128; ++i) {
        if (node->children[i]) {
            collectCompletions(node->children[i].get(), currentPrefix + static_cast<char>(i), foundCompletions, penalty);
        }
    }
}

void TokenTrie::fuzzyCollect(const Node* node, const std::string& target, size_t targetPos, 
                             std::string currentPath, int editsLeft, 
                             std::unordered_map<std::string, int>& foundCompletions) const {
    if (!node) return;

    if (targetPos == target.length()) {
        // We reached the end of the prefix. 
        // 1 typo = 1000 penalty. This ensures EXACT matches always appear before fuzzy ones!
        int initialEdits = 1; // Assuming max 1 edit
        int editsUsed = initialEdits - editsLeft;
        int penalty = editsUsed * 1000;
        
        collectCompletions(node, currentPath, foundCompletions, penalty);
        return;
    }

    char expectedChar = target[targetPos];
    auto expectedIndex = static_cast<unsigned char>(expectedChar);

    // 1. Exact Match (No penalty)
    if (expectedIndex < 128 && node->children[expectedIndex]) {
        fuzzyCollect(node->children[expectedIndex].get(), target, targetPos + 1, 
                     currentPath + expectedChar, editsLeft, foundCompletions);
    }

    // 2. Fuzzy Branches (Typo Tolerance)
    if (editsLeft > 0) {
        // Deletion: The user typed an extra letter by accident. We skip it in the target.
        fuzzyCollect(node, target, targetPos + 1, currentPath, editsLeft - 1, foundCompletions);

        for (size_t i = 0; i < 128; ++i) {
            if (node->children[i]) {
                char childChar = static_cast<char>(i);
                
                // Substitution: User hit the wrong key. Skip target char, consume trie char.
                if (i != expectedIndex) {
                    fuzzyCollect(node->children[i].get(), target, targetPos + 1, 
                                 currentPath + childChar, editsLeft - 1, foundCompletions);
                }
                
                // Insertion: User missed a key. Keep target char, consume trie char.
                fuzzyCollect(node->children[i].get(), target, targetPos, 
                             currentPath + childChar, editsLeft - 1, foundCompletions);
            }
        }
    }
}

std::vector<std::string> TokenTrie::getCompletions(const std::string& prefix, size_t limit) const {
    std::unordered_map<std::string, int> foundCompletions;
    
    // We allow 1 edit distance for typo tolerance ONLY for longer prefixes
    // Otherwise it matches the entire dictionary by deleting the 1st character!
    int maxEdits = (prefix.length() > 2) ? 1 : 0; 
    
    fuzzyCollect(&m_root, prefix, 0, "", maxEdits, foundCompletions);
    
    // Transfer to vector for priority sorting (Best-First)
    std::vector<std::pair<std::string, int>> sortedList(foundCompletions.begin(), foundCompletions.end());
    
    std::sort(sortedList.begin(), sortedList.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second; // Descending score (Frequency/Priority)
        }
        if (a.first.length() != b.first.length()) {
            return a.first.length() < b.first.length(); // Ascending length
        }
        return a.first < b.first; // Alphabetical fallback
    });
    
    std::vector<std::string> results;
    for (const auto& item : sortedList) {
        results.push_back(item.first);
        if (limit > 0 && results.size() >= limit) {
            break;
        }
    }
    
    return results;
}
