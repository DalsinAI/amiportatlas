# How to use AmiPortAtlas

## 1. Install it

You need AmigaOS 3.2 or later (or GFOS), a 68040 or better and a TCP/IP
stack (bsdsocket.library).

1. Unpack `AmiPortAtlas-0.2.0.lha` with LhA or OpenCompress, e.g. into RAM:.
2. Copy the `AmiPortAtlas` drawer anywhere, for example to `Work:`.
3. Copy `C/Atlas` to `C:`, for the Shell command.
4. Copy `Libs/gfopentls.library` to `LIBS:`.
5. Copy `S/GFOpenTLS/ca-bundle.pem` to `S:GFOpenTLS/` (make the drawer
   first if it isn't there).

From a Shell, with the archive unpacked in RAM:

    Copy RAM:AmiPortAtlas Work:AmiPortAtlas ALL CLONE
    Copy RAM:AmiPortAtlas.info Work: CLONE
    Copy RAM:C/Atlas C: CLONE
    Copy RAM:Libs/gfopentls.library LIBS: CLONE
    MakeDir S:GFOpenTLS
    Copy RAM:S/GFOpenTLS/ca-bundle.pem S:GFOpenTLS/ CLONE

Without gfopentls.library and the CA bundle, AmiPortAtlas still runs, but
it can't read anything over https and shows its last saved copies.

## 2. Find a game in the window

Double-click **AmiPortAtlas**. The list fills from the saved copies at
once, then from the network.

- **Find**: type part of a name or of its one-line description.
- **Show**: all, available, our games, by state, installed, or updates.
- **Fit**: any, 040, 060 or GPU.
- **From**: all sources, or just one (the Port Atlas, Aminet...).
- **Sort by**, or click a column heading.
- Pick an entry to see its pictures, state, CPU fit and release page.
  An entry from another source also shows About, By and Source.

**Install...** downloads the archive, checks its SHA-256 and runs its
Installer script (or copies its drawer, for archives that have none).
Once installed the same button says **Play**, and **Update...** when a
newer version is out; double-clicking the picture plays it too.
**Release page** opens its page. **Remove** deletes only what AmiPortAtlas
installed. **Find installed** looks on DH0: to DH15: for games you
installed by hand, and **Check for updates** reads every source again.

Installing from a source the Team doesn't check asks you first.

## 3. The same from the Shell

`Atlas` works like `apt`: it reads the saved copies until you tell it to
UPDATE.

    Atlas UPDATE                     read every source again, count the updates
    Atlas SEARCH tyrian              find entries
    Atlas SHOW opentyrian            what the sources say about one
    Atlas INSTALL opentyrian         install it (to Work:Games, or TO=drawer)
    Atlas LIST UPDATES               what has an update
    Atlas UPGRADE YES                install every update, no questions
    Atlas LIST INSTALLED FROM=amicradle
    Atlas REMOVE opentyrian

A name can be an id (`g-opentyrian`, `kobo@aminet-game`), a title, or any
part of one that only one entry has.

## 4. Choose your sources

    Atlas SOURCES                    every source, on or off, and what it gave
    Atlas SOURCE OFF aminet-game     stop reading one
    Atlas SOURCE ON aminet-util      start reading one
    Atlas UPDATE

The master list comes from this repository (`data/sources.json`), so new
sources reach every player without a new AmiPortAtlas.

## 5. Add a source of your own

Any web space can be a source: a drawer with your archives, their readmes
and pictures, and an `index.json` in the form [FEED.md](FEED.md) and
[SOURCES.md](SOURCES.md) describe.

    Atlas SOURCE ADD mygames URL=https://example.org/amiga/index.json NAME="My games"
    Atlas UPDATE
    Atlas LIST FROM=mygames

`Atlas SOURCE DROP mygames` takes it away again. Your lines are kept in
`ENVARC:AmiPortAtlas/Sources`.

## 6. Publish your own program

Write an Aminet-style readme beside the archive (`MyGame.readme`, with
Short:, Author:, Type: and Version:), then:

    Atlas PUBLISH Work:Out/MyGame.lha TO=mygames UPLOAD=ftps://you@ftp.example.org/htdocs/amiga/
      PICTURES=Work:Out/shot1.png,Work:Out/shot2.png INSTALLER

Atlas asks for the password (nothing is shown and nothing is kept), puts
the entry into your index and uploads the archive, readme, pictures and
the index last, so nobody ever reads half an upload. Add INSTALLER when
the archive carries its own `Install-MyGame` script; without it, an install
copies the archive's drawer.

To send the same archive to Aminet's upload drawer:

    Atlas PUBLISH Work:Out/MyGame.lha TO=aminet UPLOAD=ftp://anonymous@main.aminet.net/new/

To have your source in everyone's list, open a pull request adding it to
`data/sources.json`.

## 7. When something goes wrong

- **"No source could be read"**: check the network, gfopentls.library and
  `S:GFOpenTLS/ca-bundle.pem`. The saved copies still show.
- **A source "not read"** in `Atlas SOURCES` names why (a 404, no
  network). The other sources are still read.
- **An install refused**: the drawer already holds a game AmiPortAtlas
  didn't install, or there isn't the space. The message says which.
