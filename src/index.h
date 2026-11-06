/* AmiPortAtlas: what Atlas PUBLISH needs to make a source's index: an
 * Aminet-style .readme read, an entry made from it, the index written.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * Plain C with no Amiga calls, so the host tests try it.
 */
#ifndef AMIPORTATLAS_INDEX_H
#define AMIPORTATLAS_INDEX_H

#include "feed.h"

/* The fields of a readme as Aminet has them ("Short:", "Author:", "Type:",
 * "Version:", "Architecture:", "Requires:"), as UTF-8. Missing ones are "". */
typedef struct Readme {
    char shortdesc[128];
    char author[128];
    char type[48];       /* the Aminet directory: game/think, util/arc */
    char version[32];
    char arch[96];
    char requires[160];
} Readme;

/* Reads a readme (ISO-8859-1, as Amiga text is, or UTF-8). Answers 1 when it
 * had a Short: line. */
int  readme_parse(const char *text, long len, Readme *r);

/* A kind from an Aminet directory: game/ is "game", demo/ "demo", util/ and
 * disk/ "tool", dev/ "development", libs "library", else "application". */
const char *readme_kind(const char *type);

/* ISO-8859-1 made UTF-8 (text already UTF-8 is kept). out has max bytes. */
void latin1_to_utf8(const char *in, char *out, int max);

/* An id from an archive's name: "Fish-Fillets_1.2.lha" is "fish-fillets-1-2". */
void index_id(const char *archive, char *out, int max);

/* The entry with p's id put into f, replacing one there (which is freed),
 * else added at the end. p's fields are moved into f. Answers 0 on no memory. */
int  index_put(Feed *f, Port *p);

/* The whole feed written as an index (schema 1, UTF-8; strings as they are
 * held, so parse it with feed_utf8 set), with name as its top-level "name".
 * Answers malloc'd text and its length in *len, or NULL on no memory. */
char *feed_write(const Feed *f, const char *name, const char *generated, long *len);

#endif
