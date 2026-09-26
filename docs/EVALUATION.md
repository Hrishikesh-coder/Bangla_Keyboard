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
| `DefaultCandidateResolver` (always candidate 0) | 776 / 1924 | **40.3%** |
| `DictionaryCandidateResolver`, word present in dictionary | 1708 / 1924 | **88.8%** |
| `DictionaryCandidateResolver`, **word held out** | 1088 / 1924 | **56.5%** |

## Finding the errors before guessing at fixes

The first round of improvements was chosen by reasoning about what *might* be wrong. The
second was chosen by counting what actually was: for every failure, which letters came out
different, and whether the resolver's search had been truncated.

Two causes dominated, and neither was the one I would have guessed.

**`ri` was tokenised as ঋ** — 45 occurrences in a 2000-word sample, more than twice the next
cause. Maximal munch took `ri` as a single token, which made রি unspellable: প্রিয় could not
be typed `priyo` at all. Counting the corpus settles it — র+ি appears in words totalling
38,181 occurrences, ৃ in 8,733, ঋ in 216 — so the greedy match was wrong roughly four times
for every time it was right. The token is now gone; ঋ and ৃ are typed `rri`, which is also
Avro's convention. This was a **tokenizer** bug that had been costing the *resolver* its
accuracy score.

**The search cap was hit in 30.8% of failures.** The resolver explored at most 5 ambiguous
tokens and 64 combinations, and was giving up before reaching the answer. Raising it to
8 and 512 recovered most of the rest; beyond that the curve is flat.

| Search limits | Held-out accuracy |
|---|---|
| 5 tokens, 64 combinations | 55.7% |
| **8 tokens, 512 combinations** | **56.4%** |
| 10 / 4096 | 56.5% |
| 12 / 32768 | 56.5% |

Cost: **0.54 ms per keystroke** for a sixteen-character word with the full pipeline —
contextual resolution, dictionary search, morphology and the character model. Windows
unhooks a low-level hook that exceeds 300 ms, so that is a factor of 550 in hand.

After both fixes, in-dictionary failures fell from 17.1% to 11.4% and the remaining
confusions are a flat tail with no dominant cause: ী/ি, ট/ত, ন/ণ, শ/স. Those are genuine
homophones, which positional context cannot resolve by definition.

## What the remaining failures actually are

It is easy to read a 11% failure rate as 11% worth of bugs. Classifying every
in-dictionary failure says otherwise. For each one, the benchmark checks exhaustively
whether *any* combination of candidates composes to the target word:

| Cause | Count | Is it a defect? |
|---|---|---|
| Lost to a **higher-frequency** dictionary word | 193 | **No** |
| Target not reachable by any combination | 21 | Yes — rule-table gap |
| Lost to an equal-frequency word | 1 | No — a coin toss |
| Reachable but no tier selected it | 2 | Yes |

**87% of failures are not bugs.** Both spellings are real Bengali words, the Roman input is
genuinely ambiguous between them, and the resolver picked the commoner one. Without
sentence context that is the correct choice — it is right more often than any alternative
policy. Removing those "failures" would require knowing what the user meant, which is a
language-model problem, not a resolution bug.

The 21 unreachable targets were real defects, and they had three distinct causes:

- **Word-final hasant.** বাহ্, আল্লাহ্ and 62 other corpus words end in an explicit hasant,
  and none of them could be typed at all: every phonetic rule emits a consonant, and
  `UnicodeComposer` only ever inserts a hasant *between* two of them. Avro's key for this is
  `,,` but the hook only captures A–Z into the buffer, so punctuation never reaches the
  engine. Added to the special-character picker, which exists for exactly this class.
- **Malformed corpus entries.** 39 words wrote আ as অ + া — the commonest corruption in
  Bengali corpora, since it renders almost identically and survives most pipelines. A
  dictionary entry the engine can never produce is worse than a missing one: it can never
  match, and it can still beat the correct spelling on frequency. Repaired, and 89 further
  entries that were still malformed were dropped rather than guessed at. A test now rejects
  the whole class.
- **Convention artefacts**, where the word needs a capital `O` and the lazy-typist model
  types lowercase. Not defects — see `docs/TYPING_CONVENTION.md`.

## A fix that measurement said not to make

`ii` looks like exactly the same bug as `ri`: maximal munch takes it as ঈ, so ি+ই is
unspellable, and দিই comes out as দী. The obvious move is to remove the token as before.

The corpus says no. ঈ and ী appear in words totalling 45,728 occurrences; ি+ই in 3,389.
Removing the token would be wrong about thirteen times for every time it was right — the
exact inverse of the `ri` case, where র+ি beat ৃ four to one.

Two identical-looking bugs, opposite correct answers, and no way to tell them apart by
reasoning. This is the argument for measuring, in one example.

## Closing the generalisation gap

Two tiers were added below whole-word lookup, both measured on the same held-out split.
The margin for the character model was tuned on words ranked 6000–8000 and reported on
words ranked 2000–4000, so the number below is not fitted to itself.

| Resolver on held-out words | Correct | Accuracy |
|---|---|---|
| whole-word lookup only | 1004 / 1922 | 52.2% |
| + morphological stem lookup | 1067 / 1922 | **55.5%** |
| + character trigram model (margin 4) | 1084 / 1922 | **56.4%** |

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
