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

Text is UTF-8. `PRAGMA user_version` is the schema's number (2): an index
with an older number is made again empty, a newer one is refused.

| Table | Rows | Columns |
| --- | --- | --- |
| `folders` | the five drawers, and any other at the top of the drive | `folder` (0 to 4, the order above; from 5 the player's own), `name` |
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

`games`, for the New page (Dale, 17 November 2026: "a 'new' which is a
notifiactions / news list page of games"): `first_seen`, the date
(`2026-11-17`) the index first saw the entry, and `updated`, the date its
`version` last changed. The first fill of an empty index dates nothing, so
the page starts empty rather than with every game. The page is

    SELECT ... FROM games WHERE listed = 1 AND (first_seen >= :since OR updated >= :since)
    ORDER BY max(ifnull(first_seen, ''), ifnull(updated, '')) DESC, title

A drawer in one of the five that no source lists gets a row of its own:
its `id` is `disk:` and the two names in small letters
(`disk:whdload/lemmings`), its `title` is the drawer's name, its `source`
is NULL and `listed` is 0. So a WHDLoad set copied onto the drive shows in
the launcher before any source describes it, and a source that later lists
those games (by `drawer`) takes the rows over.

An entry the sources drop goes from the index, unless it is on this
machine: then it stays, `listed` 0, until it is removed.

## The window's sections

Above the list is a row of tabs: **All**, **New**, then a tab for each
drawer there is something to show for. The tabs are found, not fixed
(Dale, 17 November 2026: "the tabs at hte top need to autodiscvoer some
how"). A drawer gets a tab when

- it is at the top of the Games drive, whatever its name (the launcher's
  own drawer apart), so a drawer the player makes there shows up; or
- a source lists a game that belongs in it, so a new source's drawer shows
  up as soon as its index is read, and with no Games drive at all the
  window still has the sections the sources fill.

The five standard drawers come first in their order, any other after
them. The drive is looked at when the program starts and at every Find
installed. A drawer's tab shows the games that belong in it
(`src/gamesfolders.c`, the same rule the index uses); New shows what the
index dates as new or updated. Find, Show, Fit, From and the sorting work
inside whichever section is shown.

## Who makes the index

AmiPortAtlas makes it again each time it reads the sources (at its start
and at Check for updates), when there is a Games drive; `Atlas INDEX`
makes it from the Shell. Both use `src/gamesindex.c`. The New section
shows what the index dates as new or updated in the last 14 days.

## Games that are only on the drive

Every drawer at the top of `Games:` is looked through, not only the five
(the launcher's own drawer aside): one the player made is a folder too,
numbered from 5 in `folders` and found again each time the index is made.
A drawer in it that no source lists gets a `disk:` row, and the window
adds those rows to its list after the sources' entries (source `disk`,
state "On disk", "found" under Installed), each under the tab of the
drawer it is in. In a drawer the player made, a drawer named as a listed
game's marks that game found instead, so a port kept in `Games:Ported`
is not shown twice.

## Where a game is installed

On a machine with the Games drive a game goes into its drawer there:
`Games:Ports/KoboDeluxe` for a port, `Games:Exclusive/CanvasSkies` for one
of our own, and so on for each source's drawer (`games_install_drawer`).
The drawer is made when the drive has none. A drawer the player chose
before (`ENV:AmiPortAtlas/Drawer`) still wins, and `Atlas INSTALL TO=`
names another. With no Games drive it is `Work:Games`, then `Work:`, as
before. The port Installer scripts follow the same rule (`DH2:Ports`;
amicradle docs/porting/INSTALLER.md).

## Still to do

3. The launcher as a commodity, in the systray once started.
4. The full screen mode driven by a joystick.
5. Sources for WHDLoad, itch.io and Aminet: what each may fetch.
