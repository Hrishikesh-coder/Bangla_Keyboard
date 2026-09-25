#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Tier-1 engine: a deterministic key -> Bengali glyph map (Probhat / Jatiya style).
 *
 * This is the other half of how Bengali is actually typed. Phonetic input guesses; a fixed
 * layout does not. Every key produces exactly one glyph, the typist presses the hasant key
 * themselves to build a conjunct, and nothing depends on context. Professional Bengali
 * typists use fixed layouts precisely because they are predictable.
 *
 * Deliberately stateless: no buffer, no candidates, no composition pass. A keystroke maps to
 * a string and is injected immediately, so it needs none of the PhoneticEngine machinery.
 *
 * The map is loaded from JSON at runtime, so a different layout - or one a user builds in a
 * layout editor - is a data file rather than a code change.
 */
class FixedLayoutEngine {
public:
    FixedLayoutEngine() = default;

    /// Loads a layout from a JSON file. Returns false if missing or malformed.
    bool loadFromFile(const std::string& jsonFilePath);

    /// Loads a layout from a raw JSON string.
    bool loadFromString(const std::string& jsonContent);

    /// Human-readable layout name from the JSON "name" field.
    const std::string& layoutName() const { return m_layoutName; }

    /// True when the typed character has a mapping in this layout.
    bool isMapped(char key) const;

    /**
     * @brief Maps one typed character to its Bengali output.
     * @return The mapped string, or a one-character string containing `key` when unmapped.
     */
    std::string mapKey(char key) const;

    /// Maps every character of a string. Unmapped characters pass through unchanged.
    std::string mapText(const std::string& text) const;

    void clear();
    size_t size() const { return m_map.size(); }

    /// The whole key map, for the on-screen keyboard and layout editor.
    const std::unordered_map<char, std::string>& keyMap() const { return m_map; }

private:
    std::string m_layoutName;
    std::unordered_map<char, std::string> m_map;
};
