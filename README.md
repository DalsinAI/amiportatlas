# AmiPortAtlas

Pick a game from the AmiCradle Port Atlas, then download, install and play
it. AmiPortAtlas lists every game in the Atlas, ports and our own games
such as Canvas Skies, with their state, fit, screenshots and release pages.
A game with a release installs with one click, through the same Installer
script a hand install uses.

It runs on AmigaOS 3.2.3, GFOS (3.2.3 with GFOpenUp and the GFOpenKick ROM),
AROS and AmigaOS 4.1.

## What it needs

| | AmigaOS 3.2.3 / GFOS | AROS | AmigaOS 4.1 |
| --- | --- | --- | --- |
| Window | GFOpenLayout UI (linked in), in the GFOpenGadTools theme | the same | the same |
| Network | a TCP/IP stack (bsdsocket.library) | its stack | Roadshow |
| HTTPS | gfopentls.library (GFOpenUp) | gfopentls.library | gfopentls.library |
| Unpacking | SYS:Utilities/OpenCompress, else C:LhA | the same | C:LhA |
| Installing | SYS:System/Installer 43.3 or newer | the same | the same |

The games in the Atlas are 68k programs for AmigaOS 3, so they install on
3.2.3, GFOS and AROS m68k. On AROS x86 and AmigaOS 4.1, AmiPortAtlas lists
everything and opens the release pages; a game installs there once the
Atlas carries a build for that system.

## Where the list comes from

`https://www.amicradle.com/ports/ports.json`, made from the Port Atlas in
[DalsinAI/amicradle](https://github.com/DalsinAI/amicradle) (`docs/porting`).
The website's Port Atlas page reads the same file, so the two always agree.
[docs/FEED.md](docs/FEED.md) has the fields. A copy is kept in
`PROGDIR:Cache` for when the network is down.

## Building

```
make test                 host tests: the feed reader, SHA-256 and the real window
make render               the window drawn on the host as build/window.png
make os3                  AmigaOS 3.2.3 and GFOS (68040, soft float)
```

The Makefile's head names what each needs: a DalsinAI/gfopenlayout
checkout (`OLUI`), GFOpenGadTools' theme reader (`OGT`), GFOpenTLS's
headers (`TLS_INC`). The AROS and AmigaOS 4.1 builds follow once
GFOpenLayout UI's glue and GFOpenTLS are built there.

## Licence

MIT. Designed, tested and delivered by The AmiCradle Team.
