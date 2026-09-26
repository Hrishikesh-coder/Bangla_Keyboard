#pragma once

#include "core/WordDictionary.h"

#include <string>
#include <unordered_map>

/**
 * @brief Words this particular user has taught the IME, persisted across sessions.
 *
 * The measured weakness of dictionary resolution is coverage: on a held-out split,
 * whole-word lookup scored 82.8% on words it knew and 50.0% on words it did not. No
 * shipped word list closes that gap, because the missing words are largely names,
 * workplace jargon and loanwords -- which differ per person and are exactly the words
 * someone types most often.
 *
 * So learn them. Every time the user overrides the engine -- cycling a candidate,
 * clicking a correction, picking a prediction -- they have stated what they meant. That is
 * a label, free and unambiguous, and it costs nothing to keep.
 *
 * Learned words are weighted above the corpus floor so a personal spelling wins over a
 * statistically-commoner one: someone whose surname is রয় should not have it silently
 * turned into রায় forever.
 */
class UserDictionary {
public:
    UserDictionary() = default;

    /// %APPDATA%\Shobdomala\learned.json, beside the settings file.
    static std::string defaultPath();

    bool load(const std::string& path);
    bool save(const std::string& path) const;

    /**
     * @brief Records that the user chose this spelling.
     *
     * Repeated confirmations raise its weight, so a word typed daily outranks one
     * corrected once months ago.
     */
    void learn(const std::string& word);

    /// Removes a learned word, for when something was recorded by accident.
    bool forget(const std::string& word);

    /// Merges the learned words into a dictionary the resolver already consults.
    void mergeInto(WordDictionary& dictionary) const;

    bool contains(const std::string& word) const;
    size_t size() const { return m_learned.size(); }
    void clear() { m_learned.clear(); }

    /// Weight given to a word the first time it is learned.
    static constexpr uint32_t kInitialWeight = 4000;
    /// Added on each subsequent confirmation.
    static constexpr uint32_t kReinforcement = 1000;

private:
    std::unordered_map<std::string, uint32_t> m_learned;
};
