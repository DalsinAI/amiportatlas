/* AmiPortAtlas: the games index, an SQLite database of every game the
 * sources list and every game on the Games drive.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * The Games drive has five drawers at its top, one for each place games
 * come from: Ports, WHDLoad, Itch.io, Aminet and Exclusive. The index says
 * for each game which of them it belongs in, what the sources say of it,
 * and whether it is on this machine and where. docs/GAMES.md has the
 * tables. Plain C with SQLite and no Amiga calls, so the host tests try it;
 * on the Amiga it links DalsinAI/gfopensqlite (soft float, one task).
 */
#ifndef AMIPORTATLAS_GAMESDB_H
#define AMIPORTATLAS_GAMESDB_H

#include "feed.h"
#include "sources.h"

#define GAMESDB_SCHEMA 1
#define GAMESDB_DRIVE  "Games:"
#define GAMESDB_FILE   "Games:Atlas.db"   /* at the top of the Games drive, beside the launcher */

/* The Games drive's drawers, in the order they are shown. */
enum { GF_NONE = -1, GF_PORTS, GF_WHDLOAD, GF_ITCH, GF_AMINET, GF_EXCLUSIVE, GF_COUNT };

/* "Ports", "WHDLoad", "Itch.io", "Aminet", "Exclusive"; NULL for GF_NONE. */
const char *games_folder_name(int folder);

/* The drawer with that name, without case; GF_NONE when it is none of them. */
int games_folder_named(const char *name);

/* Where a source's games go: a source whose id is or starts with "whdload",
 * "itch" or "aminet" has its own drawer, the Port Atlas ("amicradle") is
 * Ports, and any other source has none (GF_NONE): its entries are indexed
 * but belong in no drawer of the Games drive. */
int games_folder_of_source(const char *source_id);

/* Where an entry goes: one of our own games (origin "original") is
 * Exclusive wherever it is listed; anything that is not a game has no
 * drawer; else its source's. */
int games_folder_of(const Port *p);

/* A drawer that only sorts the games under it, as big sets have ("A", "B",
 * "0-9", "#"): a name of one character, or "0-9". Its drawers are the
 * games; it is not one itself. */
int games_shelf_drawer(const char *name);

typedef struct GamesDb GamesDb;

/* Opens the index, making the file and its tables when they are not there.
 * An index of an older schema is rebuilt empty; one of a newer schema is
 * refused. NULL and an English reason in why on failure. */
GamesDb *gamesdb_open(const char *path, char *why, int whylen);
void     gamesdb_close(GamesDb *db);

/* The last thing that went wrong, as SQLite said it. */
const char *gamesdb_error(GamesDb *db);

/* The sources as they are now: the table is replaced. 1 on success. */
int  gamesdb_put_sources(GamesDb *db, const Sources *s);

/* Every entry of the one list (feed_merge's) put in: an entry already there
 * is brought up to date and keeps what the machine knows of it (installed,
 * found, its drawer on disk); one the sources no longer list goes, unless
 * it is on this machine. All in one transaction: on failure the index is
 * as it was. Answers how many entries are listed, -1 on failure. */
int  gamesdb_put_feed(GamesDb *db, const Feed *f);

/* A game installed through AmiPortAtlas (ENVARC:AmiPortAtlas/Installed):
 * its version and its drawer. version NULL: no longer installed. Answers 1
 * when the index has that id. */
int  gamesdb_set_installed(GamesDb *db, const char *id, const char *version, const char *path);

/* Forgets what was found on disk, before the Games drive is looked through
 * again. Games installed through AmiPortAtlas stay. */
int  gamesdb_clear_found(GamesDb *db);

/* A drawer found in one of the Games drive's drawers: path is the drawer's
 * full name ("Games:WHDLoad/Lemmings"), program the file that starts it or
 * NULL. A listed game of that drawer whose own drawer has this name
 * (without case) is marked found; else it is a game no source lists and
 * gets a row of its own, "disk:whdload/lemmings", titled as its drawer.
 * Answers 1 when it matched a listed game, 0 when it made or kept a row of
 * its own, -1 on failure. */
int  gamesdb_put_found(GamesDb *db, int folder, const char *drawer, const char *path, const char *program);

/* How many games (kind "game"): in one drawer, or with GF_NONE in all the
 * index; with here set, only those on this machine. -1 on failure. */
long gamesdb_count(GamesDb *db, int folder, int here);

/* One text answer, for the tests and the Shell: the first column of sql's
 * first row into out ("" when there is none). 1 when a row came. */
int  gamesdb_text(GamesDb *db, const char *sql, char *out, int max);

#endif
