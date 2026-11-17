/* AmiPortAtlas: the games index made and read on the Amiga, for the Atlas
 * command and for the window. MIT, (c) 2026 The AmiCradle Team.
 *
 * src/gamesdb.c is the index itself, plain C; this is what needs the
 * system: the look through the Games drive's drawers, today's date, what
 * AmiPortAtlas installed.
 */
#ifndef AMIPORTATLAS_GAMESINDEX_H
#define AMIPORTATLAS_GAMESINDEX_H

#include "gamesdb.h"

typedef struct GamesIndexReport {
    int  listed;                 /* entries from the sources */
    long news_today;             /* new or updated today */
    long listed_in[GF_COUNT];    /* games listed for each drawer */
    int  on_drive[GF_COUNT];     /* drawers found in each on the drive */
    int  stopped;                /* stop() said so: the index is not whole */
} GamesIndexReport;

/* 1 when drive is there ("Games:"); no "insert volume" requester. */
int  games_index_drive(const char *drive);

/* The index in file made again from the one list f, the sources s, what
 * AmiPortAtlas installed (installed_load() first) and the drawers in
 * drive's five drawers; a drawer that only sorts games ("A", "0-9") is
 * looked into, once. stop (may be NULL) is asked between drawers. 1, or 0
 * with the reason in why. */
int  games_index_make(const char *drive, const char *file, const Feed *f, const Sources *s,
                      int (*stop)(void), GamesIndexReport *r, char *why, int whylen);

/* The ids of the entries new or updated in the last days days, sorted, in
 * *ids (malloc'd, each one malloc'd; games_index_news_free frees them).
 * How many, 0 when there is no index or no news, -1 on no memory. */
int  games_index_news(const char *file, int days, char ***ids);
void games_index_news_free(char **ids, int n);
/* 1 when id is among them. */
int  games_index_is_news(char *const *ids, int n, const char *id);

#endif
