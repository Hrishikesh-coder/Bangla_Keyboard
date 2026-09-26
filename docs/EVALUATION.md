# Evaluation

Reproduce with `make bench` (or `bin\benchmark_resolver.exe 2000`). Source:
`tools/benchmark_resolver.cpp`.

## What is being measured

Bangla phonetic input is ambiguous by construction. শ, ষ and স are homophones; so are ন and
ণ, and ই and ঈ. A typist does not type the disambiguating form — they type the easy one,
`sh` for both শ and ষ, `n` for both ন and ণ — and expect the IME to work out which was
meant. Recovering that is `DictionaryCandidateResolver`'s entire job.

**Method.** Take 2000 words from the corpus list, reverse-transliterate each into the Roman
a *lazy* typist would produce (wherever several Bengali letters share a Roman token, use the
shared token, discarding the information the resolver must recover), run that Roman forward
through the engine, and compare against the original word.

**Sample.** Words ranked 2000–4000 by frequency, not the top 2000. The most frequent words
are pronouns and particles with little ambiguity; scoring on them would understate the
problem.

## Results

Homophone-bearing words — the 1922 of 1935 scored words containing at least one ambiguous
letter, i.e. where the resolver has real work to do:

| Resolver | Correct | Accuracy |
|---|---|---|
| `DefaultCandidateResolver` (always candidate 0) | 751 / 1922 | **39.1%** |
| `DictionaryCandidateResolver`, word present in dictionary | 1592 / 1922 | **82.8%** |
| `DictionaryCandidateResolver`, **word held out** | 961 / 1922 | **50.0%** |

## Closing the generalisation gap

Two tiers were added below whole-word lookup, both measured on the same held-out split.
The margin for the character model was tuned on words ranked 6000–8000 and reported on
words ranked 2000–4000, so the number below is not fitted to itself.

| Resolver on held-out words | Correct | Accuracy |
|---|---|---|
| whole-word lookup only | 961 / 1922 | 50.0% |
| + morphological stem lookup | 1019 / 1922 | **53.0%** |
| + character trigram model (margin 4) | 1039 / 1922 | **54.1%** |

**Morphological stripping** is the larger and simpler win. Bangla is agglutinative: বাড়িতে
is rarely in a word list, বাড়ি always is, and the ambiguous letters live in the stem rather
than the ending. About 80 lines of suffix stripping buys three points.

**The character model needed a confidence margin, and the negative result is the
interesting part.** Applied unconditionally it made accuracy *worse* — 49.6%, below the
50.0% baseline. Candidate order in `phonetic_rules.json` is itself a linguistic prior (শ
before ষ before স is a claim about which is likelier), and an unconstrained trigram model
was overriding that prior with something marginally different and frequently worse. Only
allowing it to override when it is substantially more confident turns a 0.4-point loss into
a 1.1-point gain:

| Margin | Held-out accuracy |
|---|---|
| 0 (always override) | 49.6% |
| 1 | 52.1% |
| 2 | 52.9% |
| **4** | **54.1%** |
| 8 | 53.9% |
| 16 | 53.1% |

The validation slice independently peaks at 4, so the value is not an artefact of the test
set.

The honest summary: statistical fallbacks recover **4.1 points** of the 32.8-point gap
between knowing a word and not knowing it. Useful, and far short of a substitute for
coverage. Anyone hoping a language model would replace the dictionary should read that
table first.

## Reading the numbers

**39.1% → 82.8%** is what the resolver achieves when the word is in its dictionary. It more
than doubles accuracy, which confirms the mechanism works.

**39.1% → 50.0% is the honest generalisation number.** "Held out" rebuilds the resolver's
dictionary with the 2000 test words removed, so it has to spell words it has never seen
using only the statistics of the words it has. Scoring a lookup-based resolver on words
inside its own lookup table measures memorisation, and any lookup table passes that test.

The gap between 82.8% and 50.0% is the most useful thing here: **dictionary coverage is the
dominant variable.** Roughly two thirds of the resolver's benefit comes from having the word
at all. That is directly actionable — growing the word list improves real accuracy far more
than tuning the resolution logic would.

## Threats to validity, stated plainly

- **Reverse transliteration is a model of a typist, not a typist.** It assumes people always
  reach for the shortest lowercase key. Real users are less consistent, so the real-world
  ambiguity rate is probably higher and all three numbers somewhat optimistic.
- **65 of 2000 words (3.3%) were unreachable** — the rule set has no Roman spelling for some
  part of them — and are excluded from every score. Including them would depress all three
  numbers equally and measure the rule table rather than the resolver.
- **Subtitle corpus skew.** The word list comes from OpenSubtitles, so conversational
  vocabulary is over-represented relative to formal writing. Fair for an IME used mainly for
  messaging; a Wikipedia-derived list would suit document editing better.
- **Held-out still shares the corpus distribution.** Test words were removed from the
  dictionary, but they come from the same source as the training words, so this measures
  generalisation to unseen *words*, not to a different domain.

## An earlier version of this benchmark was wrong

The first implementation reverse-transliterated by greedy longest-match over Bengali
strings. It reported 92% of words as unreachable, which meant it was measuring the quality
of the reverse map rather than the resolver — the headline numbers from that run (11.5% vs
18.5%) were meaningless and are recorded here only so nobody quotes them.

The cause is worth understanding: a flat string match cannot reconstruct the two things
`UnicodeComposer` *inserts* rather than copies — the virama between stacked consonants, and
the inherent vowel, which is written as nothing at all. The reverse walk has to mirror the
composer's structure. After fixing that, unreachable fell from 92% to 3.3%.
