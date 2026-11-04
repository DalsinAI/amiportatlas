# AmiPortAtlas design

The window follows the mockup Dale approved on 4 November 2026: the
selected game's pictures top left, cycling every five seconds through that
game's own screenshots; its facts under them; the whole Atlas on the right,
anything without a release greyed; the buttons bottom right, read last.

- **Window:** GFOpenLayout UI: the window is HTML and CSS (`src/ui.c`),
  laid out by GFOpenLayout and drawn in the theme GFOpenPrefs Look chose.
  The same code lays the window out on the host (`make render`).
- **List:** a `vlist` (rows given as they are drawn, so the 400-odd entries
  cost only what shows) with columns #, Name, Year, State, Fit, Data and
  Installed, and a mark: green to install, blue installed, orange an
  update, gold our own games, grey not released yet. Clicking a column title sorts by it, again reverses.
  Show: All ports, Available, Our games, Released, Testing, In progress,
  Installed, Updates. Fit: Any, 040, 060, GPU. Find filters by name as it is
  typed.
- **Play:** double-clicking an installed game's row or its picture starts
  it, as Workbench would from its icon. The Install button reads Play for
  an installed game and Update when the feed has a newer version.
- **Install:** the checks (CPU, FPU, GFOpenGPU modules, room on the drawer
  and on T:) come before any download. The archive comes over HTTPS with
  GFOpenTLS and is checked against the feed's SHA-256 as it arrives, is
  unpacked into `T:AmiPortAtlas`, and its own `Install-DRAWER` script runs
  as a Novice install. The drawer picked is in `ENV:AmiPortAtlas/Parent`
  while it runs, and remembered in `ENVARC:AmiPortAtlas/Drawer`.
- **Remove:** asks, then deletes the game's drawer and icon.

## Code

| File | What |
| --- | --- |
| `src/feed.c` | the ports.json reader, sorting; plain C, host-tested |
| `src/sha256.c` | SHA-256; plain C, host-tested |
| `src/http.c` | HTTP/1.1 GET over bsdsocket.library and GFOpenTLS; redirects, chunked |
| `src/install.c` | checks, install, launch, remove, the installed list |
| `src/platform.c` | libraries and interfaces, hooks, CPU, free space, per system |
| `src/ui.c` | the window in HTML and CSS, and what it does; no Amiga calls, host-tested |
| `src/app.c` | the Intuition window and olui's glue, pictures, marks, the feed, installing |
| `tests/ui_host.c`, `tools/render_ui.py` | the real window laid out and drawn on the host |

## Still to do

1. `port_installer.py` in amicradle: offer `ENV:AmiPortAtlas/Parent` as the
   default drawer when it is set, so a Novice install goes where the player
   picked.
2. The install progress window from the mockup, with Stop (the first cut
   uses the status line and the gauge).
3. Pictures fetched in the background instead of when a game is picked.
4. AROS and AmigaOS 4.1 builds: GFOpenLayout UI's Intuition glue and
   GFOpenTLS built there.
7. A vlist row in grey for what can't be installed yet (gfopenlayout: a
   class a row, from the row function); the mark says it for now.
5. In the feed: the drawer's keep list (saves and settings) so Remove can
   keep them, and per-system downloads for AROS x86 and AmigaOS 4.1.
6. First build and lab run on origimagic for each target.
