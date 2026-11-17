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
#include "gamesfolders.h"

#define GAMESDB_SCHEMA 2
#define GAMESDB_DRIVE  "Games:"
#define GAMESDB_FILE   "Games:Atlas.db"   /* at the top of the Games drive, beside the launcher */

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

/* Today's date, "2026-11-17", for the New page: an entry that put_feed sees
 * for the first time gets it as first_seen, and one whose version changed
 * gets it as updated. The first fill of an empty index marks nothing, or
 * every game would be new on the first day. Not set, nothing is dated. */
void gamesdb_set_today(GamesDb *db, const char *date);

/* Each entry new or updated on or after date given to fn, by its id, in the
 * order strcmp puts them. How many, -1 on failure. */
typedef void (*GamesDbIdFn)(void *user, const char *id);
long gamesdb_each_new(GamesDb *db, const char *date, GamesDbIdFn fn, void *user);

/* How many entries are new or updated on or after date. -1 on failure. */
long gamesdb_new_since(GamesDb *db, const char *date);

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
