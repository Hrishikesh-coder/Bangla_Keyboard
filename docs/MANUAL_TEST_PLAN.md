# Manual Test Plan

**Why this document exists:** every line of this project is compile-verified — MinGW cross
build with `-Wall -Wextra`, 40 automated tests covering the engine. None of that has been
run on Windows. Compilation cannot tell you whether a font fell back, whether the overlay
lands somewhere sensible in Chrome, or whether the tray icon is legible at 16×16. Those
need a human at a real machine.

Run this on **two machines** if at all possible. One should be a laptop with scaling above
100%, because that is where DPI bugs live.

Record the result of each numbered check. A check that cannot be performed (no second
monitor, say) should be marked as not-run rather than passed.

---

## A. Build

| # | Check | Expected |
|---|---|---|
| A1 | `build.bat` from a clean clone | Both binaries built, `Results: 40/40 passed` |
| A2 | `bin\shobdomala.exe` exists and runs | Tray icon appears, no console window |
| A3 | Launch from `cmd` | Engine trace appears **in that window**, not a new one |
| A4 | `shobdomala.exe --test` | Interactive REPL, `:q` exits |
| A5 | CMake build (`cmake .. && cmake --build .`) | Same binary name, tests runnable via `ctest` |

A3 and A4 are the ones to watch: the app is GUI-subsystem, so both depend on `ConsoleHost`
working. If A4 shows a message box about needing a console, that path is broken.

## B. Tray icon

| # | Check | Expected |
|---|---|---|
| B1 | Icon at 100% scaling | Grey disc, letter **A** legible |
| B2 | Icon at 150%/200% scaling | Still sharp, not a resampled blur |
| B3 | Left click | Toggles English ↔ Bengali, icon turns blue **অ** |
| B4 | Right click | Menu opens; modes are **radio** items, only one ticked |
| B5 | Switch to fixed layout | Icon turns green **ক** |
| B6 | Restart Explorer (Task Manager → Restart) | Icon **reappears** |
| B7 | Dark vs light taskbar | Disc edge is clean, no grey box behind it |

B6 is the `TaskbarCreated` handler. B7 is the DIB alpha pass — if the glyph looks like a
hole punched through the disc, that is the bug.

## C. Composition overlay

Test in **Notepad first** (simplest), then Word, Chrome's address bar, and VS Code.

| # | Check | Expected |
|---|---|---|
| C1 | Type `ami` in Notepad | Card appears **at the caret**, Bengali forms live |
| C2 | Card contents | Mode pip, Roman buffer, composition, candidate chips |
| C3 | Bengali renders as letters, not boxes | Nirmala UI resolved |
| C4 | Conjunct check: type `shanti` | শান্তি — ন্ত is a **single ligature**, not ন ্ ত |
| C5 | Click a candidate chip | Letter changes; **focus stays in Notepad** |
| C6 | `Ctrl+Shift+Space` | Cycles the highlighted candidate |
| C7 | Type near the bottom of the screen | Card **flips above** the caret instead of clipping |
| C8 | Type in a maximised window on a second monitor | Card appears on the **right** monitor |
| C9 | Backspace mid-word | Composition shrinks correctly |
| C10 | Press an arrow key mid-word | Word commits; no stray backspaces |

C5 is the `WS_EX_NOACTIVATE` behaviour. C10 is the defensive commit — if text gets eaten
there, live preview is unsafe and `Ctrl+Shift+P` is the workaround.

## D. Engine behaviour

| # | Type this | Expected |
|---|---|---|
| D1 | `ami banglay gan gai` | আমি বাংলায় গান গাই |
| D2 | `shanti` | শান্তি |
| D3 | `kol` | কল (see `docs/TYPING_CONVENTION.md`) |
| D4 | `kl` | ক্ল |
| D5 | `sOnar` | সোনার |
| D6 | `dhonnobad` | ধন্যবাদ — preview must **not** flicker through ধ্ন্য্বাদ |
| D7 | `download` | download, untouched |
| D8 | `T` `D` `N` `Sh` | ট ড ণ ষ |

D6 is the double-composition regression. If it appears mid-word and corrects on space, the
fix did not take.

## E. Word prediction

| # | Check | Expected |
|---|---|---|
| E1 | Type `bang` | Second chip row offers বাংলা, বাংলাদেশ |
| E2 | Click a prediction | **Whole word** replaces the composition and commits |
| E3 | Type `bangla` slowly, watch the row at every keystroke | Never once says **did you mean** — completions or nothing |
| E4 | Type `banla` then **space** | Overlay stays open: **did you mean** বাংলা |
| E5 | Click that correction | বান্লা replaced by বাংলা, the space preserved |
| E6 | Type `banla`, space, then keep typing | Offer disappears, no text disturbed |
| E7 | Type a correct word then space | **No** offer at all |
| E8 | Type `banla` then **Enter** | No offer (a newline cannot be safely retyped) |
| E9 | Type a name not in the list, then space | No offer rather than a wrong guess |

## F. Fixed layout

| # | Check | Expected |
|---|---|---|
| F1 | `Ctrl+Shift+L` to fixed mode | Icon green; typing is one key → one glyph |
| F2 | `k` `/` `S` | ক্ষ (hasant is on `/`) |
| F3 | `z` `x` `Z` | য় শ য |
| F4 | `AltGr + .` | ় (nukta) |
| F5 | `AltGr + r` | ₹ |
| F6 | Type `2026` | ২০২৬ |

F4/F5 are the AltGr level. If AltGr does nothing, check whether the keyboard driver
actually produces right-Alt (some non-US layouts differ).

## G. On-screen keyboard

| # | Check | Expected |
|---|---|---|
| G1 | Tray menu → On-screen keyboard | Board appears, bottom-centre |
| G2 | Each cap | Bengali glyph large, Latin key small underneath |
| G3 | Click a key | Glyph inserted into the **focused app**, board keeps no focus |
| G4 | Shift, then a key | Shifted glyph; Shift releases automatically |
| G5 | AltGr, then `.` | ় inserted |
| G6 | Drag the board by its header | Moves; no title bar needed |

## H. Persistence

| # | Check | Expected |
|---|---|---|
| H1 | Set fixed mode, turn preview off, move the board, exit via tray | — |
| H2 | Relaunch | Same mode, preview still off, board in the same place |
| H3 | Check `%APPDATA%\Shobdomala\settings.json` | Readable JSON |
| H4 | Corrupt that file deliberately, relaunch | Starts with defaults, does not crash |
| H5 | Kill via Task Manager, relaunch | Settings still preserved (written on change) |

## I. Known-risk areas

Not bugs, but the places to look first if something is odd:

- **Elevated windows.** UIPI blocks `SendInput` from a normal process into an elevated one.
  Typing into an admin PowerShell needs Shobdomala running elevated too.
- **Terminals and autocomplete boxes.** Live preview edits with backspaces; an application
  that rewrites its own field can swallow them. `Ctrl+Shift+P` falls back to flush-on-space.
- **Non-US keyboard drivers.** Key translation goes through `ToUnicodeEx` with the active
  layout, but has only been reasoned about, not tested, on a non-US layout.

---

## Reporting

For each failure record: check number, machine, Windows version, display scaling, the app
under test, what happened, and a screenshot if it is visual. Most of the untested risk here
is visual, and a screenshot settles in one second what a paragraph cannot.
