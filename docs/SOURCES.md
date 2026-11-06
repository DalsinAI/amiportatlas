# Sources

AmiPortAtlas and its Shell command, Atlas, read several sources, as APT
reads the repositories in its sources.list: each source offers games and
programs, and AmiPortAtlas shows them all as one list, finds updates in any
of them and installs from any of them.

## The master list

`data/sources.json` in this repository, read from its main:

    https://raw.githubusercontent.com/DalsinAI/amiportatlas/main/data/sources.json

(`ENV:AmiPortAtlas/SourcesURL` names another, for labs.) It is read every
time the sources are, and kept in the cache drawer as `sources.json`. With
no copy at all, AmiPortAtlas reads the Port Atlas alone.

Top level: `schema_version` (1; a newer one asks for an update of
AmiPortAtlas), `generated`, `sources`. Each source:

| Field | What |
| --- | --- |
| `id` | 1 to 24 of `a-z`, `0-9` and `-`. Names the saved copy and the `@id` of its entries |
| `name` | as the player reads it ("Aminet: games") |
| `index` | the address of its index (https:// or http://); `null`: listed, no index yet |
| `home` | its own web pages; an entry from it without a page of its own links here |
| `on` | read unless the player switches it off |
| `checked` | the Team checks what it offers; installing from any other source asks first |
| `pictures` | its entries have screenshots |
| `systems` | where its programs run: `os3`, `gfos`, `aros-m68k`, `aros-x86`, `os4`, `morphos` |
| `note` | a line about it |

Unknown fields are read past. A bad id, a second source with the same id
or an index that isn't http(s) is left out.

The list starts with the AmiCradle Port Atlas (checked, on) and lists
Aminet (games on, utilities off), OS4Depot, MorphOS-Storage and AROS
Archives. Those four have no index yet: each needs a reader that turns the
site's own listing into an index (below, still to do).

## The player's own lines

`ENVARC:AmiPortAtlas/Sources` (written with `Atlas SOURCE`, or by hand):

    ; AmiPortAtlas sources: on|off ID [INDEX-URL [NAME]]
    off aminet-game
    on os4depot
    on mygames https://example.org/amiga/index.json My games

A line naming a master source only switches it on or off: its index stays
the Team's. A line with another id and an index adds a source of the
player's own, never checked.

## An index

Every source's index has the form of the Port Atlas's `ports.json`
([FEED.md](FEED.md)): `schema_version` 1, `generated`, an optional `name`,
and `ports`, a list of entries. Besides the fields FEED.md lists, an entry
may have:

| Field | What |
| --- | --- |
| `kind` | `game` (when none), `application`, `tool`, `library`, `demo`, `development` |
| `short` | one line on what it is (Aminet's Short:) |
| `author` | who made it |
| `category` | the source's own place for it: `game/think`, `Games/Puzzle` |
| `install` | `script` (when none): the archive's `Install-DRAWER` Installer script runs; `unpack`: the archive's `DRAWER` drawer and its icon are copied into the drawer picked |

An entry installs when it has `download_url`, `download_sha256` (64 hex
digits), and plain `id`, `drawer` and `program`, from any source. Its
`number` is the Atlas's own; other sources' entries have none and sort
after the Atlas's.

In the one list, the Atlas's entries keep their ids (the installed list
names games by them); another source's entry is `id@source`, so the same
name in two sources stays two entries, each with its own Source.

## The window

**From** picks all sources or one. Find matches the name and the line.
An entry from another source shows **About**, **By** and **Source**. The
status line says how each source went: entries read, a saved copy used, or
not read. Installing from a source that isn't checked asks first.
**Check for updates** reads every source again.

## Atlas, from the Shell

`C:Atlas`, reading the same sources, saved copies and installed list as
the window:

| Command | What |
| --- | --- |
| `Atlas UPDATE` | reads the master list and every source again, and says how many updates there are |
| `Atlas LIST [ALL\|AVAILABLE\|INSTALLED\|UPDATES] [FROM=id]` | what there is (AVAILABLE when none is given) |
| `Atlas SEARCH words` | entries with every word in their name, line or id |
| `Atlas SHOW name` | what the sources say about one |
| `Atlas INSTALL names [TO=drawer] [YES]` | installs, into `ENV:AmiPortAtlas/Drawer` or Work:Games when TO isn't given |
| `Atlas UPGRADE [names] [YES]` | installs every update, or those named, where each game is |
| `Atlas REMOVE names [YES]` | deletes what AmiPortAtlas installed |
| `Atlas SOURCES` | the sources, on and off, and what each gave |
| `Atlas SOURCE ON\|OFF\|DROP id` | switches a source, or takes the player's own away |
| `Atlas SOURCE ADD id URL=index [NAME=name]` | adds a source of the player's own |
| `Atlas PUBLISH archive TO=id ...` | below |

A name is an id (`g-kobodeluxe`, `kobo@aminet-game`), a title, or a part of
one that only one entry has. YES answers the questions. Atlas, like
`apt`, reads the saved copies unless told to UPDATE; a source never read
is read from the network.

## Publishing to your own source

A source is a drawer of files on any web space: the archives, their
readmes and pictures, and the index. `Atlas PUBLISH` puts a new archive
there, as `dput` does for an APT repository:

    Atlas SOURCE ADD mygames URL=https://example.org/amiga/index.json NAME="My games"
    Atlas PUBLISH Work:Out/MyGame.lha TO=mygames UPLOAD=ftps://dale@ftp.example.org/htdocs/amiga/
      PICTURES=Work:Out/shot1.png,Work:Out/shot2.png

1. It reads the archive's size and SHA-256, and `MyGame.readme` beside it
   (or README=), Aminet's form: Short:, Author:, Type:, Version:.
2. It asks for the password (shown as nothing, never kept). The upload
   address is kept in `ENVARC:AmiPortAtlas/Publish` for next time.
3. It fetches the source's index over the same connection (a new one when
   there is none), puts the entry in (replacing one with the same id,
   which comes from the archive's name), and uploads the archive, the
   readme and the pictures, then the index last, each as `.part` renamed
   into place, so a reader never sees half a file or an index naming one
   that isn't there.

The entry: TITLE= (the archive's name when none), VERSION= (the readme's),
DRAWER= (the title) and PROGRAM= (the drawer), KIND= (from Type:), CPU=,
FPU, INSTALLER for an archive with its own `Install-DRAWER` script (else
it installs by copying its drawer). Its download address is the index's
drawer and the archive's name.

To a target with no index of ours (Aminet's upload drawer), PUBLISH
uploads the archive and its readme only, as they are, and needs Short:
and Type: in the readme:

    Atlas PUBLISH Work:Out/MyGame.lha TO=aminet UPLOAD=ftp://anonymous@main.aminet.net/new/

ftps:// is FTP with TLS (AUTH TLS, then PROT P for the data), through
GFOpenTLS; ftp:// is plain, and sends the password as it is, so it is for
anonymous uploads.

Joining the master list is a request to the Team: a pull request adding
the source to `data/sources.json`.

## Still to do

1. Readers for Aminet, OS4Depot, MorphOS-Storage and AROS Archives, each
   turning the site's listing into an index the master list then names.
   They run off the Amiga and publish next to the Port Atlas.
2. Installs for other systems: an OS4Depot or MorphOS-Storage entry is
   listed and opens its page, and installs once AmiPortAtlas is built
   there and knows its systems.
3. The upload address for Aminet confirmed against Aminet's own upload
   rules before its first real upload.
4. FTPS data connections that need the control connection's TLS session
   again (vsftpd's require_ssl_reuse): GFOpenTLS has no session reuse yet.
