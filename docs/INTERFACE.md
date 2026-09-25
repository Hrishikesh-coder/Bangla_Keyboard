# The Interface

Shobdomala is a system-wide IME, so it has no main window and no business owning one. The
user's attention belongs to whatever they are actually writing in. Everything below follows
from that constraint.

## 1. What was wrong with console-only

The prototype printed its state to a console:

```
[BUFFER] "shan" -> শান
[FLUSH] "shanti" ==> "শান্তি"
```

Informative, and useless while typing. To see what the engine thought you meant you had to
look away from the sentence you were writing, find a console window, and look back. For an
input method that is a fundamental failure: the whole point is to stay in the flow of
writing. The project's own README listed "no in-place floating preview window" as known
limitation number one.

Three surfaces replace it, in descending order of how often the user sees them.

## 2. The composition overlay

A dark card that appears at the caret while a word is in progress, showing three things:

```
 ● PHONETIC   shanti          <- mode pip + exactly what was captured
 শান্তি                        <- the composition, at reading size
 ───────────────────────────
 [ শ ]  [ ষ ]  [ স ]          <- alternatives; the chosen one carries the accent
```

**Why show the raw Roman buffer.** When the output is wrong, the user needs to know whether
they mistyped or the engine misread. The buffer answers that instantly and costs one line.

**Why the alternatives are visible.** Cycling with `Ctrl+Shift+Space` was previously blind —
you pressed it and watched the text change, with no idea how many options existed or whether
you had gone past the one you wanted. Showing the set turns a guess into a choice.

**Why one accent colour.** `ACCENT` is used in exactly one place in the entire interface:
the selected candidate. While cycling, the eye has one thing to track. An interface that
accents five things accents nothing.

**Why chips are clickable.** Cycling is fine for two options and tedious for five. Clicking
is possible here only because of `WS_EX_NOACTIVATE` — the overlay never takes focus, so a
click reaches it as an ordinary `WM_LBUTTONDOWN` while the target application keeps its
caret and its selection untouched.

### Three Win32 details that make it behave

| Detail | Why |
|---|---|
| `WS_EX_NOACTIVATE` | Never steals focus. Without it the target application loses its caret the moment the overlay appears, and live preview's backspaces would go to the wrong window. |
| `WS_EX_TOOLWINDOW` | Keeps it out of the taskbar and out of Alt+Tab. It is furniture, not a window the user manages. |
| `GetGUIThreadInfo` on the **foreground** thread | A low-level hook runs on *our* thread, which has no caret. `GetCaretPos` would report ours, not the user's. This asks the focused thread where its caret actually is. |

It falls back to the foreground window's top-left, then to the cursor, because many web
views and custom editors report no caret at all. Landing in roughly the right place beats
landing at (0, 0).

## 3. The tray icon

Mode has to be visible without the user asking, and changeable without a keyboard shortcut
they might not remember.

The icon is **drawn at runtime** rather than shipped as a `.ico`. That buys two things: it
scales to whatever `SM_CXSMICON` reports on a high-DPI display instead of being resampled,
and mode is carried by **both** shape and colour — `A` on grey for English, `অ` on blue for
phonetic, `ক` on green for fixed layout. A colour-blind user reads the letter; everyone
else reads the colour at a glance. Relying on colour alone at 16×16 would have failed both.

Left click toggles English/Bengali, because that is the action taken hundreds of times a
day. Everything else is one level deeper, in the right-click menu. Modes are **radio items**,
not checkboxes — that is not cosmetic, it tells the user only one can be active.

One detail that is easy to miss and fatal: Explorer can restart, and when it does it
broadcasts `TaskbarCreated`. A tray icon that does not listen for it silently disappears for
the rest of the session. `TrayIcon::wndProc` re-adds itself.

## 4. The on-screen keyboard

Its obvious job is input: click ঞ if you cannot find it. Its **real** job is teaching.

Every key cap shows the Bengali glyph large and the Latin key that produces it small
underneath. Using the mouse therefore teaches the layout and gradually makes the board
unnecessary — which is the right ambition for a discoverability aid. Latching Shift flips
the board to the shifted glyphs, which is the only way the second half of a fixed layout
becomes discoverable at all.

The board is built from `FixedLayoutEngine`'s key map, not from a hard-coded picture. Edit
`config/layout_probhat.json` and the drawing changes with it. A layout and the keyboard that
displays it can never drift apart if there is only one source of truth.

Shift is one-shot: it releases after the key it modified, like a real keyboard, so nobody is
left silently latched. Anything that is not a key cap returns `HTCAPTION` from
`WM_NCHITTEST`, so the whole board is a drag handle and needs no title bar.

## 5. The visual language

All of it lives in `include/ui/UiTheme.h`. Hard-coding a colour anywhere else is how an
interface ends up looking assembled rather than designed.

Tokens are named by **role**, not hue — `SURFACE`, `TEXT_MUTED`, `ACCENT` — so a light theme
would be a change in one file and nowhere else.

**Dark and low-contrast on purpose.** The overlay sits on top of someone's document. It has
to be readable without competing with the thing being written. A bright panel would win the
fight for attention and lose the user's place in their sentence.

**Fonts.** Bengali is rendered in Nirmala UI, which ships with Windows 8+ and does conjunct
shaping and matra reordering properly — `UnicodeComposer` emits a logically correct codepoint
sequence, but turning ক + ্ + ষ into ক্ষ is the font's job. `UiTheme::createFont` verifies
with `GetTextFaceW` that Windows actually gave us the face requested, because it substitutes
silently, then falls back through Vrinda and Shonar Bangla.

**DPI.** Every measurement is a design unit at 96 DPI passed through `scale()`.
`GetDpiForWindow` is resolved dynamically because it is Windows 10 1607+, so the binary still
loads on older systems. Both windows handle `WM_DPICHANGED` by rebuilding fonts and
re-laying-out, which matters on a laptop docked to an external monitor.

**Painting.** Both windows double-buffer into a memory DC. The overlay repaints on every
keystroke; painting straight to the screen DC would flicker visibly while typing. The tray
icon is composed into a 32-bit DIB section for a real alpha channel, and every pixel's alpha
is rewritten afterwards because GDI text drawing zeroes it — skip that and the glyph appears
as a transparent hole punched through the icon.

## 6. Threading

All three surfaces live on the thread that runs the message pump, which is also the thread
the keyboard hook's callback runs on. That is deliberate: the hook updates the overlay with
a direct call, with no cross-thread marshalling anywhere on the keystroke path. The callback
does layout and an `InvalidateRect`, both cheap — which matters, because Windows silently
unhooks a low-level hook whose callback takes too long.

State changes flow one way. `KeyboardState` owns mode and the live-preview switch and
announces changes through a callback; the tray listens. Mode can change from two directions —
a global hotkey handled inside the hook, or the tray menu — and this is what guarantees the
icon can never disagree with the engine. Polling on a timer would have made the icon lag
behind the keystroke that changed it.

## 7. What is deliberately not here

- **A settings dialog.** Every setting is currently a JSON file that reloads on start, and a
  dialog that only duplicates a text file earns nothing.
- **A layout editor.** The on-screen keyboard already renders any layout file; making the
  caps editable is the natural next step, but drawing it and editing it are different
  problems and only the first is solved.
- **Docking or auto-hide for the on-screen keyboard.** It is draggable and toggleable, which
  covers the demo case. Remembering its position between runs needs a settings store,
  which is the item above.
