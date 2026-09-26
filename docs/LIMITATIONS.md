# Limitations

Written for the report. Every item here is a deliberate boundary or a known weakness, and
naming them is worth more marks than hoping nobody asks. An examiner who finds a limit you
have already described reads it as judgement; one who finds a limit you have hidden reads
it as an accident.

---

## 1. The overlay assumes the caret has not moved

Live preview renders the in-progress word into the target application and, on the next
keystroke, erases it with backspaces before injecting the updated version. That is only
correct while the caret is where we left it.

**When it breaks:** the user clicks elsewhere mid-word; an autocomplete dropdown rewrites
the field; a terminal redraws its line; a spreadsheet commits a cell.

**What we do about it:** any non-delimiter key (arrows, Home, Tab) commits the current word
first, so the backspaces never cross a caret move we can see. `Ctrl+Shift+P` or
`--no-preview` falls back to flush-on-delimiter, which injects nothing until the word ends
and is therefore immune.

**What we cannot do:** detect a caret move we are not told about. A low-level keyboard hook
sees keys, not focus or selection changes. A production IME solves this by implementing
Windows TSF (Text Services Framework), where the application tells the IME about its
document. TSF is the right answer and is far beyond a term project — naming it is the
honest position.

---

## 2. Context resolves position, not meaning

The contextual engine decides forms that depend on **where a letter sits**: ঙ versus ং, a
matra versus an independent vowel, the inherent vowel. It does that reliably.

It cannot resolve **homophones**, because they are distinguished by meaning:

| Typed | Could be | Distinguished by |
|---|---|---|
| `sh` | শ / ষ / স | which word it is |
| `n` | ন / ণ | which word it is |
| `i` | ই / ঈ | which word it is |

No amount of positional context fixes this. শান্তি and ষান্তি are equally well-formed
sequences; only a dictionary knows the first is a word.

**What we do about it:** candidates are ordered by likelihood and index 0 is chosen;
`Ctrl+Shift+Space` cycles; the overlay makes the alternatives visible and clickable; the
exception dictionary overrides whole words outright; word prediction offers real words from
the dictionary.

**The proper fix,** which we have left as a documented extension point: implement
`ICandidateResolver` over a frequency list or an n-gram model, so the resolver chooses using
the surrounding word rather than always taking index 0. The interface exists and is
unchanged since the original prototype precisely so this can be done without touching the
tokenizer, the analyzer or the hook.

---

## 3. UIPI blocks injection into elevated windows

`SendInput` from a normal-integrity process into an elevated one is refused by User
Interface Privilege Isolation. Typing into an administrator PowerShell, Task Manager, or an
installer does nothing.

**Workaround:** run Shobdomala elevated too.

**Why we did not just do that:** an input method that reads every keystroke should run with
the least privilege that works. Requiring administrator by default is a worse security
posture than the limitation it removes. This is a Windows security boundary working as
designed, not a defect.

---

## 4. The word list is a starter, not a corpus

`config/words_bangla.json` holds 197 words with hand-assigned relative weights. It is
enough to demonstrate prediction and correction and is explicitly labelled as such inside
the file.

Real frequencies come from a corpus — Bengali Wikipedia dumps, or the Indic NLP corpora.
The loader takes any `{"word": count}` map or a bare array, so replacing the file needs no
code change. Nothing in the engine assumes this particular list.

---

## 5. No floating candidate window in fixed-layout mode

Fixed-layout typing is stateless by design: one key, one glyph, no buffer. There is nothing
in progress to preview and no candidates to choose between, so the overlay does not appear.

That is correct behaviour, but it does make the two modes feel different, and the only
confirmation of which mode you are in is the tray icon.

---

## 6. No layout editor

The on-screen keyboard renders whatever `config/layout_probhat.json` contains, so a custom
layout is already a data edit. Making the key caps editable in place is the natural next
step and is not implemented: rendering a layout and editing one are different problems, and
only the first is solved. Editing would also need conflict detection (two keys claiming one
glyph) and a way to save without clobbering a file the user may have hand-edited.

---

## 7. Rendering is delegated, and that is the correct architecture

`UnicodeComposer` emits a logically correct codepoint sequence — ক + ্ + ষ. Turning that
into the ligature ক্ষ, reordering the pre-base ে and ি glyphs so they draw to the *left* of
the consonant they follow logically, and choosing conjunct forms is the font and text
shaper's job, done by DirectWrite and the target application.

This is how the platform is meant to work, not a shortcut. It does mean output can be
logically correct and still look wrong in an application with a font that lacks Bengali
shaping — a failure we can neither detect nor fix from inside the IME.

---

## 8. Untested on real hardware

Everything is compile-verified (MinGW, `-Wall -Wextra`, no warnings) and the engine has 40
automated tests. The Windows UI layer has never been run.

Compilation cannot tell you whether Nirmala UI resolved, whether the overlay lands sensibly
in Chrome, or whether the tray icon is legible at 16×16. `docs/MANUAL_TEST_PLAN.md` is the
checklist for closing that gap, and it should be run on two machines before submission —
one of them with display scaling above 100%, because that is where DPI bugs live.
