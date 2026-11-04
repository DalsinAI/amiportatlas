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
| Window | ReAction (in 3.2.3; GFOpenReAction on GFOS) | GFOpenReAction | ReAction |
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
make test                 the host tests: the feed reader and SHA-256
make os3                  AmigaOS 3.2.3 and GFOS (m68k-amigaos-gcc)
make aros-m68k            AROS m68k
make aros-i386            AROS x86
make os4                  AmigaOS 4.1 (ppc-amigaos-gcc)
```

`TLS_INC` points at GFOpenTLS's `include` drawer (default
`../openamigatls/include`).

## Licence

MIT. Designed, tested and delivered by The AmiCradle Team.
