/* AmiPortAtlas: reading the sources and their indexes, for the window and
 * for the Atlas command. MIT, (c) 2026 The AmiCradle Team.
 *
 * The master list (SOURCES_URL) and each source's index are saved in the
 * cache drawer, so both work with the network down. The cache drawer is
 * the one ENV:AmiPortAtlas/Cache names (the window sets it to its own
 * PROGDIR:Cache), else S:AmiPortAtlas.
 */
#ifndef AMIPORTATLAS_REPO_H
#define AMIPORTATLAS_REPO_H

#include "feed.h"
#include "sources.h"

#define REPO_INDEX_MAX   (4L * 1024 * 1024)   /* one source's index */
#define REPO_SOURCES_MAX (256L * 1024)        /* the master list */

/* Told what is being read, a line at a time ("Reading OS4Depot..."). */
typedef void (*RepoTell)(void *user, const char *line);

/* The cache drawer, made when it isn't there. */
void  repo_cache(char *out, int max);

/* Reads the master list and the player's lines, then the index of every
 * source switched on. With fetch, each comes from the network and is
 * saved, its saved copy used when the network fails; without, the saved
 * copies only (a source never read is read from the network all the same).
 * Answers every source's entries in one feed, each told its source, and
 * the sources, with what became of each, in *out (the caller frees both).
 * NULL when nothing could be read at all, with the reason in why. */
Feed *repo_read(int fetch, Sources **out, RepoTell tell, void *user, char *why, int whylen);

/* One line on how the sources went: "AmiCradle Port Atlas 414, My games 3;
 * OS4Depot: no index yet; Aminet: games: offline, the saved copy". */
void  repo_summary(const Sources *s, char *out, int max);

/* A source's name for an entry's source id ("" when unknown). */
const char *repo_source_name(const Sources *s, const char *id);

/* The player's lines (ENVARC:AmiPortAtlas/Sources): malloc'd, "" when
 * there are none; NULL on no memory. Saving writes ENV: and ENVARC:. */
char *repo_user_lines(void);
int   repo_user_save(const char *text);

#endif
