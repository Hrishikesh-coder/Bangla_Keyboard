#include "core/WordDictionary.h"
#include "core/BanglaText.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// UTF-8 <-> codepoints
// ---------------------------------------------------------------------------

std::vector<char32_t> WordDictionary::toCodepoints(const std::string& utf8) {
    std::vector<char32_t> out;
    out.reserve(utf8.size() / 3 + 1);

    size_t i = 0;
    while (i < utf8.size()) {
        const auto lead = static_cast<unsigned char>(utf8[i]);
        size_t length = 1;
        char32_t cp = lead;

        if ((lead & 0x80) == 0x00)      { length = 1; cp = lead; }
        else if ((lead & 0xE0) == 0xC0) { length = 2; cp = lead & 0x1F; }
        else if ((lead & 0xF0) == 0xE0) { length = 3; cp = lead & 0x0F; }
        else if ((lead & 0xF8) == 0xF0) { length = 4; cp = lead & 0x07; }
        else {
            // Stray continuation byte: keep it as-is rather than dropping data silently.
            out.push_back(lead);
            ++i;
            continue;
        }

        if (i + length > utf8.size()) {
            out.push_back(lead); // truncated sequence at end of input
            ++i;
            continue;
        }

        for (size_t k = 1; k < length; ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + k]) & 0x3F);
        }
        out.push_back(cp);
        i += length;
    }
    return out;
}

std::string WordDictionary::fromCodepoints(const std::vector<char32_t>& codepoints) {
    std::string out;
    for (char32_t cp : codepoints) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

bool WordDictionary::loadFromFile(const std::string& jsonFilePath) {
    std::ifstream file(jsonFilePath);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

bool WordDictionary::loadFromString(const std::string& jsonContent) {
    try {
        json j = json::parse(jsonContent);
        clear();

        const json& words = (j.is_object() && j.contains("words")) ? j["words"] : j;

        if (words.is_object()) {
            for (auto& [word, frequency] : words.items()) {
                if (!word.empty() && word[0] == '_') {
                    continue; // metadata
                }
                add(word, frequency.is_number_unsigned()
                              ? frequency.get<uint32_t>()
                              : 1u);
            }
        } else if (words.is_array()) {
            for (const auto& entry : words) {
                if (entry.is_string()) {
                    add(entry.get<std::string>(), 1);
                }
            }
        } else {
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[WordDictionary] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

void WordDictionary::add(const std::string& word, uint32_t frequency) {
    if (word.empty()) {
        return;
    }
    // Normalise on the way in. A corpus word list is mixed: the OpenSubtitles Bengali
    // list has ~2300 precomposed nukta letters in its top 20k entries alongside decomposed
    // ones, and storing both forms would mean the engine matches some and not others.
    const std::string canonical = BanglaText::normalize(word);
    Node* node = &m_root;
    for (char32_t cp : toCodepoints(canonical)) {
        auto& child = node->children[cp];
        if (!child) {
            child = std::make_unique<Node>();
        }
        node = child.get();
    }
    if (node->frequency == 0) {
        ++m_wordCount;
    }
    node->frequency = std::max<uint32_t>(frequency, 1);
}

void WordDictionary::clear() {
    m_root.children.clear();
    m_root.frequency = 0;
    m_wordCount = 0;
}

// ---------------------------------------------------------------------------
// Prefix prediction
// ---------------------------------------------------------------------------

const WordDictionary::Node* WordDictionary::walk(const std::vector<char32_t>& codepoints) const {
    const Node* node = &m_root;
    for (char32_t cp : codepoints) {
        auto it = node->children.find(cp);
        if (it == node->children.end()) {
            return nullptr;
        }
        node = it->second.get();
    }
    return node;
}

bool WordDictionary::contains(const std::string& word) const {
    const Node* node = walk(toCodepoints(BanglaText::normalize(word)));
    return node && node->frequency > 0;
}

uint32_t WordDictionary::getFrequency(const std::string& word) const {
    const Node* node = walk(toCodepoints(word));
    return (node != nullptr) ? node->frequency : 0;
}

bool WordDictionary::hasPrefix(const std::string& prefix) const {
    if (prefix.empty()) {
        return false;
    }
    return walk(toCodepoints(prefix)) != nullptr;
}

void WordDictionary::collect(const Node* node,
                             std::vector<char32_t>& prefix,
                             std::vector<WordSuggestion>& out) const {
    if (node->frequency > 0) {
        out.push_back(WordSuggestion{ fromCodepoints(prefix), node->frequency, 0 });
    }
    for (const auto& [cp, child] : node->children) {
        prefix.push_back(cp);
        collect(child.get(), prefix, out);
        prefix.pop_back();
    }
}

std::vector<WordSuggestion> WordDictionary::topWords(size_t limit) const {
    std::vector<WordSuggestion> all;
    std::vector<char32_t> prefix;
    collect(&m_root, prefix, all);

    std::sort(all.begin(), all.end(), [](const WordSuggestion& a, const WordSuggestion& b) {
        if (a.frequency != b.frequency) {
            return a.frequency > b.frequency;
        }
        return a.word < b.word;
    });

    if (limit > 0 && all.size() > limit) {
        all.resize(limit);
    }
    return all;
}

std::vector<WordSuggestion> WordDictionary::predict(const std::string& prefix,
                                                    size_t limit) const {
    std::vector<WordSuggestion> out;
    if (prefix.empty()) {
        return out;
    }

    std::vector<char32_t> codepoints = toCodepoints(BanglaText::normalize(prefix));
    const Node* start = walk(codepoints);
    if (!start) {
        return out;
    }

    // Collect the whole subtree, then rank. The subtree under a two- or three-letter
    // Bengali prefix is small enough that ranking afterwards beats threading a heap
    // through the traversal.
    collect(start, codepoints, out);

    std::sort(out.begin(), out.end(), [](const WordSuggestion& a, const WordSuggestion& b) {
        if (a.frequency != b.frequency) {
            return a.frequency > b.frequency;
        }
        return a.word < b.word; // stable, predictable ordering for equal frequencies
    });

    if (limit > 0 && out.size() > limit) {
        out.resize(limit);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Fuzzy correction
// ---------------------------------------------------------------------------

void WordDictionary::searchFuzzy(const Node* node,
                                 char32_t letter,
                                 const std::vector<char32_t>& target,
                                 const std::vector<int>& previousRow,
                                 std::vector<char32_t>& current,
                                 int maxDistance,
                                 std::vector<WordSuggestion>& out) const {
    const size_t columns = target.size() + 1;

    // One row of the Levenshtein matrix, for the word spelled by the path to this node.
    std::vector<int> row(columns);
    row[0] = previousRow[0] + 1;

    for (size_t i = 1; i < columns; ++i) {
        const int insertCost = row[i - 1] + 1;
        const int deleteCost = previousRow[i] + 1;
        const int replaceCost = previousRow[i - 1] + (target[i - 1] == letter ? 0 : 1);
        row[i] = std::min({ insertCost, deleteCost, replaceCost });
    }

    current.push_back(letter);

    if (node->frequency > 0 && row[columns - 1] <= maxDistance) {
        out.push_back(WordSuggestion{ fromCodepoints(current),
                                      node->frequency,
                                      row[columns - 1] });
    }

    // The smallest value in the row is the best score any descendant can achieve, because
    // every extra letter can only add to the distance. If even that exceeds the threshold,
    // the entire subtree is hopeless and is skipped -- this is what keeps the search cheap
    // enough to run on every keystroke.
    if (*std::min_element(row.begin(), row.end()) <= maxDistance) {
        for (const auto& [cp, child] : node->children) {
            searchFuzzy(child.get(), cp, target, row, current, maxDistance, out);
        }
    }

    current.pop_back();
}

std::vector<WordSuggestion> WordDictionary::correct(const std::string& word,
                                                    int maxDistance,
                                                    size_t limit) const {
    std::vector<WordSuggestion> out;
    if (word.empty() || maxDistance < 0) {
        return out;
    }

    const std::vector<char32_t> target = toCodepoints(BanglaText::normalize(word));

    std::vector<int> firstRow(target.size() + 1);
    for (size_t i = 0; i < firstRow.size(); ++i) {
        firstRow[i] = static_cast<int>(i);
    }

    std::vector<char32_t> current;
    for (const auto& [cp, child] : m_root.children) {
        searchFuzzy(child.get(), cp, target, firstRow, current, maxDistance, out);
    }

    // Closest first; among equally close words, the more common one is the better guess.
    std::sort(out.begin(), out.end(), [](const WordSuggestion& a, const WordSuggestion& b) {
        if (a.distance != b.distance)   return a.distance < b.distance;
        if (a.frequency != b.frequency) return a.frequency > b.frequency;
        return a.word < b.word;
    });

    if (limit > 0 && out.size() > limit) {
        out.resize(limit);
    }
    return out;
}
