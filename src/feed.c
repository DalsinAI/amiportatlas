/* AmiPortAtlas: reads ports.json.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * A small JSON reader made for this one file: objects, arrays, strings,
 * numbers, true, false and null. Unknown keys are skipped, so newer feeds
 * keep working with older copies of the program.
 */
#include <stdlib.h>
#include <string.h>
#include "feed.h"

typedef struct {
    const char *p, *end;
    const char *why;
} Reader;

static void ws(Reader *r)
{
    while (r->p < r->end && (*r->p == ' ' || *r->p == '\t' || *r->p == '\n' || *r->p == '\r'))
        r->p++;
}

static int take(Reader *r, char c)
{
    ws(r);
    if (r->p < r->end && *r->p == c) { r->p++; return 1; }
    return 0;
}

static int fail(Reader *r, const char *why)
{
    if (!r->why) r->why = why;
    return 0;
}

static int hex4(Reader *r, unsigned long *out)
{
    unsigned long v = 0;
    int i;
    if (r->end - r->p < 4) return fail(r, "a \\u escape is cut short");
    for (i = 0; i < 4; i++) {
        char c = *r->p++;
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned long)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned long)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned long)(c - 'A' + 10);
        else return fail(r, "a \\u escape has a character that is not hex");
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
static int string(Reader *r, char **out, int allow_null)
{
    const char *s;
    char *buf, *o;
    ws(r);
    if (allow_null && r->end - r->p >= 4 && memcmp(r->p, "null", 4) == 0) {
        r->p += 4;
        *out = NULL;
        return 1;
    }
    if (!take(r, '"')) return fail(r, "a string was expected");
    s = r->p;
    while (r->p < r->end && *r->p != '"') {
        if (*r->p == '\\') r->p++;
        r->p++;
    }
    if (r->p >= r->end) return fail(r, "a string is not closed");
    /* Decoded text is never longer than the source (an ellipsis, 3 bytes in
     * UTF-8, becomes 3 dots), so the source length is room enough. */
    buf = o = (char *)malloc((size_t)(r->p - s) + 1);
    if (!buf) return fail(r, "out of memory");
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

static int number(Reader *r, long *out)
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
    if (!any) return fail(r, "a number was expected");
    *out = neg ? -v : v;
    return 1;
}

static int boolean(Reader *r, int *out)
{
    ws(r);
    if (r->end - r->p >= 4 && memcmp(r->p, "true", 4) == 0) { r->p += 4; *out = 1; return 1; }
    if (r->end - r->p >= 5 && memcmp(r->p, "false", 5) == 0) { r->p += 5; *out = 0; return 1; }
    if (r->end - r->p >= 4 && memcmp(r->p, "null", 4) == 0) { r->p += 4; *out = 0; return 1; }
    return fail(r, "true or false was expected");
}

static int skip(Reader *r, int depth);

static int skip_container(Reader *r, char close, int depth)
{
    if (take(r, close)) return 1;
    for (;;) {
        if (close == '}') {
            char *k;
            if (!string(r, &k, 0)) return 0;
            free(k);
            if (!take(r, ':')) return fail(r, "a ':' was expected");
        }
        if (!skip(r, depth + 1)) return 0;
        if (take(r, ',')) continue;
        if (take(r, close)) return 1;
        return fail(r, "a ',' or the end of a list was expected");
    }
}

static int skip(Reader *r, int depth)
{
    char *s;
    long n;
    if (depth > 32) return fail(r, "the feed is nested too deeply");
    ws(r);
    if (r->p >= r->end) return fail(r, "the feed ends too soon");
    switch (*r->p) {
    case '{': r->p++; return skip_container(r, '}', depth);
    case '[': r->p++; return skip_container(r, ']', depth);
    case '"': if (!string(r, &s, 0)) return 0; free(s); return 1;
    case 't': case 'f': { int b; return boolean(r, &b); }
    case 'n': if (r->end - r->p >= 4 && memcmp(r->p, "null", 4) == 0) { r->p += 4; return 1; }
              return fail(r, "an unknown word in the feed");
    default: return number(r, &n);
    }
}

/* A list of strings, or null. */
static int strings(Reader *r, char ***out, int *count)
{
    int cap = 0;
    *out = NULL;
    *count = 0;
    ws(r);
    if (r->end - r->p >= 4 && memcmp(r->p, "null", 4) == 0) { r->p += 4; return 1; }
    if (!take(r, '[')) return fail(r, "a list was expected");
    if (take(r, ']')) return 1;
    for (;;) {
        char *s;
        if (!string(r, &s, 1)) return 0;
        if (s) {
            if (*count == cap) {
                char **n;
                cap = cap ? cap * 2 : 4;
                n = (char **)realloc(*out, (size_t)cap * sizeof(char *));
                if (!n) { free(s); return fail(r, "out of memory"); }
                *out = n;
            }
            (*out)[(*count)++] = s;
        }
        if (take(r, ',')) continue;
        if (take(r, ']')) return 1;
        return fail(r, "a ',' or ']' was expected in a list");
    }
}

static void free_list(char **l, int n)
{
    int i;
    for (i = 0; i < n; i++) free(l[i]);
    free(l);
}

#define KEY(name) (strcmp(key, name) == 0)

static int port(Reader *r, Port *p)
{
    memset(p, 0, sizeof *p);
    if (!take(r, '{')) return fail(r, "a port should be an object");
    if (take(r, '}')) return 1;
    for (;;) {
        char *key;
        int ok;
        if (!string(r, &key, 0)) return 0;
        if (!take(r, ':')) { free(key); return fail(r, "a ':' was expected"); }
        if      (KEY("number"))          ok = number(r, &p->number);
        else if (KEY("id"))              ok = string(r, &p->id, 1);
        else if (KEY("origin"))          ok = string(r, &p->origin, 1);
        else if (KEY("title"))           ok = string(r, &p->title, 1);
        else if (KEY("year"))            ok = string(r, &p->year, 1);
        else if (KEY("display"))         ok = string(r, &p->display, 1);
        else if (KEY("state"))           ok = string(r, &p->state, 1);
        else if (KEY("fit"))             ok = string(r, &p->fit, 1);
        else if (KEY("game_data"))       ok = string(r, &p->game_data, 1);
        else if (KEY("version"))         ok = string(r, &p->version, 1);
        else if (KEY("release_url"))     ok = string(r, &p->release_url, 1);
        else if (KEY("port_repo"))       ok = string(r, &p->port_repo, 1);
        else if (KEY("download_url"))    ok = string(r, &p->download_url, 1);
        else if (KEY("download_size"))   ok = number(r, &p->download_size);
        else if (KEY("download_sha256")) ok = string(r, &p->download_sha256, 1);
        else if (KEY("drawer"))          ok = string(r, &p->drawer, 1);
        else if (KEY("program"))         ok = string(r, &p->program, 1);
        else if (KEY("cpu"))             ok = string(r, &p->cpu, 1);
        else if (KEY("fpu"))             ok = boolean(r, &p->fpu);
        else if (KEY("space"))           ok = number(r, &p->space);
        else if (KEY("requires"))        ok = strings(r, &p->requires, &p->nrequires);
        else if (KEY("modules"))         ok = strings(r, &p->modules, &p->nmodules);
        else if (KEY("screenshots")) {
            char **l; int n;
            ok = strings(r, &l, &n);
            if (ok && n > 0) { free_list(p->shots, p->nshots); p->shots = l; p->nshots = n; }
            else if (ok) free_list(l, n);
        }
        else if (KEY("screenshot") && p->nshots == 0) {
            /* Older feeds: one picture. "screenshots", when present, wins. */
            char *s;
            ok = string(r, &s, 1);
            if (ok && s) {
                p->shots = (char **)malloc(sizeof(char *));
                if (p->shots) { p->shots[0] = s; p->nshots = 1; } else free(s);
            }
        }
        else ok = skip(r, 2);
        free(key);
        if (!ok) return 0;
        if (take(r, ',')) continue;
        if (take(r, '}')) return 1;
        return fail(r, "a ',' or '}' was expected in a port");
    }
}


static void port_free(Port *p)
{
    free(p->id); free(p->origin); free(p->title); free(p->year); free(p->display); free(p->state);
    free(p->fit); free(p->game_data); free(p->version); free(p->release_url);
    free(p->port_repo); free(p->download_url); free(p->download_sha256);
    free(p->drawer); free(p->program); free(p->cpu);
    free_list(p->requires, p->nrequires);
    free_list(p->modules, p->nmodules);
    free_list(p->shots, p->nshots);
}

void feed_free(Feed *f)
{
    int i;
    if (!f) return;
    for (i = 0; i < f->count; i++) port_free(&f->ports[i]);
    free(f->ports);
    free(f->generated);
    free(f);
}

static int ports(Reader *r, Feed *f)
{
    int cap = 0;
    if (!take(r, '[')) return fail(r, "\"ports\" should be a list");
    if (take(r, ']')) return 1;
    for (;;) {
        if (f->count == cap) {
            Port *n;
            cap = cap ? cap * 2 : 64;
            n = (Port *)realloc(f->ports, (size_t)cap * sizeof(Port));
            if (!n) return fail(r, "out of memory");
            f->ports = n;
        }
        if (!port(r, &f->ports[f->count])) {
            port_free(&f->ports[f->count]);
            return 0;
        }
        if (!f->ports[f->count].title) {
            port_free(&f->ports[f->count]);  /* a port with no name is left out */
        } else {
            f->count++;
        }
        if (take(r, ',')) continue;
        if (take(r, ']')) return 1;
        return fail(r, "a ',' or ']' was expected in the ports list");
    }
}

Feed *feed_parse(const char *text, long len, const char **why)
{
    Reader r;
    Feed *f = (Feed *)calloc(1, sizeof(Feed));
    int ok = 1;
    r.p = text;
    r.end = text + len;
    r.why = NULL;
    if (!f) { if (why) *why = "out of memory"; return NULL; }
    if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        r.p += 3;
    if (!take(&r, '{')) ok = fail(&r, "this is not a Port Atlas feed");
    else if (!take(&r, '}')) {
        for (;;) {
            char *key;
            if (!string(&r, &key, 0)) { ok = 0; break; }
            if (!take(&r, ':')) { free(key); ok = fail(&r, "a ':' was expected"); break; }
            if (strcmp(key, "schema_version") == 0) ok = number(&r, &f->schema);
            else if (strcmp(key, "generated") == 0) ok = string(&r, &f->generated, 1);
            else if (strcmp(key, "ports") == 0) ok = ports(&r, f);
            else ok = skip(&r, 1);
            free(key);
            if (!ok) break;
            if (take(&r, ',')) continue;
            if (take(&r, '}')) break;
            ok = fail(&r, "a ',' or '}' was expected");
            break;
        }
    }
    if (ok && f->schema != FEED_SCHEMA) ok = fail(&r, "the feed is a newer kind than this AmiPortAtlas reads: please update AmiPortAtlas");
    if (!ok) {
        if (why) *why = r.why ? r.why : "the feed could not be read";
        feed_free(f);
        return NULL;
    }
    return f;
}

/* A name made into a path under T: or the drawer picked: one plain name, never
 * "" (the parent itself, which Remove would delete) nor one with ':' or '/'. */
int port_plain_name(const char *s)
{
    if (!s || !*s || strlen(s) > 100) return 0;
    for (; *s; s++) if (*s == ':' || *s == '/' || *s == '"' || (unsigned char)*s < ' ') return 0;
    return 1;
}

int port_installable(const Port *p)
{
    return p->download_url && p->download_url[0] && p->download_sha256 && strlen(p->download_sha256) == 64
        && port_plain_name(p->id) && port_plain_name(p->drawer) && port_plain_name(p->program);
}

int port_is_original(const Port *p)
{
    return p->origin && strcmp(p->origin, "original") == 0;
}

const char *port_state_text(const Port *p)
{
    const char *s = p->state ? p->state : "";
    if (strcmp(s, "released") == 0) return "Released";
    if (strcmp(s, "testing") == 0) return "Testing";
    if (strcmp(s, "in_progress") == 0) return "In progress";
    return "Not started";
}

static int state_rank(const Port *p)
{
    const char *s = p->state ? p->state : "";
    if (strcmp(s, "released") == 0) return 0;
    if (strcmp(s, "testing") == 0) return 1;
    if (strcmp(s, "in_progress") == 0) return 2;
    return 3;
}

static int fit_rank(const Port *p)
{
    const char *s = p->fit ? p->fit : "";
    if (strcmp(s, "040") == 0) return 0;
    if (strcmp(s, "060") == 0) return 1;
    if (strcmp(s, "GPU") == 0) return 2;
    return 3;
}

static long year_of(const Port *p)
{
    const char *s = p->year;
    long y = 0;
    int i;
    if (!s) return 9999;
    for (i = 0; i < 4; i++) {
        if (s[i] < '0' || s[i] > '9') return 9999;
        y = y * 10 + (s[i] - '0');
    }
    return y;
}

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static int name_cmp(const char *a, const char *b)
{
    while (*a && lower((unsigned char)*a) == lower((unsigned char)*b)) { a++; b++; }
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}

static int sort_key, sort_desc;

static int cmp(const void *x, const void *y)
{
    const Port *a = *(const Port * const *)x, *b = *(const Port * const *)y;
    long d = 0;
    switch (sort_key) {
    case SORT_NAME:  d = 0; break;
    case SORT_YEAR:  d = year_of(a) - year_of(b); break;
    case SORT_STATE: d = state_rank(a) - state_rank(b); break;
    case SORT_FIT:   d = fit_rank(a) - fit_rank(b); break;
    default:         d = a->number - b->number; break;
    }
    if (d == 0 && sort_key != SORT_NUMBER) d = name_cmp(a->title, b->title);
    if (d == 0) d = a->number - b->number;
    if (sort_desc) d = -d;
    return d < 0 ? -1 : d > 0 ? 1 : 0;
}

void feed_sort(Port **list, int n, int key, int descending)
{
    sort_key = key;
    sort_desc = descending;
    qsort(list, (size_t)n, sizeof(Port *), cmp);
}
