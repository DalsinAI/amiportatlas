/* AmiPortAtlas: telling a game's drawer and its program by their names.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * The scan of the hard disks (install.c) asks this which port a drawer is,
 * and which file in it is the game. Plain C, so the host tests try it.
 */
#ifndef AMIPORTATLAS_MATCH_H
#define AMIPORTATLAS_MATCH_H

#include "feed.h"

#define MATCH_NAME 40

/* A drawer name made comparable: letters and digits only, lower case
 * ("Fish Fillets NG" and "FishFillets_NG" are both "fishfilletsng"). */
void match_key(const char *name, char *out, int max);

typedef struct Matcher Matcher;

/* The names each port's drawer may have: the feed's drawer; the title and
 * the id, whole and by their first words; the title's initials ("HoD"). */
Matcher *matcher_new(Port *const *ports, int n);
void     matcher_free(Matcher *m);

/* The port a drawer called name is, or NULL. *strength is how sure:
 * 5 the feed's drawer, 4 the whole title or id, less for parts of them. */
Port    *matcher_find(const Matcher *m, const char *name, int *strength);

/* How well a file in p's drawer looks like its program; 0 when it can't be
 * ("game.log", "game.info", "game.old", "game_old"). The drawer's own name,
 * the feed's program and the id score most. */
int      match_program(const Port *p, const char *drawer, const char *file);

/* The version in a program's version string: "$VER: Brogue CE 1.15.1 Amiga
 * (03.11.2026)" gives "1.15.1". 0 when it has none. */
int      match_version(const char *ver, char *out, int max);

/* Versions compared by their numbers ("1.9" before "1.10"): below 0 when a
 * is older than b, 0 when the same or either has no number. */
int      version_cmp(const char *a, const char *b);

#endif
