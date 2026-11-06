/* AmiPortAtlas: the small JSON reader every feed uses.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * Each call reads one value where the reader is and moves past it; 0 means
 * it failed, with an English reason kept in r->why (the first one wins).
 */
#ifndef AMIPORTATLAS_JSON_H
#define AMIPORTATLAS_JSON_H

typedef struct {
    const char *p, *end;
    const char *why;
} JsonReader;

/* Starts reading text (len bytes, need not end in 0), past a UTF-8 mark. */
void json_begin(JsonReader *r, const char *text, long len);

int  json_take(JsonReader *r, char c);            /* 1 and past c when c is next */
int  json_fail(JsonReader *r, const char *why);   /* always 0 */
int  json_string(JsonReader *r, char **out, int allow_null);  /* *out malloc'd */
int  json_number(JsonReader *r, long *out);       /* whole numbers; null is 0 */
int  json_boolean(JsonReader *r, int *out);       /* null is 0 */
int  json_skip(JsonReader *r, int depth);         /* any value */
int  json_strings(JsonReader *r, char ***out, int *count);    /* a list of strings, or null */
void json_free_list(char **l, int n);

/* Text comes out as ISO-8859-1 for Amiga bitmap fonts, or with this set as
 * UTF-8 (declared again in feed.h, where the program sets it). */
extern int feed_utf8;

#endif
