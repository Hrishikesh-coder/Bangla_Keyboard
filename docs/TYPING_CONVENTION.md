# Typing Convention — a decision the team needs to make

**Status: proposed, not agreed. Settle this before the demo, not during it.**

This is the one user-visible choice in the engine that cannot be derived from first
principles. It changes what everyone types, so it needs a decision from the group rather
than from whoever last edited the rule file.

## The problem

Every Bengali consonant carries an inherent vowel — an unwritten *ô*. ক on its own is
already "ko". Nothing in the script marks it, which means a Roman input scheme has to
decide how the typist indicates *absence* of a vowel versus presence of the inherent one.

Two sequences that must end up different:

| Intended | Why it is hard |
|---|---|
| কল (kôl) | ক and ল each carry an inherent vowel; nothing is written between them |
| ক্ল (kla) | ক and ল are welded by a hasant; again nothing visible marks the difference in Roman |

## What is implemented now

- **Lowercase `o` is the inherent vowel.** It produces no glyph, but it *does* break the
  consonant cluster.
  `kol` → কল, `rong` → রং, `uttor` → উত্তর, `boi` → বই
- **Omitting the vowel entirely stacks a conjunct.**
  `kl` → ক্ল, `kolm` → কল্ম
- **Capital `O` forces an explicit ও / ো.**
  `sOnar` → সোনার, `bhalO` → ভালো
- **Capitals otherwise select the retroflex or alternate letter.**
  `T`=ট `D`=ড `N`=ণ `S`=শ `Sh`=ষ `R`=ড় `Rh`=ঢ়

Mechanically this works because `UnicodeComposer` already treats অ specially: after a
consonant it emits nothing but clears the "previous was a consonant" flag. An empty-string
candidate cannot do the same job — it emits nothing *and leaves the flag set*, so the next
consonant would still fuse.

## The alternative

Avro's own rule file defaults `o` to ো after a consonant, so `kol` → কোল and the inherent
vowel is typed by omitting the letter (`kl` → কল, which collides with the conjunct case and
is resolved by context in a way we do not implement).

## Why the current choice

Counting real words, the inherent reading wins clearly:

| Typed | Inherent-`o` (implemented) | Explicit-`o` (alternative) | Correct |
|---|---|---|---|
| `kol` | কল | কোল | কল |
| `rong` | রং | রোং | রং |
| `uttor` | উত্তর | উত্তোর | উত্তর |
| `boi` | বই | বোই | বই |
| `sonar` | সনার | সোনার | সোনার |

Four out of five. The one case the alternative gets right, `sonar`, is reachable as `sOnar`
and is in the exception dictionary anyway.

It also matches what Hrishikesh's original README documented (`kol` → কল), so the
convention is not a new invention — it is the one the project started with.

## What has to happen

1. **Everyone types the tagline once.** `ami banglay gan gai` → আমি বাংলায় গান গাই. If
   anyone's muscle memory fights it, better to find out now.
2. **Agree or overrule.** If the group prefers Avro's default, the change is four lines in
   `config/phonetic_rules.json` — `"o"` gets a context variant with `"ো"` first — plus
   updating the tests that pin `kol`, `rong`, `uttor` and `boi`. No C++ changes.
3. **Whatever is decided, say it in the report.** The ambiguity is real and inherent to
   phonetic input; showing that it was identified and decided deliberately is worth more
   than pretending it did not exist.

## Fallbacks that exist either way

- `Ctrl+Shift+Space` cycles the alternatives for the ambiguous letter.
- Clicking a chip in the overlay picks one directly.
- The exception dictionary overrides whole words the rules cannot get right.
