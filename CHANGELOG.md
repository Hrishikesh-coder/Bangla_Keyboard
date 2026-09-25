# Changelog

## feature/contextual-engine

Builds on the initial Shobdomala prototype. The architecture is unchanged — core/native
split, `ICandidateResolver` extension point, JSON-configured rules — and every public
interface from the original is still present and still works.

### The bug this branch exists to fix

The prototype's own README examples did not hold:

| Input | Before | After |
|---|---|---|
| `shanti` | শন্তি | **শান্তি** |
| `amar` | অমর | **আমার** |
| `bangla` | বঙ্ল | **বাংলা** |
| `gai` | গৈ | **গাই** |
| `kol` | কোল | **কল** |
| `rong` | রোং | **রং** |

One root cause. `DefaultCandidateResolver` always returns index 0 and `SymbolTable` returned
one fixed candidate list per token, so no rule could ever behave differently depending on
where it appeared in a word. `ICandidateResolver` was designed as the extension point for
exactly this and was never used.

### Added

**Contextual resolution — a genuine second pass**
- `TokenContext.h` — `TokenClass` (vowel / consonant / modifier / punctuation) and the
  `Ctx` context flags: `word_start`, `word_end`, `after_consonant`, `after_vowel`,
  `after_modifier`, `before_consonant`, `before_vowel`, `before_modifier`.
- `ContextAnalyzer` — annotates every token with a context bitmask computed purely from
  neighbouring token *classes*, so it runs before a single Bengali codepoint has been
  chosen.
- `SymbolTable` — now stores a `TokenRule` per token: class, default candidates, and an
  ordered list of `ContextVariant` overrides. `lookupContextual(token, context)` returns the
  list that applies. Classes are inferred from the candidates when the JSON omits them.
- Rationale and the assembler parallel: `docs/CONTEXTUAL_ENGINE.md`.

**Exception dictionary** (`ExceptionDictionary`, `config/exceptions.json`)
- Whole-word overrides checked before the rule pipeline: irregular spellings
  (`dhonnobad` → ধন্যবাদ) and English words that map to themselves so they pass through
  untouched (`download`, `email`, `file`).
- Exact lookup first, then case-insensitive.

**Fixed layout mode** (`FixedLayoutEngine`, `config/layout_probhat.json`)
- Stateless key → glyph mapping: one key, one glyph, conjuncts built by pressing the hasant
  key. Reachable with `Ctrl + Shift + L`.
- The shipped map is a **draft**: complete and internally consistent (asserted by
  `test_shipped_layout_is_complete`) but not verified key-for-key against the official
  Probhat chart.

**Live in-place preview**
- The in-progress word now renders into the target application as it is typed. Each
  keystroke erases the previous rendering and injects the updated one.
- `InputInjector::replaceText(previousUnits, text)` batches the backspaces and the
  replacement into a single `SendInput` call, so they cannot be interleaved with real
  keystrokes. `InputInjector::utf16UnitCount` measures in UTF-16 code units, which is what
  backspace operates on in Windows edit controls.
- `Ctrl + Shift + P` or `--no-preview` restores the original flush-on-delimiter behaviour.
  Any non-delimiter key commits the current word defensively, because the live path assumes
  the caret has not moved.

**A user interface** (`include/ui/`, `src/ui/`)
- `CandidateWindow` — a dark card that follows the text caret while a word is in progress,
  showing the mode, the raw Roman buffer, the composed Bengali at reading size, and clickable
  chips for the alternatives. Closes known limitation #1: the preview used to go to a console.
  `WS_EX_NOACTIVATE` keeps focus in the target application; the caret comes from
  `GetGUIThreadInfo` on the *foreground* thread, since a low-level hook's own thread has no
  caret.
- `TrayIcon` — mode at a glance and mode control without a shortcut. The icon is drawn at
  runtime, so it scales with `SM_CXSMICON` and carries state in both shape and colour
  (`A`/grey, `অ`/blue, `ক`/green). Re-adds itself on `TaskbarCreated`, or it would vanish
  whenever Explorer restarts.
- `OnScreenKeyboard` — clickable keys built from `FixedLayoutEngine`'s map, each cap showing
  the Bengali glyph with the Latin key underneath, so mouse use teaches the layout. Latching
  one-shot Shift; the whole board is a drag handle via `WM_NCHITTEST`.
- `UiTheme` — design tokens named by role, DPI scaling, font selection with verification
  (Windows substitutes faces silently), and drawing helpers. The accent colour is used in
  exactly one place — the selected candidate.
- `PhoneticEngine::activeAmbiguousTokenIndex/activeCandidateOptions/activeCandidateSelection/
  setActiveSelection` — so a clicked chip can select an option directly instead of cycling.
- `KeyboardState::setOnChanged` — mode changes from the hook and from the tray both announce,
  so the icon can never disagree with the engine.
- Rationale: `docs/INTERFACE.md`.

**Three input modes**
- `InputMode::BENGALI_PHONETIC` and `InputMode::BENGALI_FIXED`; `InputMode::BENGALI` is kept
  as an alias for the phonetic mode so existing code compiles.
- `Ctrl + Shift + B` keeps its original meaning (English ↔ phonetic);
  `Ctrl + Shift + L` cycles all three.

**Trie tokenizer** (`TokenTrie`)
- Replaces the per-position "try every substring length" scan, which constructed and hashed
  up to `maxTokenLength` temporary strings per character. Tokenization is now O(n) and
  allocation-free during the walk. This matters because the hook re-tokenizes the whole
  in-progress word on every keystroke, inside a callback Windows unhooks if it runs too long.

**CLI**
- `:explain <word>` — tokens, classes, contexts and the candidate chosen for each.
- `:layout <keys>` — map keys through the fixed layout.
- `:rules` — now reports contextual rule and exception counts.
- Demo mode transliterates whole lines, so the whole-word exception dictionary can fire on
  each word of a sentence.

### Changed

- **`config/phonetic_rules.json` rewritten.** 87 tokens (was 42), with contextual variants
  for `ng`, `y`, `w`, `ai`, `oi`, `ou`, `au`. Adds the case-sensitive retroflex series
  (`T`=ট `D`=ড `N`=ণ `S`=শ `Sh`=ষ `R`=ড় `Rh`=ঢ়), pre-composed `kkh`=ক্ষ and `x`=ক্স,
  য-fola and ব-fola.
- **Typing convention.** Lowercase `o` is now the inherent vowel — `kol` → কল, `rong` → রং —
  which is why `অ` appears as a candidate: the composer emits nothing for it after a
  consonant but does clear the consonant state, so `kolm` → কল্ম while `kl` → ক্ল. Capital
  `O` forces the explicit ও/ো (`sOnar` → সোনার). **Agree this with the team before the demo.**
- **Key translation** uses `ToUnicodeEx` with flag `0x4` (do not modify keyboard state) and
  a manually built modifier array, instead of assuming a US layout and ignoring Caps Lock.
  The flag is required: calling `ToUnicodeEx` from inside a low-level hook without it
  corrupts dead-key sequences desktop-wide.
- **Build files** — `Makefile`, `CMakeLists.txt` and `build.bat` pick up the new sources and
  config files. The engine and its tests now build on Linux/macOS too (the native layer stays
  Win32-only), so the transliteration engine can be tested in CI without a Windows machine.
  `tests/test_main.cpp` no longer includes `windows.h` unconditionally.
- **README** — updated directory tree, class breakdown, input-flow diagram, shortcut table,
  test output and known limitations; new sections 7a–7d.

### Compatibility

- Rule files written for the original prototype still load: `"kh": "খ"` and
  `"sh": ["শ", "ষ", "স"]` are both accepted, verified by
  `test_legacy_rule_format_still_loads`.
- All 9 pre-existing tests, including `test_retaining_cycled_candidate_across_typing`, still pass unmodified.
- `SymbolTable::lookup`, `getTokensSortedByLengthDesc`, `getMaxTokenLength`,
  `Tokenizer::tokenize`, `PhoneticEngine::transliterate` and `KeyboardHook::install` keep
  their existing signatures and behaviour.

### Interaction with candidate retention (05e0eed)

This branch is rebased on top of Antariksh's candidate-retention and odometer cycling work,
and the two compose without changes to either:

- `updateActiveBuffer` retains a selection only when the token *and its options list* match
  the previous keystroke. Contextual variants change the options list when a token's
  context changes, so the guard correctly drops a stale selection instead of carrying it
  onto a different candidate set.
- On commit, `KeyboardHook::finalTextFor()` checks the exception dictionary first and
  otherwise composes from the active candidate list via `flushActive()`. Calling
  `transliterate()` there would rebuild from index 0 and silently discard whatever the user
  picked with `Ctrl+Shift+Space`.

### Tests

26 passing, up from 9. The 17 added by this branch cover the trie (including a byte-for-byte equivalence
check against a reference implementation of the original substring scan), context flag
computation, contextual rule selection, legacy rule loading, the exception dictionary,
inherent-vowel handling, case-sensitive tokens, candidate cycling end to end, punctuation
passthrough, the fixed layout, direct candidate selection, and Unicode conformance.

Conformance tests assert exact codepoint sequences rather than comparing rendered strings,
because Bengali has several ways to look correct and be wrong:

```
বাংলা == U+09AC U+09BE U+0982 U+09B2 U+09BE
শান্তি == U+09B6 U+09BE U+09A8 U+09CD U+09A4 U+09BF
বই    == U+09AC U+0987        (two independent letters, NOT ব + ি)
```
