# Shobdomala (শব্দমালা)
### Minimal Windows System-Wide Bengali Phonetic Input Prototype

An educational System Programming prototype demonstrating how to intercept, process, transliterate, and inject Unicode text globally across Windows applications using the Win32 API and modern C++17.

---

## Table of Contents
1. [Overview](#overview)
2. [What is a Keyboard Hook?](#what-is-a-keyboard-hook)
3. [Where the Windows Keyboard Driver Fits](#where-the-windows-keyboard-driver-fits)
4. [Architecture](#architecture)
5. [Class Breakdown](#class-breakdown)
6. [Complete Input Flow Pipeline](#complete-input-flow-pipeline)
7. [Phonetic Tokenization (Longest-Match-First)](#7-phonetic-tokenization-longest-match-first)
   - [7a. Contextual Resolution (the second pass)](#7a-contextual-resolution-the-second-pass)
   - [7b. Exception Dictionary](#7b-exception-dictionary)
   - [7c. Fixed Layout Mode](#7c-fixed-layout-mode)
   - [7d. Live In-Place Preview](#7d-live-in-place-preview)
   - [7e. The Interface](#7e-the-interface)
   - [7f. Word Prediction and Autocorrect](#7f-word-prediction-and-autocorrect)
8. [Configurable Symbol Table & JSON](#configurable-symbol-table--json)
9. [Unicode Composition & Bengali Conjuncts](#unicode-composition--bengali-conjuncts)
10. [Special Character Picker (ৎ ং ঃ ঁ ঞ)](#special-character-picker-ৎ-ং-ঃ-ঁ-ঞ)
11. [How `SendInput` and Loop Prevention Work](#how-sendinput-and-loop-prevention-work)
12. [Win32 APIs Explained](#win32-apis-explained)
13. [Building and Running](#building-and-running)
14. [Running Unit Tests](#running-unit-tests)
15. [Known Limitations](#known-limitations)

---

## 1. Overview

**Shobdomala** is an educational, modular prototype of a system-wide input method editor (IME) for Windows. It allows users to type phonetic Roman English characters (e.g. `shanti`, `amar`, `kol`) and transliterates them into Bengali Unicode characters (`শান্তি`, `আমার`, `কল`) in any focused Windows application (Notepad, web browsers, code editors, word processors, etc.).

### Key Design Principles
- **Educational Simplicity**: Clean, straightforward C++17 code without bloated frameworks, complex state machines, or third-party GUI dependencies.
- **Configurable**: Phonetic rules are defined in an editable `config/phonetic_rules.json` file.
- **No Over-Engineering**: Deliberately omits machine learning, heavy dictionary lookups, contextual morphology, and auto-correct.

---

## 2. What is a Keyboard Hook?

In the Microsoft Windows operating system, a **hook** (`HHOOK`) is a mechanism by which an application can intercept events (messages, mouse clicks, keystrokes) before they reach the target application window.

A **low-level keyboard hook** (`WH_KEYBOARD_LL`) is installed using the Win32 function `SetWindowsHookExW`. Unlike ordinary thread-specific hooks, `WH_KEYBOARD_LL`:
1. Installs globally across all running desktop threads without requiring a separate DLL to be injected into target processes.
2. Intercepts raw keyboard events directly from the OS input stream before they are posted to any target application's message queue.
3. Allows the hook procedure to inspect key codes, evaluate modifier states, and either:
   - **Pass the event along** by returning `CallNextHookEx(...)`.
   - **Consume / suppress the event** by returning `1` (preventing the key from reaching the active application).

---

## 3. Where the Windows Keyboard Driver Fits

Understanding the Windows input architecture explains how physical keystrokes reach our hook and target applications:

```
[ Physical Keyboard Hardware ]
            │
            ▼ (Electrical interrupt / USB HID packet)
[ Keyboard Port Driver (i8042prt.sys / kbdhid.sys) ]
            │ (Raw scancodes)
            ▼
[ Keyboard Class Driver (kbdclass.sys) ]
            │
            ▼ (Kernel-to-User space transition)
[ Windows Subsystem Kernel: win32k.sys (Raw Input Thread - RIT) ]
            │
            ▼ (Creates low-level keyboard packet)
    ┌───────────────────────────────────────────────┐
    │  WH_KEYBOARD_LL Hook Chain (Shobdomala Hook)  │
    └──────────────────────┬────────────────────────┘
                           │
       ┌───────────────────┴───────────────────┐
       ▼ (if consumed: return 1)               ▼ (if passed: CallNextHookEx)
 [ Suppressed from OS ]             [ Thread Message Queue of Active Window ]
                                               │
                                               ▼
                                    [ Target App WindowProc ]
                                     (WM_KEYDOWN / WM_CHAR)
```

1. **Hardware & Port Drivers (`kbdhid.sys`)**: Detect physical key depressions, generating hardware scancodes.
2. **Keyboard Class Driver (`kbdclass.sys`)**: Collects scancodes and presents them to the OS.
3. **Windows Graphics and Subsystem (`win32k.sys`)**: The Raw Input Thread (RIT) converts hardware input into virtual key codes and delivers them to the global `WH_KEYBOARD_LL` hook chain.
4. **Shobdomala's Hook**: Intercepts the event. If in Bengali mode, Roman alphabetic keys are captured into an `InputBuffer` and suppressed (`return 1`).
5. **Input Injection (`SendInput`)**: When a word delimiter (space, enter, punctuation) is pressed, Shobdomala synthesizes Unicode characters directly into the RIT.

---

## 4. Architecture

The codebase is split strictly into a platform-independent **Core** transliteration engine and a platform-specific **Native** Win32 layer:

```
Shobdomala/
├── CMakeLists.txt              # Standard CMake build definition
├── Makefile                    # MinGW GCC / Make build file
├── build.bat                   # Direct one-click Windows build script
├── README.md                   # This documentation
├── config/
│   ├── phonetic_rules.json     # User-editable Roman -> Bengali rules, with context variants
│   ├── exceptions.json         # Whole-word overrides + English passthrough list
│   ├── layout_probhat.json     # Verified Probhat layout (base / shift / AltGr)
│   └── words_bangla.json       # Starter word-frequency list for prediction
├── docs/
│   ├── CONTEXTUAL_ENGINE.md    # Why the engine needs two passes (report material)
│   ├── INTERFACE.md            # UI design rationale
│   ├── TYPING_CONVENTION.md    # The inherent-vowel decision, for the team
│   ├── MANUAL_TEST_PLAN.md     # 50 checks to run on real Windows hardware
│   └── LIMITATIONS.md          # Known boundaries, for the report
├── include/
│   ├── core/
│   │   ├── Candidate.h            # Roman token + candidate Bengali strings
│   │   ├── CandidateResolver.h    # Abstract selection strategy (extension point)
│   │   ├── ContextAnalyzer.h      # Pass 2: annotates tokens with their surroundings
│   │   ├── ExceptionDictionary.h  # Whole-word overrides
│   │   ├── FixedLayoutEngine.h    # Stateless key -> glyph layout engine
│   │   ├── InputBuffer.h          # Manages in-progress Roman keystrokes
│   │   ├── PhoneticEngine.h       # High-level pipeline coordinator
│   │   ├── SpecialCharPicker.h    # Selection menu for ৎ, ং, ঃ, ঁ, ঞ
│   │   ├── SymbolTable.h          # Rule storage, contextual lookup, trie index
│   │   ├── TokenContext.h         # TokenClass + context bit flags
│   │   ├── Tokenizer.h            # Longest-match-first tokenizer
│   │   ├── TokenTrie.h            # Prefix tree backing the tokenizer
│   │   └── UnicodeComposer.h      # Bengali virama & matra Unicode composition
│   └── native/
│       ├── InputInjector.h     # Win32 SendInput Unicode synthesizer
│       ├── KeyboardHook.h      # WH_KEYBOARD_LL hook & message pump
│       └── KeyboardState.h     # InputMode enum & centralized shortcuts
├── src/
│   ├── core/
│   │   ├── CandidateResolver.cpp
│   │   ├── ContextAnalyzer.cpp
│   │   ├── ExceptionDictionary.cpp
│   │   ├── FixedLayoutEngine.cpp
│   │   ├── InputBuffer.cpp
│   │   ├── PhoneticEngine.cpp
│   │   ├── SpecialCharPicker.cpp
│   │   ├── SymbolTable.cpp
│   │   ├── TokenContext.cpp
│   │   ├── Tokenizer.cpp
│   │   ├── TokenTrie.cpp
│   │   └── UnicodeComposer.cpp
│   ├── native/
│   │   ├── InputInjector.cpp
│   │   ├── KeyboardHook.cpp
│   │   └── KeyboardState.cpp
│   ├── ui/
│   │   ├── UiTheme.cpp
│   │   ├── CandidateWindow.cpp
│   │   ├── OnScreenKeyboard.cpp
│   │   └── TrayIcon.cpp
│   └── main.cpp                # CLI entry point, hook lifecycle, and demo mode
├── tests/
│   ├── test_helpers.h          # Lightweight test macros (zero dependencies)
│   └── test_main.cpp           # Unit test suite covering all modules
└── third_party/
    └── nlohmann/
        └── json.hpp            # Single-header JSON parser (MIT)
```

---

## 5. Class Breakdown

### Core Module (`include/core/` and `src/core/`)

1. **`Candidate`**:
   - Represents a phonetic Roman token and its available Bengali Unicode candidate options.
   - Example: `"sh"` has options `["শ", "ষ", "স"]`.
   - Includes `cycleNext()` to cycle candidates on demand.
   - An empty string (`""`) represents an **epsilon** (no-output) candidate.

2. **`ICandidateResolver` / `DefaultCandidateResolver`**:
   - Provides an abstract interface for picking candidate indices.
   - Baseline implementation always chooses index `0`.
   - Serves as an educational extension point: future students can plug in a dictionary, frequency list, or ML model without touching the hook or tokenizer.

3. **`SymbolTable`**:
   - Stores mappings from Roman strings to vectors of Bengali strings using `std::unordered_map`.
   - Reads rules from `config/phonetic_rules.json`.
   - Pre-sorts tokens by length in descending order for rapid longest-match-first matching.

4. **`Tokenizer`**:
   - Implements **longest-match-first** tokenization.
   - Example: Given `"shanti"`, matches `"sh"` before `"s"`, yielding `["sh", "a", "n", "t", "i"]`.
   - Uses the `SymbolTable` to know valid tokens; does not hardcode rules.

5. **`UnicodeComposer`**:
   - Operates purely on Unicode code points (`char32_t`).
   - Automatically inserts Bengali **virama / hasant** (`্`, U+09CD) between adjacent consonants to form conjuncts (e.g. `ক` + `ষ` -> `ক্ষ`).
   - Converts independent vowels to dependent vowel signs (matras/kar) when following consonants (e.g. `ক` + `ই` -> `কি`).
   - Handles inherent vowel `অ` correctly without creating invalid characters.

6. **`InputBuffer`**:
   - Accumulates typed Roman characters while Bengali mode is active.
   - Handles `append`, `backspace`, and `clear`.

7. **`SpecialCharPicker`**:
   - Handles the dedicated shortcut (`Ctrl + Shift + D`) for the 5 special Bengali characters:
     - `1`: `ৎ` (Khanda Ta)
     - `2`: `ং` (Anusvara)
     - `3`: `ঃ` (Visarga)
     - `4`: `ঁ` (Chandrabindu)
     - `5`: `ঞ` (Nya)
   - Implements a simple two-state machine (`INACTIVE` / `AWAITING_DIGIT`).

8. **`PhoneticEngine`**:
   - Orchestrates the full pipeline: `InputBuffer` -> `Tokenizer` -> `SymbolTable` -> `CandidateResolver` -> `UnicodeComposer`.
   - Manages candidate cycling on the in-progress word.

**`TokenTrie`**:

- Prefix tree over every registered Roman token, backing the tokenizer's longest-match walk.
- Replaces a per-position substring scan; tokenization is O(n) instead of O(n x maxTokenLength).

**`TokenContext`** (`TokenClass`, `Ctx` flags):

- Classifies a token as vowel / consonant / modifier / punctuation, declared in JSON or inferred from its first candidate.
- Defines the context bit flags (`word_start`, `after_consonant`, `before_vowel`, ...) that contextual rules are written against.

**`ContextAnalyzer`**:

- The second pass. Annotates each token with a context bitmask computed purely from neighbouring token *classes*, before any Bengali codepoint is chosen.
- This is what lets one token produce different letters in different positions — see [7a](#7a-contextual-resolution-the-second-pass).

**`ExceptionDictionary`**:

- Whole-word overrides consulted before the rule pipeline: irregular spellings (`dhonnobad` -> ধন্যবাদ) and English words that map to themselves so they pass through untouched.

**`FixedLayoutEngine`**:

- Stateless key -> glyph mapping for fixed Bengali layouts. No buffer, no candidates, no composition; the typist presses the hasant key to build conjuncts.

### Native Module (`include/native/` and `src/native/`)

1. **`KeyboardState`**:
   - Maintains the active `InputMode` (`ENGLISH` or `BENGALI`).
   - Centralizes all keyboard shortcuts in one single place.

2. **`KeyboardHook`**:
   - Installs and manages the `WH_KEYBOARD_LL` hook via `SetWindowsHookExW`.
   - Runs the mandatory Win32 message loop (`GetMessageW`).
   - Dispatches keystrokes to buffer, shortcuts, or passes them through.

3. **`InputInjector`**:
   - Converts UTF-8 strings to UTF-16 using `MultiByteToWideChar`.
   - Injects Unicode characters into the focused application using Win32 `SendInput` with `KEYEVENTF_UNICODE`.

---

## 6. Complete Input Flow Pipeline

```
  [ User presses physical keys: s -> h -> a -> n -> t -> i -> SPACE ]
                                  │
                                  ▼
             [ LowLevelKeyboardProc (KeyboardHook.cpp) ]
                                  │
          ┌───────────────────────┴────────────────────────┐
          │                                                │
(kbd->flags & LLKHF_INJECTED)?                       Normal Key
          │                                                │
          ▼ YES                                            ▼
   CallNextHookEx (Pass)                   Check Keyboard Shortcuts:
   (covers our own Bengali output          - Ctrl+Shift+B -> Toggle English/Bengali
    AND our preview backspaces)            - Ctrl+Shift+L -> Cycle input mode
                                           - Ctrl+Shift+Space -> Cycle Candidate
                                           - Ctrl+Shift+D -> Special Menu
                                           - Ctrl+Shift+P -> Toggle live preview
                                                           │
                                                           ▼
                                               Which InputMode?
                          ┌────────────────────────────────┼────────────────────────┐
                          ▼                                ▼                        ▼
                     ENGLISH                        BENGALI_FIXED           BENGALI_PHONETIC
                CallNextHookEx (pass)        1. ToUnicodeEx -> ASCII key             │
                                             2. FixedLayoutEngine::mapKey            │
                                             3. Inject glyph, return 1               │
                                                (stateless: no buffer)               │
                                                                                     │
             ┌───────────────────────────────────────────────────────────────────────┤
             ▼                                                                       ▼
   Letter key A-Z:                                                    Space / Enter / Punct:
   1. ToUnicodeEx -> char (Shift + Caps correct; case matters: T=ট t=ত)  1. commitBuffer()
   2. InputBuffer::append                                               2. Exception dictionary
   3. PhoneticEngine::updateActiveBuffer                                    may correct the
        ├─ Tokenizer        (pass 1: trie, maximal munch)                    preview in place
        ├─ ContextAnalyzer  (pass 2: context bitmask per token)           3. Clear buffer + preview
        ├─ SymbolTable      (contextual candidate lookup)                 4. Pass the delimiter
        ├─ CandidateResolver(index within that list)                         through untouched
        └─ UnicodeComposer  (viramas, matras, inherent vowel)
   4. refreshPreview():
        InputInjector::replaceText(previousUnits, composed)
        = N backspaces + new text, in ONE SendInput batch
   5. Return 1 (suppress the Roman key from the active window)
```

**Live preview vs. flush-on-delimiter.** The path above renders the word into the target
application on every keystroke. With `Ctrl + Shift + P` (or `--no-preview`) nothing is
injected until a delimiter is typed, which is the original prototype's behaviour and the
safe fallback in applications that move the caret underneath us — see
[7d](#7d-live-in-place-preview).

---

## 7. Phonetic Tokenization (Longest-Match-First)

Why longest-match-first is needed:
In Roman Bengali input, a sequence like `"shanti"` must be tokenized as:
`["sh", "a", "n", "t", "i"]`
rather than:
`["s", "h", "a", "n", "t", "i"]`.

### Algorithm
```
pos = 0
while pos < input.length:
    matched = false
    for len = min(maxTokenLength, remainingLength) down to 1:
        substring = input.substr(pos, len)
        if symbolTable.has(substring):
            tokens.push_back(substring)
            pos += len
            matched = true
            break
    if not matched:
        tokens.push_back(input[pos])
        pos += 1
```

By querying the `SymbolTable` from `maxTokenLength` down to `1`, tokens like `"chh"`, `"sh"`, `"kh"` are greedily matched before single-letter prefixes like `"c"`, `"s"`, `"k"`.

---

## 7a. Contextual Resolution (the second pass)

Longest-match tokenization alone is not enough. A phonetic token's correct Bengali output
depends on what surrounds it, and the original pipeline had nowhere to put that knowledge:
`DefaultCandidateResolver` always returned index 0, so every rule behaved identically
everywhere in a word. The result, measured against this README's own examples:

| Input | Before | After |
|---|---|---|
| `shanti` | শন্তি | **শান্তি** |
| `amar` | অমর | **আমার** |
| `bangla` | বঙ্ল | **বাংলা** |
| `gai` | গৈ | **গাই** |
| `rong` | রোং | **রং** |

Three kinds of context dependence cause this:

1. **Independent vowel vs. matra** — আ stands alone but attaches as া. `gai` must be গ + া + ই, not গৈ.
2. **Letters that change identity by position** — `ng` is ঙ between vowels (রঙিন) but ং before a consonant or at a word end (বাংলা, রং).
3. **The inherent vowel** — every consonant carries an unwritten ô. `kol` is কল: the `o` emits no glyph, yet it must still break the cluster or ক and ল fuse into ক্ল.

### Why this needs a separate pass

When the tokenizer emits `a` in `bangla`, it has not yet looked at what follows. Deciding
`a`'s output requires tokens that have not been scanned. This is exactly the forward
reference that forces an assembler into two passes — `JMP LOOP` cannot be emitted until
`LOOP` is known — and it is resolved the same way: **separate recognition from resolution.**

| Two-pass assembler | Shobdomala |
|---|---|
| Pass 1: scan source, build symbol table, record references | Pass 1: `Tokenizer` — longest-match-first segmentation |
| Pass 2: resolve references against the complete table, emit code | Pass 2: `ContextAnalyzer` -> contextual lookup -> `UnicodeComposer`, emit Unicode |

`ContextAnalyzer` annotates every token with a bitmask drawn from `word_start`, `word_end`,
`after_consonant`, `after_vowel`, `after_modifier`, `before_consonant`, `before_vowel` and
`before_modifier`. It works purely on **token classes**, never on Bengali output, so it runs
before a single codepoint has been chosen — a genuine second pass, not a fixup afterwards.

### Contextual rules are data

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

Variants are tested in declaration order; the first whose `when` flags are **all** satisfied
wins. Adding a rule never requires touching C++. The older flat forms (`"kh": "খ"` and
`"sh": ["শ", "ষ", "স"]`) still load unchanged, and a rule that omits `class` has it inferred
from its first candidate.

### The inherent-vowel marker

`UnicodeComposer` already treated অ specially: after a consonant it emits nothing but clears
the "previous was a consonant" flag. The rule set now uses that deliberately —
`"o": { "class": "vowel", "candidates": ["অ", "ও"] }` — which is why `rong` gives রং,
`kolm` gives কল্ম, and `kl` (no vowel typed at all) gives ক্ল. An empty-string epsilon
candidate cannot do this job: it emits nothing *and leaves the consonant flag set*, so the
next consonant would still fuse.

Typing conventions that follow: lowercase `o` is the inherent vowel (`kol` -> কল), capital
`O` forces the explicit ও/ো (`sOnar` -> সোনার), and capitals otherwise select the retroflex
series (`T`=ট `D`=ড `N`=ণ `S`=শ `Sh`=ষ `R`=ড়).

Full write-up in [`docs/CONTEXTUAL_ENGINE.md`](docs/CONTEXTUAL_ENGINE.md).

### Tokenizer: trie instead of substring scan

The original tokenizer tried every substring length from `maxTokenLength` down to 1 at each
position, constructing and hashing up to `maxTokenLength` temporary strings per character.
`TokenTrie` walks the input once and allocates nothing, making tokenization O(n) rather than
O(n x maxTokenLength). This is not premature optimisation: the hook re-tokenizes the entire
in-progress word on **every keystroke**, inside a callback Windows silently unhooks if it
runs too long. `test_trie_matches_bruteforce_scan` asserts the trie produces byte-identical
tokenization to the original scan, so the change is proven behaviour-preserving.

---

## 7b. Exception Dictionary

Rule-based transliteration is systematic; Bengali spelling is not. `config/exceptions.json`
holds whole-word overrides checked **before** the rule pipeline runs:

```json
{
  "words": {
    "dhonnobad": "ধন্যবাদ",
    "download": "download"
  }
}
```

Two uses. Words whose written form contains something nobody types phonetically —
`dhonnobad` is pronounced with a doubled ন but written ধন্যবাদ with a য-fola, which no
phonetic rule can derive. And English words that must survive untouched: they map to
themselves, which is how the engine learns to leave them alone. Lookup is exact first, then
case-insensitive.

---

## 7c. Fixed Layout Mode

Phonetic input guesses; a fixed layout does not. `FixedLayoutEngine` implements the other
way Bengali is actually typed: one key, one glyph, with the typist pressing the hasant key
themselves to build conjuncts. It is completely stateless — no buffer, no candidates, no
composition pass — which is exactly why professional Bengali typists prefer fixed layouts.

Press `Ctrl + Shift + L` to cycle into it. The map lives in `config/layout_probhat.json`, so
a different layout, or one a user designs, is a data edit rather than a code change.

The shipped map is **verified**: transcribed key-for-key from the canonical X11 definition
`xkb_symbols "ben_probhat"` (`/usr/share/X11/xkb/symbols/in`), the Probhat that Linux
distributions ship, derived from the ankurbangla.org scheme. An earlier draft scored 72% —
the letter rows were right, the Z row, most punctuation and the whole AltGr level were not.
`test_shipped_layout_matches_probhat` pins the keys that were wrong.

Probhat is three levels deep: base, Shift, and **AltGr**, which holds the nukta, ৗ, ঽ, the
rupee sign and the currency-fraction signs. All three are implemented. Windows has no AltGr
virtual key — the driver synthesises left-Ctrl plus right-Alt — so it is detected as
right-Alt being down.

---

## 7d. Live In-Place Preview

By default the in-progress word is now rendered **into the target application as it is
typed**, the way Avro behaves: each keystroke erases the previous rendering with backspaces
and injects the updated one, batched into a single `SendInput` call so the two halves cannot
be interleaved with real keystrokes.

`InputInjector::replaceText(previousUnits, newText)` handles this. The unit of measurement is
UTF-16 code units, because that is what backspace operates on in Windows edit controls — not
bytes, and not codepoints.

The live path assumes the caret has not moved since the word began. If the user clicks
elsewhere, presses an arrow key, or the application rewrites the field underneath (an
autocomplete box, a terminal, a spreadsheet cell editor), backspaces would delete the wrong
text. Two defences: any non-delimiter key commits the current word first, and
`Ctrl + Shift + P` (or `--no-preview` at startup) falls back to the original
flush-on-delimiter behaviour.

---

## 7e. The Interface

A system-wide IME has no main window and no business owning one. Three surfaces replace the
prototype's console-only feedback, in descending order of how often the user sees them.

### The composition overlay

A dark card that appears at the caret while a word is in progress:

```
 ● PHONETIC   shanti          <- mode pip + exactly what was captured
 শান্তি                        <- the composition, at reading size
 ───────────────────────────
 [ শ ]  [ ষ ]  [ স ]          <- alternatives; the chosen one carries the accent
```

This closes known limitation #1. Previously the preview went to a console, so seeing what
the engine thought you meant required looking away from the sentence you were writing.

The raw Roman buffer is shown because when the output is wrong, the user needs to know
whether they mistyped or the engine misread. The alternatives are shown because cycling with
`Ctrl+Shift+Space` was otherwise blind — you watched text change with no idea how many
options existed. Chips are **clickable**, which is only possible because `WS_EX_NOACTIVATE`
means the overlay never takes focus, so a click leaves the target application's caret alone.

The caret position comes from `GetGUIThreadInfo` on the **foreground** thread. A low-level
hook runs on our thread, which has no caret; `GetCaretPos` would report ours.

### The tray icon

The icon is drawn at runtime rather than shipped as a `.ico`, so it scales to whatever
`SM_CXSMICON` reports and carries mode in **both** shape and colour: `A` on grey for English,
`অ` on blue for phonetic, `ক` on green for fixed layout. Left click toggles English/Bengali;
right click opens the full menu, where modes are radio items because only one can be active.

### The on-screen keyboard

Its obvious job is input. Its real job is teaching: every key cap shows the Bengali glyph
large and the Latin key that produces it small underneath, so using the mouse gradually makes
the board unnecessary. It is built from `FixedLayoutEngine`'s key map, so editing
`config/layout_probhat.json` changes what is drawn — a layout and its keyboard cannot drift
apart when there is one source of truth.

### Visual language

All tokens live in `include/ui/UiTheme.h`, named by role rather than hue so a light theme
would be one file's change. The accent colour appears in exactly one place in the whole
interface — the selected candidate — so that while cycling, the eye has one thing to track.

Bengali renders in Nirmala UI, which does the conjunct shaping and matra reordering;
`UnicodeComposer` emits a logically correct codepoint sequence, but turning ক + ্ + ষ into
ক্ষ is the font's job. Every measurement is a 96-DPI design unit passed through `scale()`,
and both windows handle `WM_DPICHANGED`.

Full rationale, including the Win32 details that make each surface behave:
[`docs/INTERFACE.md`](docs/INTERFACE.md).

---

## 7f. Word Prediction and Autocorrect

`TokenTrie` indexes Roman **rule tokens** and answers "which longer rules begin with `kh`".
`WordDictionary` indexes Bengali **words** and answers "which words begin with বাং". Two
different questions, two different structures.

The dictionary is keyed by **Unicode codepoint**, not by UTF-8 byte. Every Bengali letter
is three bytes, so a byte-wise edit distance would score a one-letter typo as three edits
and could assemble broken sequences mid-word.

**Prediction** walks the prefix and ranks the subtree by frequency.

**Correction** carries a Levenshtein DP row down the trie and abandons a subtree the moment
the smallest value in its row exceeds the threshold — no descendant can score better, since
extra letters only add to the distance. Comparing against every word separately would be
O(words × length) per keystroke, which is not affordable inside a hook callback that Windows
will silently unhook for running long. It only runs when prefix prediction finds nothing and
at least three letters have been composed; below that, almost everything is within two edits
of almost everything else.

Predictions appear as a second chip row in the overlay, deliberately **not** accented: the
accent marks the selected *candidate*, and giving an offer the same colour as a current
selection would obscure which one `Ctrl+Shift+Space` is acting on. Clicking one replaces the
**entire word** and commits it.

`config/words_bangla.json` is a 197-word starter list with hand-assigned weights, labelled
as such in the file. Any `{"word": count}` map or bare array loads unchanged.

---

## 8. Configurable Symbol Table & JSON

Rules are stored in `config/phonetic_rules.json`:

```json
{
  "a": ["অ", "আ", ""],
  "aa": ["আ"],
  "i": ["ই", "ঈ"],
  "sh": ["শ", "ষ", "স"],
  "k": ["ক"],
  "kh": ["খ"],
  "t": ["ত", "ট"],
  "th": ["থ", "ঠ"]
}
```

- **One-to-Many Mapping**: Any token can specify multiple Bengali characters ordered by likelihood.
- **Epsilon Candidate**: The empty string `""` represents epsilon (optional vowels like trailing 'a' or 'o' in Sanskrit/Bengali loanwords).
- **Zero Recompilation**: Students can add new tokens or customize their personal transliteration preferences simply by editing this JSON file.

---

## 9. Unicode Composition & Bengali Conjuncts

In Bengali typography and the Unicode standard:
1. **Virama / Hasanta (`্`, U+09CD)**:
   When two consonants are adjacent without an intervening vowel, they form a conjunct (যুক্তাক্ষর).
   In Unicode, this is represented by:
   `Consonant 1` + `U+09CD (্)` + `Consonant 2`.
   - `ক` + `্` + `ষ` -> `ক্ষ` (`k` + `sh`)
   - `ন` + `্` + `ত` -> `ন্ত` (`n` + `t`)
   - `স` + `্` + `ত` + `্` + `র` -> `স্ত্র` (`s` + `t` + `r`)

2. **Dependent Vowel Signs (Matras / কার)**:
   - When an independent vowel follows a consonant, `UnicodeComposer` converts it to its dependent form:
     - `আ` -> `া` (U+0BE)
     - `ই` -> `ি` (U+0BF)
     - `ঈ` -> `ী` (U+0C0)
     - `উ` -> `ু` (U+0C1)
     - `ঊ` -> `ূ` (U+0C2)
     - `ঋ` -> `ৃ` (U+0C3)
     - `এ` -> `ে` (U+0C7)
     - `ঐ` -> `ৈ` (U+0C8)
     - `ও` -> `ো` (U+0CB)
     - `ঔ` -> `ৌ` (U+0CC)
   - When an independent vowel occurs at the start of a word or after another vowel, it remains independent (e.g. `আমার`).

3. **Inherent Vowel `অ`**:
   Bengali consonants already carry an inherent `ô`/`a` sound. When candidate `অ` follows a consonant, no visual matra is added, but it fulfills the syllable vowel so subsequent consonants do not form an erroneous conjunct with it (e.g. `kal` -> `কল`, not `ক্ল`).

---

## 10. Special Character Picker (ৎ ং ঃ ঁ ঞ)

Characters like Khanda Ta and Chandrabindu do not map cleanly to standard Roman letters. Shobdomala provides direct access via a keyboard shortcut:

1. Press **`Ctrl + Shift + D`** while in Bengali mode.
2. The console displays the menu:
   ```
   [SPECIAL CHARS] [1] ৎ (Khanda Ta)  [2] ং (Anusvara)  [3] ঃ (Visarga)  [4] ঁ (Chandrabindu)  [5] ঞ (Nya)
   [PICKER] Press 1-5 to insert, or any other key to cancel.
   ```
3. Pressing digits `1` to `5` immediately injects the corresponding Bengali character into the active window. Pressing any other key cancels the picker cleanly.

---

## 11. How `SendInput` and Loop Prevention Work

### The Infinite Loop Problem
When Shobdomala captures Roman characters, it calls `SendInput` to inject the transliterated Bengali Unicode characters. Because `SetWindowsHookEx` intercepts *all* system keystrokes, Shobdomala's own injected characters would normally trigger the hook callback again. Without prevention, this would cause infinite recursion, crashing the application or locking the keyboard.

### The Solution: `LLKHF_INJECTED`
Windows automatically tags every event generated via `SendInput` with the `LLKHF_INJECTED` (0x00000001) flag in `KBDLLHOOKSTRUCT.flags`.

In `KeyboardHook::hookCallback`:
```cpp
auto* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

// Step 1: Prevent recursion from our own injected events
if (kbd->flags & LLKHF_INJECTED) {
    return CallNextHookEx(s_hHook, nCode, wParam, lParam);
}
```
If this flag is detected, the callback immediately passes the event through with `CallNextHookEx` and returns, preventing re-processing.

---

## 12. Win32 APIs Explained

Every non-obvious Windows API used in this project is documented below:

### 1. `SetWindowsHookExW`
- **What it does**: Installs an application-defined hook procedure (`LowLevelKeyboardProc`) into the global hook chain.
- **Why we use it**: To intercept keystrokes globally across all applications before they are processed by target windows.
- **Parameters**:
  - `idHook`: `WH_KEYBOARD_LL` (13) indicates a low-level keyboard hook.
  - `lpfn`: Pointer to our static callback function (`hookCallback`).
  - `hMod`: Handle to the module (`GetModuleHandleW(nullptr)`).
  - `dwThreadId`: `0` indicates the hook applies globally to all threads on the desktop.
- **Return Value**: An `HHOOK` handle to the installed hook, or `NULL` on failure.

### 2. `UnhookWindowsHookEx`
- **What it does**: Uninstalls a hook procedure previously installed by `SetWindowsHookEx`.
- **Why we use it**: Crucial for clean shutdown; prevents leaving dangling callback pointers in the OS kernel.
- **Parameters**: `hhk`: The `HHOOK` handle returned by `SetWindowsHookExW`.
- **Return Value**: Non-zero (`TRUE`) if successful, `0` (`FALSE`) on failure.

### 3. `CallNextHookEx`
- **What it does**: Passes the hook notification to the next hook procedure in the chain.
- **Why we use it**: Whenever we do not want to suppress an event (e.g. English mode, non-alphabetic keys, or injected events), we must allow other hooks and target apps to receive it.
- **Parameters**: `hhk` (ignored in modern Windows), `nCode`, `wParam`, `lParam`.
- **Return Value**: The `LRESULT` returned by the next hook procedure.

### 4. `GetMessageW`, `TranslateMessage`, `DispatchMessageW`
- **What it does**: Runs the Win32 message pump for the calling thread.
- **Why we use it**: Windows requires the thread that installed `WH_KEYBOARD_LL` to actively pump messages. If the thread does not pump messages, Windows considers the hook timed out and silently removes it.
- **Return Value**: `GetMessageW` returns `>0` for normal messages, `0` for `WM_QUIT`, and `-1` on error.

### 5. `SendInput`
- **What it does**: Synthesizes keyboard events into the global input stream.
- **Why we use it**: To insert Bengali Unicode characters into whichever window has active focus.
- **Parameters**:
  - `cInputs`: Number of structures in the `pInputs` array.
  - `pInputs`: Array of `INPUT` structures configured with `type = INPUT_KEYBOARD` and `dwFlags = KEYEVENTF_UNICODE`.
  - `cbSize`: `sizeof(INPUT)`.
- **Return Value**: Number of events successfully inserted into the input stream.

### 6. `MultiByteToWideChar`
- **What it does**: Converts a UTF-8 character string into a UTF-16 wide-character (`wchar_t`) string.
- **Why we use it**: The core transliteration engine works in standard UTF-8, while `SendInput`'s `KEYEVENTF_UNICODE` requires 16-bit UTF-16 code units.
- **Return Value**: Number of `wchar_t` units written to the buffer.

### 7. `GetAsyncKeyState`
- **What it does**: Queries the real-time physical up/down state of a virtual key.
- **Why we use it**: To verify if `Ctrl`, `Shift`, or `Alt` are physically held down when processing shortcut key combinations.
- **Return Value**: A 16-bit `SHORT`; if the high-order bit (`0x8000`) is set, the key is physically pressed.

---

## 13. Building and Running

### Prerequisites
- Windows 10 or Windows 11
- A C++17 compatible compiler:
  - **MinGW-w64 (GCC 9+)** or
  - **Visual Studio 2019/2022 (MSVC)** or
  - **Clang on Windows**
- (Optional) CMake 3.15+

### Build Option A: Direct Batch Script (Recommended for Quick Start)
From the project root directory, run:
```cmd
.\build.bat
```
This compiles `bin\shobdomala.exe` and `bin\shobdomala_tests.exe` and executes the unit tests.

### Build Option B: Using Make / MinGW
```cmd
mingw32-make
```

### Build Option C: Using CMake
```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

---

## 14. Running and Using Shobdomala

### 1. Offline Transliteration Demo (No Windows Hook Required)
If you want to test the phonetic engine and tokenization without installing the system-wide keyboard hook:
```cmd
.\bin\shobdomala.exe --test
```
You can type Roman words and inspect results:
```
shobdomala> shanti
Result: শান্তি
shobdomala> :tokens shanti
Tokens for "shanti": [ "sh", "a", "n", "t", "i" ]
shobdomala> :q
```

### 2. Live System-Wide Mode
Run the main executable:
```cmd
.\bin\shobdomala.exe
```

#### Keyboard Shortcuts:
| Shortcut | Action | Description |
|---|---|---|
| **`Ctrl + Shift + B`** | **Toggle Input Mode** | Switches between `ENGLISH` and `BENGALI` (phonetic). |
| **`Ctrl + Shift + L`** | **Cycle Input Mode** | `ENGLISH` -> phonetic -> fixed layout -> `ENGLISH`. |
| **`Ctrl + Shift + Space`** | **Cycle Candidate** | Cycles the alternative candidate for ambiguous tokens. |
| **`Ctrl + Shift + D`** | **Special Bengali Menu** | Opens the menu for `ৎ`, `ং`, `ঃ`, `ঁ`, `ঞ` (press `1`-`5`). |
| **`Ctrl + Shift + P`** | **Toggle Live Preview** | Live in-place rendering vs. flush-on-delimiter. |
| **`Ctrl + C`** | **Exit** | Uninstalls the keyboard hook cleanly and exits. |

#### Example Live Typing Workflow:
1. Start `shobdomala.exe`.
2. Open **Notepad** (or any text field).
3. Press **`Ctrl + Shift + B`** to switch to Bengali mode (console prints `[MODE TOGGLE] Input mode switched to: BENGALI`).
4. Type: `amar sonar bangla` followed by Space.
5. Watch the Bengali text appear in Notepad!
6. Press `Ctrl + Shift + B` again to return to normal English typing.

---

## 15. Running Unit Tests

Run the standalone unit test suite:
```cmd
.\bin\shobdomala_tests.exe
```

Output:
```
========================================
   Running Shobdomala Unit Tests
========================================
[RUN ] test_symbol_table_lookup ... PASSED
[RUN ] test_longest_match_tokenization ... PASSED
[RUN ] test_candidate_selection_and_cycling ... PASSED
[RUN ] test_unicode_composition_simple ... PASSED
[RUN ] test_virama_conjunct_sequence ... PASSED
[RUN ] test_epsilon_candidate ... PASSED
[RUN ] test_special_char_picker ... PASSED
[RUN ] test_end_to_end_transliteration ... PASSED
[RUN ] test_retaining_cycled_candidate_across_typing ... PASSED
[RUN ] test_token_trie_longest_match ... PASSED
[RUN ] test_trie_matches_bruteforce_scan ... PASSED
[RUN ] test_context_analyzer_flags ... PASSED
[RUN ] test_contextual_rule_selection ... PASSED
[RUN ] test_legacy_rule_format_still_loads ... PASSED
[RUN ] test_exception_dictionary ... PASSED
[RUN ] test_exception_overrides_rules ... PASSED
[RUN ] test_shipped_rules_load ... PASSED
[RUN ] test_readme_examples ... PASSED
[RUN ] test_inherent_vowel_handling ... PASSED
[RUN ] test_case_sensitive_retroflex_tokens ... PASSED
[RUN ] test_unicode_conformance_sequences ... PASSED
[RUN ] test_candidate_cycling_end_to_end ... PASSED
[RUN ] test_direct_candidate_selection ... PASSED
[RUN ] test_punctuation_and_unknown_passthrough ... PASSED
[RUN ] test_fixed_layout_engine ... PASSED
[RUN ] test_shipped_layout_is_complete ... PASSED

----------------------------------------
Results: 40/40 passed
========================================
```

---

## 16. Known Limitations

Full discussion, with the reasoning behind each boundary, in
[`docs/LIMITATIONS.md`](docs/LIMITATIONS.md). In brief:

1. **The overlay assumes a stationary caret.** Live preview edits with backspaces, so an application that moves the caret underneath us can swallow them. Any non-delimiter key commits defensively; `Ctrl+Shift+P` falls back to flush-on-delimiter. The real fix is Windows TSF, which is beyond a term project.
2. **Context resolves position, not meaning.** ঙ/ং and matra/independent-vowel are positional and handled. শ/ষ/স, ন/ণ and ই/ঈ are homophones, distinguished by which word it is — handled by candidate cycling, the exception dictionary and word prediction, with `ICandidateResolver` left as the extension point for a frequency or n-gram model.
3. **UIPI blocks injection into elevated windows.** Run elevated to type into them. Not defaulting to elevation is deliberate: a program that reads every keystroke should hold the least privilege that works.
4. **The word list is a 197-word starter**, not a corpus. Labelled as such in the file; any `{"word": count}` source loads unchanged.
5. **No overlay in fixed-layout mode.** Fixed typing is stateless, so there is nothing in progress to preview — correct, but it makes the modes feel different.
6. **No layout editor.** The on-screen keyboard renders any layout file; editing caps in place is the unimplemented next step.
7. **Shaping is delegated to DirectWrite.** The composer emits logically correct codepoints; ligature formation and pre-base matra reordering are the font's job. That is the intended architecture, not an omission.
8. **Untested on real hardware.** Compile-verified with 40 automated engine tests; the Windows UI has never been run. [`docs/MANUAL_TEST_PLAN.md`](docs/MANUAL_TEST_PLAN.md) is the checklist for closing that gap.
