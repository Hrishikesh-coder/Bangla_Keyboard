#pragma once

#include <string>
#include <vector>

/**
 * @brief Represents one phonetic token and its candidate Bengali outputs.
 *
 * For example, Roman token "sh" maps to options ["শ", "ষ", "স"].
 * An empty string ("") in options represents an epsilon / no-output candidate.
 */
struct Candidate {
    std::string romanToken;
    std::vector<std::string> options;
    size_t selectedIndex = 0;

    Candidate() = default;

    Candidate(std::string token, std::vector<std::string> opts, size_t defaultIndex = 0)
        : romanToken(std::move(token)), options(std::move(opts)), selectedIndex(defaultIndex) {
        if (selectedIndex >= options.size() && !options.empty()) {
            selectedIndex = 0;
        }
    }

    /// Returns the currently selected Bengali string candidate.
    const std::string& selected() const {
        static const std::string emptyString;
        if (options.empty()) {
            return emptyString;
        }
        return options[selectedIndex % options.size()];
    }

    /// Cycles to the next candidate option (wrap-around).
    void cycleNext() {
        if (!options.empty()) {
            selectedIndex = (selectedIndex + 1) % options.size();
        }
    }

    /// Whether this candidate has multiple alternatives to cycle through.
    bool isAmbiguous() const {
        return options.size() > 1;
    }
};
