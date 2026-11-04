/* AmiPortAtlas: the Port Atlas feed (ports.json).
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * The feed is published at https://www.amicradle.com/ports/ports.json by the
 * Port Atlas publisher in DalsinAI/amicradle (docs/porting). Its fields are
 * described in docs/FEED.md. This reader is plain C with no Amiga calls, so
 * the same file builds for every target and for the host tests.
 */
#ifndef AMIPORTATLAS_FEED_H
#define AMIPORTATLAS_FEED_H

#define FEED_SCHEMA 1

typedef struct Port {
    long  number;          /* stable: the record's place in the Atlas */
    char *id;
    char *origin;          /* "port", or "original" for our own games (Canvas Skies) */
    char *title;
    char *year;
    char *display;
    char *state;           /* not_started, in_progress, testing, released */
    char *fit;             /* 040, 060, GPU */
    char *game_data;       /* free, shareware, own, restricted */
    char *version;
    char *release_url;
    char *port_repo;
    char *download_url;    /* NULL: nothing to install yet */
    long  download_size;
    char *download_sha256; /* 64 hex digits */
    char *drawer;
    char *program;
    char *cpu;             /* 68020, 68030, 68040, 68060 */
    int   fpu;
    long  space;           /* bytes the install needs */
    char **requires;  int nrequires;
    char **modules;   int nmodules;
    char **shots;     int nshots;  /* screenshots, the main one first */
} Port;

typedef struct Feed {
    long  schema;
    char *generated;
    Port *ports;
    int   count;
} Feed;

/* Parses a whole ports.json held in memory (len bytes, need not end in 0).
 * Text is turned into ISO-8859-1 for Amiga fonts; a character outside it
 * becomes '?'. Answers NULL and an English reason in *why on failure. */
Feed *feed_parse(const char *text, long len, const char **why);
void  feed_free(Feed *feed);

/* A port can be installed when the feed gives a download for it. */
int   port_installable(const Port *p);

/* Sorting the list. */
enum { SORT_NUMBER, SORT_NAME, SORT_YEAR, SORT_STATE, SORT_FIT, SORT_COUNT };
void  feed_sort(Port **list, int n, int key, int descending);

/* 1 for our own games rather than ports. */
int   port_is_original(const Port *p);

/* The state as the player reads it: "Released", "Testing" and so on. */
const char *port_state_text(const Port *p);

#endif
