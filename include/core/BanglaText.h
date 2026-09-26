#pragma once

#include <string>

/**
 * @brief Canonical normalisation for Bengali text.
 *
 * Three Bengali letters have two encodings each:
 *
 *   ড়  U+09DC   or  ড U+09A1 + ় U+09BC
 *   ঢ়  U+09DD   or  ঢ U+09A2 + ় U+09BC
 *   য়  U+09DF   or  য U+09AF + ় U+09BC
 *
 * They render identically and compare unequal. That is the whole problem: a bug of this
 * kind is invisible on screen and silent in the logs. A dictionary storing one form simply
 * never matches an engine producing the other, and the only symptom is that prediction
 * quietly stops working for every word containing these letters -- which in Bangla is a
 * great many, since হয়, নিয়ে, বাড়ি and মেয়ে are all everyday words.
 *
 * We normalise to the **decomposed** form, and not arbitrarily: U+09DC, U+09DD and U+09DF
 * are on the Unicode Composition Exclusion list (UAX #15), so NFC deliberately does *not*
 * recompose them. Decomposed is therefore the standard-conformant canonical form, and the
 * precomposed characters exist only for round-tripping legacy encodings.
 *
 * Everything entering the dictionary and everything coming out of the fixed layout passes
 * through here, so the components cannot drift apart again.
 */
namespace BanglaText {

/// Returns `text` with precomposed nukta letters replaced by their canonical decompositions.
std::string normalize(const std::string& text);

/// True when `text` contains a precomposed nukta letter, i.e. normalize() would change it.
bool needsNormalization(const std::string& text);

} // namespace BanglaText
