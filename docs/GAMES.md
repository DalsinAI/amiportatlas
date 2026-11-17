# The Games drive and the games index

Dale, 17 November 2026: AmiPortAtlas becomes the games launcher. It sits at
the top of the Games drive, knows every source of games, and keeps an
SQLite index of them.

## The Games drive

`Games:` (DH2, a volume, in the standard build) has five drawers at its
top, one for each place games come from:

| Drawer | What goes in it |
| --- | --- |
| `Ports` | our ports, from the AmiCradle Port Atlas |
| `WHDLoad` | WHDLoad games |
| `Itch.io` | games from itch.io |
| `Aminet` | games from Aminet |
| `Exclusive` | our own games (the Atlas's `origin` is `original`) |

The standard build makes them (DalsinAI/amicradle,
`scripts/appliance/gfos_card_machine.py`), each with the machine's drawer
icon. AmiPortAtlas and its index sit beside them.

## The index

`Games:Atlas.db`, an SQLite database (`src/gamesdb.c`). It holds every
entry of every source that is switched on, as the one list the window
shows, and every game found in the five drawers, listed or not. It is
made again from the sources and the disk whenever asked, so nothing in it
is the only copy of anything: deleting it loses nothing.

Which drawer a game belongs in comes from its source: the Port Atlas
(`amicradle`) is Ports, a source whose id is or starts with `whdload`,
`itch` or `aminet` has its own drawer, and one of our own games is
Exclusive whichever source lists it. An entry that is not a game (an
application, a tool), or that comes from any other source, is indexed with
no drawer.

Text is UTF-8. `PRAGMA user_version` is the schema's number (1): an index
with an older number is made again empty, a newer one is refused.

| Table | Rows | Columns |
| --- | --- | --- |
| `folders` | the five drawers | `folder` (0 to 4, the order above), `name` |
| `sources` | the sources as last read | `id`, `name`, `home`, `idx` (the index's address), `note`, `is_on`, `checked`, `folder` |
| `games` | one for each game or program | below |
| `shots` | a game's pictures | `game`, `n` (0 is the main one), `url` |
| `needs` | what a game needs | `game`, `what` (`requires` or `module`), `name` |
| `meta` | `generated`: the Atlas's date | `key`, `value` |

`games`, from the sources ([FEED.md](FEED.md), [SOURCES.md](SOURCES.md)):
`id` (the list's: `kobo`, `kobo@aminet-game`), `source`, `folder` (NULL:
none), `number`, `origin`, `kind`, `title` (compared without case), `year`,
`state`, `fit`, `game_data`, `version`, `summary`, `author`, `category`,
`cpu`, `fpu`, `space`, `download_url`, `download_size`, `download_sha256`,
`release_url`, `port_repo`, `drawer`, `program`, `install`, `shot` (the
main picture), and `listed` (1 while a source lists it).

`games`, from this machine: `installed_version` (installed through
AmiPortAtlas; an older one than `version` is an update), `found` (1: its
drawer is in one of the five drawers), `path` (its drawer's full name) and
`found_program`.

A drawer in one of the five that no source lists gets a row of its own:
its `id` is `disk:` and the two names in small letters
(`disk:whdload/lemmings`), its `title` is the drawer's name, its `source`
is NULL and `listed` is 0. So a WHDLoad set copied onto the drive shows in
the launcher before any source describes it, and a source that later lists
those games (by `drawer`) takes the rows over.

An entry the sources drop goes from the index, unless it is on this
machine: then it stays, `listed` 0, until it is removed.

## Still to do

1. The index built on the Amiga: AmiPortAtlas and Atlas linked with
   DalsinAI/gfopensqlite (soft float, one task), `Atlas INDEX` to make it
   from the Shell, and the five drawers looked through.
2. The window reading the index: the five drawers as its sections.
3. The launcher as a commodity, in the systray once started.
4. The full screen mode driven by a joystick.
5. Sources for WHDLoad, itch.io and Aminet: what each may fetch.
