/* AmiPortAtlas: the sources it reads, as APT reads its sources.list.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * The master list of sources is data/sources.json in DalsinAI/amiportatlas,
 * read from the repository's main (SOURCES_URL). On top of it go the
 * player's own lines in ENVARC:AmiPortAtlas/Sources: a master source
 * switched on or off, or a source of their own added. Every source serves
 * one index in the form of the Port Atlas's ports.json. docs/SOURCES.md
 * has all of it. Plain C with no Amiga calls, so the host tests try it.
 */
#ifndef AMIPORTATLAS_SOURCES_H
#define AMIPORTATLAS_SOURCES_H

#define SOURCES_SCHEMA 1
#define SOURCES_URL "https://raw.githubusercontent.com/DalsinAI/amiportatlas/main/data/sources.json"
#define SOURCES_VAR "AmiPortAtlas/Sources"   /* the player's lines, in ENVARC: */

/* What became of a source when it was last read. */
enum { SRC_UNREAD, SRC_FRESH, SRC_CACHED, SRC_FAILED, SRC_NO_INDEX };

typedef struct Source {
    char *id;          /* letters, digits and '-', at most 24: the cache file's name and the "@id" of its entries */
    char *name;        /* as the player reads it */
    char *index;       /* the index's URL; NULL: listed, but no index yet */
    char *home;        /* the source's own web pages */
    char *note;        /* a line on it, for the Sources list */
    int   on;          /* read when the list is updated */
    int   checked;     /* the Team checks what this source offers */
    int   user;        /* added by the player, not in the master list */
    int   pictures;    /* its entries have screenshots */
    char **systems; int nsystems;  /* os3, gfos, aros-m68k, aros-x86, os4, morphos */
    /* filled in when the sources are read */
    int   state;       /* SRC_... */
    int   count;       /* entries read from it */
    char  why[120];    /* SRC_FAILED or SRC_CACHED: what went wrong */
} Source;

typedef struct Sources {
    long   schema;
    char  *generated;
    Source *list;
    int    count;
} Sources;

/* Reads a master list (data/sources.json). NULL and an English reason in
 * *why on failure; entries with a bad id are left out. */
Sources *sources_parse(const char *text, long len, const char **why);

/* The list used before any master list has been read: the Port Atlas alone. */
Sources *sources_builtin(void);

/* The player's lines put on top: "on ID", "off ID", or "on ID INDEX-URL
 * [NAME...]" for a source of their own (off works the same). Lines starting
 * ';' or '#' and blank ones are skipped. Answers how many lines were used. */
int  sources_apply(Sources *s, const char *user_text);

Source *sources_find(Sources *s, const char *id);
void    sources_free(Sources *s);

/* An id a source may have: 1 to 24 of a-z, 0-9 and '-'. */
int  source_id_ok(const char *id);

/* The player's lines changed: verb "on" or "off" switches id (an added
 * source keeps its index and name), "add" adds a source of their own (or
 * changes it) with index and name, "drop" takes id's line away. Answers new
 * malloc'd text (the old is the caller's), or NULL on a bad id or URL or no
 * memory. */
char *sources_edit(const char *user_text, const char *verb, const char *id,
                   const char *index, const char *name);

#endif
