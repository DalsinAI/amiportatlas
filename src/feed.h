/* AmiPortAtlas: the Port Atlas feed (ports.json).
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * The Port Atlas is published at https://www.amicradle.com/ports/ports.json
 * by the Port Atlas publisher in DalsinAI/amicradle (docs/porting); every
 * other source's index has the same form. The fields are in docs/FEED.md,
 * the sources in docs/SOURCES.md. This reader is plain C with no Amiga
 * calls, so the same file builds for every target and for the host tests.
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
    /* From the sources (docs/SOURCES.md): */
    char *source;          /* the id of the source it came from ("amicradle", "aminet-game") */
    char *kind;            /* game (when none), application, tool, library, demo... */
    char *summary;         /* "short": one line on what it is */
    char *author;
    char *category;        /* the source's own: "game/think", "Games/Puzzle" */
    char *install;         /* "script" (when none): run its Install-DRAWER; "unpack": its drawer is copied */
} Port;

typedef struct Feed {
    long  schema;
    char *generated;
    Port *ports;
    int   count;
} Feed;

/* Text comes out as ISO-8859-1 for Amiga bitmap fonts (a character outside
 * it becomes '?'), or, with feed_utf8 set, as UTF-8 (GFOpenLayout UI takes
 * UTF-8 and turns it into the font's characters itself). */
extern int feed_utf8;

/* Parses a whole ports.json held in memory (len bytes, need not end in 0). Answers NULL and an English reason in *why on failure. */
Feed *feed_parse(const char *text, long len, const char **why);
void  feed_free(Feed *feed);
/* One entry's text freed, and the entry cleared. */
void  port_free(Port *p);

/* A port can be installed when the feed gives a download for it, and its id,
 * drawer and program are plain names (no ':' or '/', not empty). */
int   port_installable(const Port *p);
int   port_plain_name(const char *s);

/* A picture's file name in the cache: 16 hex digits of its URL's SHA-256 and
 * the URL's extension ("3f2a...c1.png"), so two sources' "shot1.png" never
 * share a file, and a picture at a new URL is a new file. max >= 22. */
void  port_picture_name(const char *url, char *out, int max);

/* The Port Atlas's source: its ids stay as they are, as the installed list
 * from AmiPortAtlas 0.1 names games by them. Another source's ids become
 * "id@source", so the same name in two sources stays two entries. */
#define ATLAS_SOURCE "amicradle"

/* One list from several sources' feeds, in the order given: the ports are
 * moved into the new feed (each told its source) and the feeds given are
 * freed, NULL ones skipped. Its generated is the Atlas's, else the first's.
 * NULL on no memory (the feeds are freed even then). */
Feed *feed_merge(Feed **feeds, const char *const *source_ids, int n);

/* 1 when the archive is unpacked and its drawer copied, rather than its own
 * Installer script run. */
int   port_unpack_only(const Port *p);

/* Sorting the list. */
enum { SORT_NUMBER, SORT_NAME, SORT_YEAR, SORT_STATE, SORT_FIT, SORT_COUNT };
void  feed_sort(Port **list, int n, int key, int descending);

/* 1 for our own games rather than ports. */
int   port_is_original(const Port *p);

/* The state as the player reads it: "Released", "Testing" and so on. */
const char *port_state_text(const Port *p);

#endif
