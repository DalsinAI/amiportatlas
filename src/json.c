/* AmiPortAtlas: the small JSON reader every feed uses (ports.json, a
 * source's index, the master list of sources).
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * Objects, arrays, strings, numbers, true, false and null; plain C with no
 * Amiga calls, so it builds for every target and for the host tests.
 */
#include <stdlib.h>
#include <string.h>
#include "json.h"

static void ws(JsonReader *r)
{
    while (r->p < r->end && (*r->p == ' ' || *r->p == '\t' || *r->p == '\n' || *r->p == '\r'))
        r->p++;
}

int json_take(JsonReader *r, char c)
{
    ws(r);
    if (r->p < r->end && *r->p == c) { r->p++; return 1; }
    return 0;
}

int json_fail(JsonReader *r, const char *why)
{
    if (!r->why) r->why = why;
    return 0;
}

static int hex4(JsonReader *r, unsigned long *out)
{
    unsigned long v = 0;
    int i;
    if (r->end - r->p < 4) return json_fail(r, "a \\u escape is cut short");
    for (i = 0; i < 4; i++) {
        char c = *r->p++;
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned long)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned long)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned long)(c - 'A' + 10);
        else return json_fail(r, "a \\u escape has a character that is not hex");
    }
    *out = v;
    return 1;
}

int feed_utf8;

/* One code point into UTF-8, or into ISO-8859-1. */
static void put(char **o, unsigned long cp)
{
    if (feed_utf8) {
        if (cp < 0x80) *(*o)++ = (char)cp;
        else if (cp < 0x800) { *(*o)++ = (char)(0xC0 | (cp >> 6)); *(*o)++ = (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { *(*o)++ = (char)(0xE0 | (cp >> 12)); *(*o)++ = (char)(0x80 | ((cp >> 6) & 0x3F));
                                 *(*o)++ = (char)(0x80 | (cp & 0x3F)); }
        else { *(*o)++ = (char)(0xF0 | (cp >> 18)); *(*o)++ = (char)(0x80 | ((cp >> 12) & 0x3F));
               *(*o)++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *(*o)++ = (char)(0x80 | (cp & 0x3F)); }
        return;
    }
    if (cp == 0x2013 || cp == 0x2014) cp = '-';
    else if (cp == 0x2018 || cp == 0x2019) cp = '\'';
    else if (cp == 0x201C || cp == 0x201D) cp = '"';
    else if (cp == 0x2026) { *(*o)++ = '.'; *(*o)++ = '.'; cp = '.'; }
    else if (cp > 0xFF) cp = '?';
    *(*o)++ = (char)cp;
}

/* A string, decoded. *out is malloc'd (NULL for JSON null when allow_null). */
int json_string(JsonReader *r, char **out, int allow_null)
{
    const char *s;
    char *buf, *o;
    ws(r);
    if (allow_null && r->end - r->p >= 4 && memcmp(r->p, "null", 4) == 0) {
        r->p += 4;
        *out = NULL;
        return 1;
    }
    if (!json_take(r, '"')) return json_fail(r, "a string was expected");
    s = r->p;
    while (r->p < r->end && *r->p != '"') {
        if (*r->p == '\\') r->p++;
        r->p++;
    }
    if (r->p >= r->end) return json_fail(r, "a string is not closed");
    /* Decoded text is never longer than the source (an ellipsis, 3 bytes in
     * UTF-8, becomes 3 dots), so the source length is room enough. */
    buf = o = (char *)malloc((size_t)(r->p - s) + 1);
    if (!buf) return json_fail(r, "out of memory");
    r->p = s;
    while (*r->p != '"') {
        unsigned char c = (unsigned char)*r->p++;
        if (c == '\\') {
            char e = *r->p++;
            unsigned long cp;
            switch (e) {
            case 'n': *o++ = '\n'; break;
            case 't': *o++ = '\t'; break;
            case 'r': break;
            case 'b': case 'f': break;
            case 'u':
                if (!hex4(r, &cp)) { free(buf); return 0; }
                if (cp >= 0xD800 && cp <= 0xDBFF && r->end - r->p >= 6 && r->p[0] == '\\' && r->p[1] == 'u') {
                    unsigned long lo;
                    r->p += 2;
                    if (!hex4(r, &lo)) { free(buf); return 0; }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                put(&o, cp);
                break;
            default: *o++ = e; break;  /* \" \\ \/ */
            }
        } else if (c < 0x80 || feed_utf8) {
            *o++ = (char)c;  /* UTF-8 is kept as it came */
        } else {
            /* UTF-8 */
            unsigned long cp;
            int more;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; more = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; more = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; more = 3; }
            else { cp = '?'; more = 0; }
            while (more-- > 0 && *r->p != '"' && ((unsigned char)*r->p & 0xC0) == 0x80)
                cp = (cp << 6) | ((unsigned char)*r->p++ & 0x3F);
            put(&o, cp);
        }
    }
    r->p++;
    *o = 0;
    *out = buf;
    return 1;
}

int json_number(JsonReader *r, long *out)
{
    long v = 0;
    int neg = 0, any = 0;
    ws(r);
    if (r->end - r->p >= 4 && memcmp(r->p, "null", 4) == 0) { r->p += 4; *out = 0; return 1; }
    if (r->p < r->end && *r->p == '-') { neg = 1; r->p++; }
    while (r->p < r->end && *r->p >= '0' && *r->p <= '9') { v = v * 10 + (*r->p++ - '0'); any = 1; }
    /* A fraction or exponent is read past; the feed has whole numbers. */
    while (r->p < r->end && (*r->p == '.' || *r->p == 'e' || *r->p == 'E' || *r->p == '+' || *r->p == '-' || (*r->p >= '0' && *r->p <= '9')))
        r->p++;
    if (!any) return json_fail(r, "a number was expected");
    *out = neg ? -v : v;
    return 1;
}

int json_boolean(JsonReader *r, int *out)
{
    ws(r);
    if (r->end - r->p >= 4 && memcmp(r->p, "true", 4) == 0) { r->p += 4; *out = 1; return 1; }
    if (r->end - r->p >= 5 && memcmp(r->p, "false", 5) == 0) { r->p += 5; *out = 0; return 1; }
    if (r->end - r->p >= 4 && memcmp(r->p, "null", 4) == 0) { r->p += 4; *out = 0; return 1; }
    return json_fail(r, "true or false was expected");
}


static int skip_container(JsonReader *r, char close, int depth)
{
    if (json_take(r, close)) return 1;
    for (;;) {
        if (close == '}') {
            char *k;
            if (!json_string(r, &k, 0)) return 0;
            free(k);
            if (!json_take(r, ':')) return json_fail(r, "a ':' was expected");
        }
        if (!json_skip(r, depth + 1)) return 0;
        if (json_take(r, ',')) continue;
        if (json_take(r, close)) return 1;
        return json_fail(r, "a ',' or the end of a list was expected");
    }
}

int json_skip(JsonReader *r, int depth)
{
    char *s;
    long n;
    if (depth > 32) return json_fail(r, "the feed is nested too deeply");
    ws(r);
    if (r->p >= r->end) return json_fail(r, "the feed ends too soon");
    switch (*r->p) {
    case '{': r->p++; return skip_container(r, '}', depth);
    case '[': r->p++; return skip_container(r, ']', depth);
    case '"': if (!json_string(r, &s, 0)) return 0; free(s); return 1;
    case 't': case 'f': { int b; return json_boolean(r, &b); }
    case 'n': if (r->end - r->p >= 4 && memcmp(r->p, "null", 4) == 0) { r->p += 4; return 1; }
              return json_fail(r, "an unknown word in the feed");
    default: return json_number(r, &n);
    }
}

/* A list of strings, or null. */
int json_strings(JsonReader *r, char ***out, int *count)
{
    int cap = 0;
    *out = NULL;
    *count = 0;
    ws(r);
    if (r->end - r->p >= 4 && memcmp(r->p, "null", 4) == 0) { r->p += 4; return 1; }
    if (!json_take(r, '[')) return json_fail(r, "a list was expected");
    if (json_take(r, ']')) return 1;
    for (;;) {
        char *s;
        if (!json_string(r, &s, 1)) return 0;
        if (s) {
            if (*count == cap) {
                char **n;
                cap = cap ? cap * 2 : 4;
                n = (char **)realloc(*out, (size_t)cap * sizeof(char *));
                if (!n) { free(s); return json_fail(r, "out of memory"); }
                *out = n;
            }
            (*out)[(*count)++] = s;
        }
        if (json_take(r, ',')) continue;
        if (json_take(r, ']')) return 1;
        return json_fail(r, "a ',' or ']' was expected in a list");
    }
}

void json_free_list(char **l, int n)
{
    int i;
    for (i = 0; i < n; i++) free(l[i]);
    free(l);
}

void json_begin(JsonReader *r, const char *text, long len)
{
    r->p = text;
    r->end = text + len;
    r->why = NULL;
    if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        r->p += 3;
}
