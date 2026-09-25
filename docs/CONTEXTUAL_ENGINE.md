# Contextual Resolution: Why the Engine Needs Two Passes

This document explains the pass added between tokenization and candidate selection, why it
was necessary, and how it connects to the one-pass / two-pass assembler theme running
through the rest of the assignment set.

## 1. The bug that motivated it

The original prototype ran this pipeline:

```
Roman input -> Tokenizer -> SymbolTable -> CandidateResolver -> UnicodeComposer -> Bengali
```

`DefaultCandidateResolver` always returns index 0, and `SymbolTable` returned one fixed
candidate list per token. So every rule behaved identically everywhere in a word. Measured
against the project's own README examples:

| Input | Produced | Expected |
|---|---|---|
| `shanti` | শন্তি | শান্তি |
| `amar` | অমর | আমার |
| `bangla` | বঙ্ল | বাংলা |
| `gai` | গৈ | গাই |
| `rong` | রোং | রং |

These are not five separate bugs. They are one bug with five faces: **a phonetic token's
correct Bengali output depends on what surrounds it, and the pipeline had nowhere to put
that knowledge.**

## 2. Three kinds of context dependence in Bengali

**Independent vowel vs. matra.** The vowel আ is written আ standing alone and া attached to a
consonant. `UnicodeComposer` already handled this conversion, but it was being fed অ (the
inherent vowel, which has no matra) instead of আ, so every `a` silently vanished. `amar`
became অমর.

**Letters that change identity by position.** `ng` is ঙ between vowels (রঙিন) but ং before a
consonant or at the end of a word (বাংলা, রং). One token, two letters, decided entirely by
the neighbours.

**The inherent vowel.** Every Bengali consonant carries an inherent ô that is never written.
So `kol` is কল: the `o` produces no glyph at all — but it still has to break the consonant
cluster, or ক and ল would fuse into ক্ল. The inherent vowel is simultaneously invisible in
the output and load-bearing in the composition.

## 3. Why one pass cannot do it

At the moment the tokenizer emits `a` in `bangla`, it does not yet know a consonant precedes
it and a consonant follows it — the tokenizer's whole job is to scan left to right and
commit to the longest match. Deciding `a`'s output requires looking at tokens that have not
been examined yet.

This is exactly the forward-reference problem that forces an assembler into two passes. A
one-pass assembler cannot emit the instruction `JMP LOOP` when `LOOP` is defined further
down the file; it must either backpatch or make a second pass. Here, the engine cannot emit
the glyph for `ng` until it knows what follows `ng`.

The resolution is the same: **separate recognition from resolution.**

| Assembler | Shobdomala |
|---|---|
| Pass 1: scan source, build the symbol table, record where each reference occurs | Pass 1: `Tokenizer` — longest-match-first segmentation into tokens |
| Pass 2: walk the recorded references, resolve each against the now-complete symbol table, emit machine code | Pass 2: `ContextAnalyzer` — annotate each token with its surroundings, select the candidate list, emit Unicode |

## 4. The pipeline as it now stands

```
Roman input
  -> ExceptionDictionary   whole-word override? then done
  -> Tokenizer             PASS 1: maximal munch over a trie
  -> ContextAnalyzer       PASS 2a: annotate every token with a context bitmask
  -> SymbolTable           PASS 2b: pick the candidate list for that context
  -> CandidateResolver     PASS 2c: pick an index within the list
  -> UnicodeComposer       PASS 2d: viramas, matras, inherent vowel
  -> Bengali Unicode
```

`ContextAnalyzer` computes, for each token, a bitmask drawn from:

```
word_start        word_end
after_consonant   after_vowel     after_modifier
before_consonant  before_vowel    before_modifier
```

Crucially, the analyzer works purely on **token classes** (vowel / consonant / modifier),
never on Bengali output. That means it runs before a single codepoint has been chosen, which
is what makes it a genuine separate pass rather than a peephole fixup afterwards.

## 5. Rules are data, not code

A contextual rule lives in `config/phonetic_rules.json`:

```json
"ng": {
  "class": "consonant",
  "candidates": ["ঙ", "ং"],
  "context": [
    { "when": ["before_consonant"], "candidates": ["ং", "ঙ"] },
    { "when": ["word_end"],         "candidates": ["ং", "ঙ"] }
  ]
}
```

Variants are tested in declaration order; the first whose `when` flags are **all** present in
the token's computed mask wins. So the file reads top-to-bottom as "most specific case
first", and adding a rule never requires touching C++.

The older flat forms still load unchanged:

```json
"kh": "খ",
"sh": ["শ", "ষ", "স"]
```

When a rule omits `class`, `SymbolTable::deriveClass` infers it from the first Bengali
codepoint of the first non-empty candidate, so legacy rule files keep working.

## 6. The inherent-vowel trick

`UnicodeComposer` already had a special case: the codepoint অ following a consonant emits
nothing but clears the "previous was a consonant" flag. The rule set now uses অ deliberately
as an **inherent-vowel marker**:

```json
"o": { "class": "vowel", "candidates": ["অ", "ও"] }
```

Consequences, all of them desirable:

- `rong` → র + (nothing) + ং = রং, not রোং
- `kolm` → ক + (nothing) + ল + ্ + ম = কল্ম, not ক্লম — the marker broke the first cluster and left the second intact
- `kl`, with no `o` at all, → ক্ল

This is why `অ` works where the empty-string epsilon candidate does not: epsilon produces no
output *and leaves the consonant flag set*, so the next consonant would still fuse.

## 7. Where ambiguity genuinely remains

Bengali phonetic input is ambiguous by nature: শ/ষ/স are homophones, and so are ন/ণ and
ই/ঈ. Context narrows the choice; it cannot eliminate it. Three mechanisms handle the rest:

1. **Candidate ordering** — index 0 is the most likely reading, chosen by the resolver.
2. **Candidate cycling** — `Ctrl+Shift+Space` walks the alternatives for the current word.
3. **Exception dictionary** — whole-word overrides for spellings no rule can derive
   (`dhonnobad` → ধন্যবাদ) and for English words that must survive untouched
   (`download` → `download`).

`ICandidateResolver` remains the documented extension point for anything smarter: a
frequency list, a dictionary, or an n-gram model can replace `DefaultCandidateResolver`
without touching the tokenizer, the analyzer or the hook.

## 8. Verification

`tests/test_main.cpp` asserts exact Unicode codepoint sequences rather than comparing
rendered strings, because Bengali has several ways to look correct and be wrong — a
standalone ো instead of a real matra, a missing virama, an independent vowel where a kar
belongs. For example:

```
বাংলা == U+09AC U+09BE U+0982 U+09B2 U+09BE
শান্তি == U+09B6 U+09BE U+09A8 U+09CD U+09A4 U+09BF
বই     == U+09AC U+0987          (two independent letters, NOT ব + ি)
```

`test_trie_matches_bruteforce_scan` additionally checks the new trie tokenizer against a
reference implementation of the original substring scan, so the performance change is proven
not to have altered behaviour.
