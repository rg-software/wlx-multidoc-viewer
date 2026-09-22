# Linux / Double Commander behavioral bugs — findings, actions, open items

Session note: work-in-progress log. Written 2026-09-22 after an interactive debugging
round on the Linux (Qt6, Wayland) build hosted in Double Commander. Reproduce and
finish the open items from here.

## How to run diagnostics

The debug hooks are opt-in via an environment variable read once at each call site:

```
WLX_KEY_DEBUG=1 doublecmd    # run DC from a terminal so stderr survives
```

With `WLX_KEY_DEBUG=1` the viewer writes to **two** places:

1. `qWarning()` lines prefixed `[WLX] ...` — go to the DC launch terminal.
2. The rightmost toolbar label (the `n / m` search-status label) — short tokens.

Observed tokens (this session): `ctrl+ctrl` (bare Ctrl key press, and **every**
Ctrl+* key, i.e. only the bare modifier ever arrives), `FOC` (focusFind reached),
`p<NN> <xxx>,<yyy>` (selection press hit-point canvas coords), `CPY` (copy reached).

Note: keys that cause a page/state change get their label overwritten almost
immediately by `ToolbarPresenter::refreshState()` (toolbar.cpp:112), so short
tokens are most reliable for Ctrl transitions; long-lived ones (FOC/CPY) survive.

## Symptom → verdict table

| Symptom | Status | What we know |
|---|---|---|
| Navigation keys (arrows, PgUp/PgDn, +/-/V/B/R/F12) | WORKS | Unmodified keys ARE delivered to the plugin; handled by the app-level `eventFilter` switch (viewer.cpp:957+). |
| Ctrl+F focuses search | WORKS | DC routes it via the WLX `ListSearchDialog` contract → `focusFind()` (viewer.cpp:616). Confirmed by `FOC` label. Search-box text not visibly highlighted in embedded context, but typing lands there. |
| Ctrl+C copies selection | BLOCKED (Wayland) | Only the bare Ctrl arrives (`ctrl+ctrl`); the `c` KeyPress never reaches the plugin. DC swallows modified combos. On Wayland there is no client-side interception (keys go to DC's focused surface; no grabs reach us). Open path: toolbar **Copy** button (works) or an alternative combo if DC forwards it. |
| Link hand cursor on Ctrl press/release | UNRESOLVED | `refreshHoverCursor()` (viewer.cpp:821) was rebuilt around a cached local pointer position (Wayland can't answer `QCursor::pos()`/`mapFromGlobal` reliably for an embedded widget). User reported "no visible change" this session; needs re-verification on a machine where the event path can be observed. |
| Text selection placement | UNRESOLVED | Highlight lands "very off"/"not right" from the cursor. One known cause fixed (see below), a residual component remains → needs the exact `[WLX] sel press` + `sel word rect` values captured from the target machine. |

## Confirmed root causes (with evidence)

1. **Keys starve because the viewer is a QFrame (NoFocus) + host-owned keyboard.**
   Any single-binding `QShortcut` loses to Double Commander's earlier-registered
   shortcuts (Qt resolves only one match; the host registered Left/Right first).
   Fix: `Qt::StrongFocus` on the viewer and canvas (viewer.cpp:241), the canvas
   claims focus on click, `onFocusFind`/keys handled in an application-level
   `eventFilter` installed as the LAST filter so it runs BEFORE `QShortcutMap`.

2. **Modified keys (except bare Ctrl) never reach the plugin on Wayland.**
   Evidence: bare Ctrl prints `ctrl+ctrl`; Ctrl+F/C print nothing new (only the
   initial `ctrl+ctrl`). DC handles Ctrl+F itself and calls `ListSearchDialog`
   (works). Ctrl+C is not forwarded at all. There is no Qt- or Wayland-client
   side way to see a key DC keeps for itself.

3. **Paged-mode hit-testing centered against the viewport, paint against the
   canvas.** `widgetToCanvas()` (viewer.cpp:710) used
   `m_scrollArea->viewport()->size()` while the paint path uses
   `pagedUnitCanvasOffset(c, size())` (viewer.cpp:56) with the canvas size. They
   are equal while a paged unit fits the viewport, but when a unit overflows and
   the canvas grows (`resizeCanvas`, viewer.cpp:459) the hit test shifts by half
   the overflow. Fixed: `widgetToCanvas` now uses the canvas size in both the
   unit and the fallback branch. Residual offset still reported → next step below.

4. **Exact modifier-mask equality dropped comboes carrying stray bits.**
   Was `mods == Qt::ControlModifier`; now `event->modifiers().testFlag(Qt::ControlModifier)`
   in `onControlKey` (viewer.cpp:859) and the app filter (viewer.cpp:950).

5. **Wayland cannot answer global-cursor queries.** `QCursor::pos()` /
   `QApplication::widgetAt` / `mapFromGlobal` were replaced by a locally cached
   mouse position (`m_lastHoverPos`, captured only from MouseMove targeted at our
   canvas/viewport; invalidated on Leave; viewer.h / viewer.cpp:1076).

## Code changes made this session

All in `src/viewer.cpp`, with one header change and no new files:

- `ViewerWidget` ctor: `setFocusPolicy(Qt::StrongFocus)`; app-level `installEventFilter`.
- MouseButtonPress on canvas: claims focus (`setFocus(Qt::MouseFocusReason)`) before ctrl+click link check / selection / pan.
- App-level `eventFilter` (viewer.cpp:912+): key map for arrows/PgUp/PgDn/Home/End/V/B/R/Esc/F12/Plus/Minus/0/Slash + Ctrl+F/Ctrl+C/Ctrl+Insert; KeyRelease passes through; text inputs (QLineEdit/QTextEdit/QPlainTextEdit) keep their keys; Ctrl transitions call `refreshHoverCursor()` unconditionally.
- `onControlKey()` kept as a backstop for events aimed at the viewer subtree directly.
- `refreshHoverCursor()`: cached local hover position instead of global guesses.
- `widgetToCanvas()`: canvas-size-based centering (fixes overflow regression).
- `focusFind()`/`copySelection()`/`startSelection()`/app filter: `WLX_KEY_DEBUG=1` instrumentation (`[WLX]` qWarning + label tokens).
- New scroll helpers for continuous-mode arrow keys: `pageJumpContinuous`, `stepVertical`, `stepVerticalTo` (mirror `ViewerWin32::pageJumpContinuous`).
- `viewer_settings.h` selection highlight was briefly bumped then **reverted** — keep the Windows-identical `0x69FFF069` while debugging; only revisit color after coordinates are proven.
- `src/plugin.cpp` unchanged (focusFind route already wired).

Current build: `cmake --preset linux-release && cmake --build --preset linux-release`
→ `build/linux-release/MultidocViewer.wlx64`. Deploy into the DC plugin directory
and **restart DC** after every build.

## Open items / next steps (the other machine)

1. **Selection offset — capture the two definitive values.** With `WLX_KEY_DEBUG=1`,
   in paged mode click a word near the TOP-CENTER of any page and read:
   - the terminal line `[WLX] sel press pos=... canvasPt=... page=... canvas=WxH vp=WxH scrollY=... paged=0/1`,
   - the line `[WLX] sel word idx=... rect=x,y,w,h`,
   - the label token `p<NN> <xxx>,<yyy>`.
   Compare `canvasPt` with the `word rect` center: if `rect` is where the click
   visually was, the mapping regressed; if `canvasPt` is where the click visually
   was but the highlight lands on `rect`, the word/rect layout disagrees with the
   painting. Post both lines + the label and the "highlight lands X" observation.

2. **Cursor on Ctrl — verify on the platform that will run it.** Press Ctrl over a
   link (hand should appear), release (hand should vanish), both without moving
   the mouse. If still dead: check that a MouseMove has actually been delivered to
   our canvas before the Ctrl press (a `[WLX]`-style press token could be added to
   the MouseMove hover branch if needed), and confirm the bare Ctrl release also
   arrives (label shows `ctrl+ctrl` on release).

3. **Ctrl+C fallback combos to probe whether DC forwards any modified key.**
   Test `Ctrl+Insert`, `Ctrl+Shift+C`, `Ctrl+Alt+C` — if any produces a non-`ctrl`
   label token and fires copy, DC's swallow list is only for Ctrl+C; otherwise
   resign Ctrl+C to the toolbar Copy button and note the Wayland limitation in
   the README/AGENTS "open gaps" table.

4. **Wayland vs X11 parity.** If the machine can run DC under XORG/Wayland-Xwayland,
   re-test items 1–3 there; `QCursor`-based behavior and native key delivery
   differ between the two and the fixes should hold in both.

## Keep-in-mind constraints

- QShortcut is NOT a viable path for single-key or Ctrl-keys inside DC (host wins
  the QShortcutMap race); the app-level event filter before the shortcut map is
  the intended mechanism.
- On Wayland, intercepted-by-DC keys are untouchable from the plugin; only DC's
  own forwarding (ListSearchDialog for Ctrl+F) and toolbar buttons survive.
- Never reintroduce `QCursor::pos()`/`mapFromGlobal` for instant hover decisions.