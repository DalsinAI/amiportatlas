/* AmiPortAtlas: readmes read and indexes written, for Atlas PUBLISH.
 * MIT, (c) 2026 The AmiCradle Team.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "index.h"

/* ---- readmes ---- */

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static int key_is(const char *line, const char *key, const char **value)
{
    while (*key) if (lower((unsigned char)*line++) != lower((unsigned char)*key++)) return 0;
    while (*line == ' ' || *line == '\t') line++;
    *value = line;
    return 1;
}

static int is_utf8(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    int any = 0;
    while (*p) {
        if (*p < 0x80) { p++; continue; }
        if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { p += 2; any = 1; continue; }
        if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) { p += 3; any = 1; continue; }
        if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) { p += 4; any = 1; continue; }
        return 0;
    }
    return any;
}

void latin1_to_utf8(const char *in, char *out, int max)
{
    int n = 0;
    if (max <= 0) return;
    if (is_utf8(in)) { snprintf(out, (size_t)max, "%s", in); return; }
    for (; *in; in++) {
        unsigned char c = (unsigned char)*in;
        if (c < 0x80) { if (n + 1 >= max) break; out[n++] = (char)c; }
        else { if (n + 2 >= max) break; out[n++] = (char)(0xC0 | (c >> 6)); out[n++] = (char)(0x80 | (c & 0x3F)); }
    }
    out[n] = 0;
}

static void field(char *out, int max, const char *start, const char *end)
{
    char tmp[400];
    int n = (int)(end - start);
    while (n > 0 && (start[n - 1] == ' ' || start[n - 1] == '\t' || start[n - 1] == '\r')) n--;
    if (n >= (int)sizeof tmp) n = (int)sizeof tmp - 1;
    memcpy(tmp, start, (size_t)n);
    tmp[n] = 0;
    latin1_to_utf8(tmp, out, max);
}

int readme_parse(const char *text, long len, Readme *r)
{
    const char *p = text, *end = text + len;
    memset(r, 0, sizeof *r);
    while (p < end) {
        const char *eol = p, *v;
        while (eol < end && *eol != '\n') eol++;
        if (eol == p || *p == '\r') break;     /* the fields end at the first blank line */
        if      (key_is(p, "Short:", &v))        field(r->shortdesc, sizeof r->shortdesc, v, eol);
        else if (key_is(p, "Author:", &v))       field(r->author, sizeof r->author, v, eol);
        else if (key_is(p, "Type:", &v))         field(r->type, sizeof r->type, v, eol);
        else if (key_is(p, "Version:", &v))      field(r->version, sizeof r->version, v, eol);
        else if (key_is(p, "Architecture:", &v)) field(r->arch, sizeof r->arch, v, eol);
        else if (key_is(p, "Requires:", &v))     field(r->requires, sizeof r->requires, v, eol);
        p = eol + 1;
    }
    return r->shortdesc[0] != 0;
}

const char *readme_kind(const char *type)
{
    if (!type) return "application";
    if (strstr(type, "libs")) return "library";
    if (strncmp(type, "game", 4) == 0) return "game";
    if (strncmp(type, "demo", 4) == 0) return "demo";
    if (strncmp(type, "util", 4) == 0 || strncmp(type, "disk", 4) == 0) return "tool";
    if (strncmp(type, "dev", 3) == 0) return "development";
    return "application";
}

void index_id(const char *archive, char *out, int max)
{
    const char *base = archive, *dot;
    int n = 0, dash = 0;
    const char *s;
    for (s = archive; *s; s++) if (*s == ':' || *s == '/') base = s + 1;
    dot = strrchr(base, '.');
    if (!dot || (lower((unsigned char)dot[1]) != 'l' && lower((unsigned char)dot[1]) != 'z')) dot = base + strlen(base);
    for (s = base; s < dot && n < max - 1; s++) {
        int c = lower((unsigned char)*s);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) { out[n++] = (char)c; dash = 0; }
        else if (n && !dash) { out[n++] = '-'; dash = 1; }
    }
    while (n && out[n - 1] == '-') n--;
    out[n] = 0;
}

/* ---- putting an entry in ---- */

int index_put(Feed *f, Port *p)
{
    int i;
    Port *n;
    for (i = 0; i < f->count; i++)
        if (f->ports[i].id && p->id && strcmp(f->ports[i].id, p->id) == 0) {
            if (!p->number) p->number = f->ports[i].number;   /* its place stays */
            port_free(&f->ports[i]);
            f->ports[i] = *p;
            memset(p, 0, sizeof *p);
            return 1;
        }
    n = (Port *)realloc(f->ports, (size_t)(f->count + 1) * sizeof(Port));
    if (!n) return 0;
    f->ports = n;
    f->ports[f->count++] = *p;
    memset(p, 0, sizeof *p);
    return 1;
}

/* ---- writing ---- */

typedef struct { char *b; long len, cap; int bad; } Buf;

static void put(Buf *o, const char *s, long n)
{
    if (o->bad) return;
    if (o->len + n + 1 > o->cap) {
        long cap = o->cap ? o->cap : 16384;
        char *nb;
        while (cap < o->len + n + 1) cap *= 2;
        nb = (char *)realloc(o->b, (size_t)cap);
        if (!nb) { o->bad = 1; return; }
        o->b = nb;
        o->cap = cap;
    }
    memcpy(o->b + o->len, s, (size_t)n);
    o->len += n;
    o->b[o->len] = 0;
}

static void puts_(Buf *o, const char *s) { put(o, s, (long)strlen(s)); }

static void str(Buf *o, const char *s)
{
    char e[8];
    if (!s) { puts_(o, "null"); return; }
    puts_(o, "\"");
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { e[0] = '\\'; e[1] = (char)c; put(o, e, 2); }
        else if (c == '\n') puts_(o, "\\n");
        else if (c == '\t') puts_(o, "\\t");
        else if (c < 0x20) { snprintf(e, sizeof e, "\\u%04x", c); puts_(o, e); }
        else put(o, (const char *)&c, 1);
    }
    puts_(o, "\"");
}

static void key(Buf *o, const char *k, int *first)
{
    puts_(o, *first ? "\n  {" : ", ");
    *first = 0;
    str(o, k);
    puts_(o, ": ");
}

static void kstr(Buf *o, const char *k, const char *v, int *first)
{
    if (!v) return;
    key(o, k, first);
    str(o, v);
}

static void knum(Buf *o, const char *k, long v, int *first)
{
    char n[24];
    if (!v) return;
    key(o, k, first);
    snprintf(n, sizeof n, "%ld", v);
    puts_(o, n);
}

static void klist(Buf *o, const char *k, char **l, int n, int *first)
{
    int i;
    if (!n) return;
    key(o, k, first);
    puts_(o, "[");
    for (i = 0; i < n; i++) { if (i) puts_(o, ", "); str(o, l[i]); }
    puts_(o, "]");
}

char *feed_write(const Feed *f, const char *name, const char *generated, long *len)
{
    Buf o;
    int i;
    memset(&o, 0, sizeof o);
    puts_(&o, "{\"schema_version\": 1, \"name\": ");
    str(&o, name);
    puts_(&o, ", \"generated\": ");
    str(&o, generated ? generated : f->generated);
    puts_(&o, ",\n \"ports\": [");
    for (i = 0; i < f->count; i++) {
        const Port *p = &f->ports[i];
        int first = 1;
        if (i) puts_(&o, ",");
        kstr(&o, "id", p->id, &first);
        if (first) { key(&o, "id", &first); puts_(&o, "null"); }
        knum(&o, "number", p->number, &first);
        kstr(&o, "title", p->title, &first);
        kstr(&o, "kind", p->kind, &first);
        kstr(&o, "short", p->summary, &first);
        kstr(&o, "author", p->author, &first);
        kstr(&o, "category", p->category, &first);
        kstr(&o, "origin", p->origin, &first);
        kstr(&o, "year", p->year, &first);
        kstr(&o, "display", p->display, &first);
        kstr(&o, "state", p->state, &first);
        kstr(&o, "fit", p->fit, &first);
        kstr(&o, "game_data", p->game_data, &first);
        klist(&o, "requires", p->requires, p->nrequires, &first);
        kstr(&o, "version", p->version, &first);
        kstr(&o, "release_url", p->release_url, &first);
        kstr(&o, "port_repo", p->port_repo, &first);
        klist(&o, "screenshots", p->shots, p->nshots, &first);
        kstr(&o, "download_url", p->download_url, &first);
        knum(&o, "download_size", p->download_size, &first);
        kstr(&o, "download_sha256", p->download_sha256, &first);
        kstr(&o, "install", p->install, &first);
        kstr(&o, "drawer", p->drawer, &first);
        kstr(&o, "program", p->program, &first);
        kstr(&o, "cpu", p->cpu, &first);
        if (p->fpu) { key(&o, "fpu", &first); puts_(&o, "true"); }
        knum(&o, "space", p->space, &first);
        klist(&o, "modules", p->modules, p->nmodules, &first);
        puts_(&o, "}");
    }
    puts_(&o, "\n ]}\n");
    if (o.bad) { free(o.b); return NULL; }
    *len = o.len;
    return o.b;
}
