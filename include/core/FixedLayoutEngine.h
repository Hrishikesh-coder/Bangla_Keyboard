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
    /**
     * @brief Which shift level a keystroke selects.
     *
     * Real fixed layouts are three-deep. Probhat puts the letters and matras on the base
     * and shift levels but keeps the nukta, the rupee sign, ৄ, ঌ, ৡ, ঽ, ৗ and the
     * currency-fraction signs on AltGr. A two-level implementation simply cannot type
     * those characters, which is why the earlier draft had to invent places for them.
     */
    enum class Level { Base, Shift, AltGr };

    FixedLayoutEngine() = default;

    /// Loads a layout from a JSON file. Returns false if missing or malformed.
    bool loadFromFile(const std::string& jsonFilePath);

    /// Loads a layout from a raw JSON string.
    bool loadFromString(const std::string& jsonContent);

    /// Human-readable layout name from the JSON "name" field.
    const std::string& layoutName() const { return m_layoutName; }

    /// True when the typed character has a mapping at the given level.
    bool isMapped(char key, Level level = Level::Base) const;

    /**
     * @brief Maps one typed character to its Bengali output.
     *
     * The caller passes the character the keyboard would normally produce, so Shift is
     * already reflected in `key` ('K' rather than 'k'). `level` exists for AltGr, which
     * produces no distinct character of its own.
     *
     * @return The mapped string, or a one-character string containing `key` when unmapped.
     */
    std::string mapKey(char key, Level level = Level::Base) const;

    /// Maps every character of a string at the base/shift levels.
    std::string mapText(const std::string& text) const;

    void clear();

    /// Total mappings across all three levels.
    size_t size() const;

    /// The map for one level, for the on-screen keyboard and the layout editor.
    const std::unordered_map<char, std::string>& keyMap(Level level = Level::Base) const;

    /// Updates or inserts a key mapping. If glyph is empty, removes the mapping.
    void setKey(char key, const std::string& glyph, Level level = Level::Base);

    /// Sets the layout name.
    void setLayoutName(const std::string& name) { m_layoutName = name; }

    /// Serializes the current layout to a formatted JSON string.
    std::string saveToString() const;

    /// Saves the current layout to a JSON file.
    bool saveToFile(const std::string& jsonFilePath) const;

private:
    /// Selects the map for a level. Shift and base share one map, keyed by the shifted
    /// character, because the keyboard already tells us which one was typed.
    const std::unordered_map<char, std::string>& mapFor(Level level) const;

    std::string m_layoutName;
    std::unordered_map<char, std::string> m_base;   ///< base + shift, keyed by typed character
    std::unordered_map<char, std::string> m_altgr;  ///< AltGr level
};
