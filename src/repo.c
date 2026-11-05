/* AmiPortAtlas: reading the sources and their indexes.
 * MIT, (c) 2026 The AmiCradle Team.
 */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/var.h>

#include "http.h"
#include "repo.h"

#define CACHE_VAR    "AmiPortAtlas/Cache"
#define CACHE_ELSE   "S:AmiPortAtlas"
#define SOURCES_URL_VAR "AmiPortAtlas/SourcesURL"   /* another master list, for labs */
#define USER_MAX     16384

static void say(char *why, int whylen, const char *text)
{
    if (whylen > 0) { strncpy(why, text, (size_t)whylen - 1); why[whylen - 1] = 0; }
}

void repo_cache(char *out, int max)
{
    BPTR l;
    if (GetVar((STRPTR)CACHE_VAR, (STRPTR)out, max, GVF_GLOBAL_ONLY) <= 0) snprintf(out, (size_t)max, CACHE_ELSE);
    l = Lock((STRPTR)out, ACCESS_READ);
    if (!l) l = CreateDir((STRPTR)out);
    if (l) UnLock(l);
}

static void cache_file(const char *name, char *out, int max)
{
    char dir[200];
    size_t n;
    repo_cache(dir, (int)sizeof dir);
    n = strlen(dir);
    snprintf(out, (size_t)max, "%s%s%s", dir, n && dir[n - 1] != ':' && dir[n - 1] != '/' ? "/" : "", name);
}

/* The Atlas's index keeps the name it had in AmiPortAtlas 0.1. */
static void index_file(const Source *x, char *out, int max)
{
    char name[40];
    if (strcmp(x->id, ATLAS_SOURCE) == 0) snprintf(name, sizeof name, "ports.json");
    else snprintf(name, sizeof name, "%s.json", x->id);
    cache_file(name, out, max);
}

static char *load(const char *path, long max, long *len)
{
    BPTR fh = Open((STRPTR)path, MODE_OLDFILE);
    char *buf;
    LONG n;
    if (!fh) return NULL;
    buf = (char *)malloc((size_t)max + 1);
    if (!buf) { Close(fh); return NULL; }
    n = Read(fh, buf, max);
    Close(fh);
    if (n <= 0) { free(buf); return NULL; }
    buf[n] = 0;
    *len = n;
    return buf;
}

static void save(const char *path, const char *text, long len)
{
    BPTR fh = Open((STRPTR)path, MODE_NEWFILE);
    if (!fh) return;
    if (Write(fh, (APTR)text, len) != len) { Close(fh); DeleteFile((STRPTR)path); return; }
    Close(fh);
}

char *repo_user_lines(void)
{
    char *buf = (char *)malloc(USER_MAX);
    LONG n;
    if (!buf) return NULL;
    n = GetVar((STRPTR)SOURCES_VAR, (STRPTR)buf, USER_MAX, GVF_GLOBAL_ONLY | GVF_BINARY_VAR);
    if (n < 0) n = 0;
    buf[n < USER_MAX ? n : USER_MAX - 1] = 0;
    return buf;
}

int repo_user_save(const char *text)
{
    return SetVar((STRPTR)SOURCES_VAR, (STRPTR)text, (LONG)strlen(text), GVF_GLOBAL_ONLY | GVF_SAVE_VAR) != 0;
}

static void tell_line(RepoTell tell, void *user, const char *fmt, const char *name)
{
    char line[160];
    if (!tell) return;
    snprintf(line, sizeof line, fmt, name);
    tell(user, line);
}

/* The master list: from the network, else the saved copy, else the Atlas alone. */
static Sources *master(int fetch, RepoTell tell, void *user)
{
    char url[420], path[240], why[200];
    char *text = NULL;
    long len = 0;
    const char *bad;
    Sources *s = NULL;
    cache_file("sources.json", path, (int)sizeof path);
    if (!fetch) {
        text = load(path, REPO_SOURCES_MAX, &len);
        if (text) { s = sources_parse(text, len, &bad); free(text); text = NULL; }
        if (s) return s;
    }
    if (GetVar((STRPTR)SOURCES_URL_VAR, (STRPTR)url, sizeof url, GVF_GLOBAL_ONLY) <= 0) snprintf(url, sizeof url, "%s", SOURCES_URL);
    tell_line(tell, user, "Reading %s...", "the list of sources");
    if (http_get_mem(url, REPO_SOURCES_MAX, &text, &len, why, (int)sizeof why)) {
        s = sources_parse(text, len, &bad);
        if (s) save(path, text, len);
        free(text);
    }
    if (!s) {
        text = load(path, REPO_SOURCES_MAX, &len);
        if (text) { s = sources_parse(text, len, &bad); free(text); }
    }
    return s ? s : sources_builtin();
}

/* One source's index into *f: fresh, or saved, or neither. */
static void one(Source *x, int fetch, RepoTell tell, void *user, Feed **f)
{
    char path[240], why[200];
    char *text = NULL;
    long len = 0;
    const char *bad = NULL;
    *f = NULL;
    index_file(x, path, (int)sizeof path);
    if (!fetch) {
        text = load(path, REPO_INDEX_MAX, &len);
        if (text) {
            *f = feed_parse(text, len, &bad);
            free(text);
            if (*f) { x->state = SRC_CACHED; x->why[0] = 0; return; }
        }
    }
    tell_line(tell, user, "Reading %s...", x->name);
    if (http_get_mem(x->index, REPO_INDEX_MAX, &text, &len, why, (int)sizeof why)) {
        *f = feed_parse(text, len, &bad);
        if (*f) save(path, text, len);
        else snprintf(why, sizeof why, "its index could not be read: %s", bad);
        free(text);
        if (*f) { x->state = SRC_FRESH; return; }
    }
    text = load(path, REPO_INDEX_MAX, &len);
    if (text) {
        *f = feed_parse(text, len, &bad);
        free(text);
    }
    x->state = *f ? SRC_CACHED : SRC_FAILED;
    snprintf(x->why, sizeof x->why, "%s", why);
}

Feed *repo_read(int fetch, Sources **out, RepoTell tell, void *user, char *why, int whylen)
{
    Sources *s;
    Feed **feeds, *m;
    const char **ids;
    char *lines;
    int i, any = 0;
    *out = NULL;
    s = master(fetch, tell, user);
    if (!s) { say(why, whylen, "Out of memory."); return NULL; }
    lines = repo_user_lines();
    if (lines) { sources_apply(s, lines); free(lines); }
    feeds = (Feed **)calloc((size_t)s->count, sizeof(Feed *));
    ids = (const char **)calloc((size_t)s->count, sizeof(char *));
    if (!feeds || !ids) { free(feeds); free(ids); sources_free(s); say(why, whylen, "Out of memory."); return NULL; }
    for (i = 0; i < s->count; i++) {
        Source *x = &s->list[i];
        ids[i] = x->id;
        x->count = 0;
        if (!x->on) continue;
        if (!x->index) { x->state = SRC_NO_INDEX; continue; }
        one(x, fetch, tell, user, &feeds[i]);
        if (feeds[i]) { x->count = feeds[i]->count; any = 1; }
    }
    m = any ? feed_merge(feeds, ids, s->count) : NULL;
    if (!any) {
        Source *a = sources_find(s, ATLAS_SOURCE);
        snprintf(why, (size_t)whylen, "No source could be read%s%s",
                 a && a->why[0] ? ": " : ".", a && a->why[0] ? a->why : "");
    } else if (!m) say(why, whylen, "There isn't the memory for every source's entries.");
    free(feeds);
    free(ids);
    *out = s;
    return m;
}

void repo_summary(const Sources *s, char *out, int max)
{
    int i, n = 0, bad = 0;
    out[0] = 0;
    for (i = 0; i < s->count && n < max - 1; i++) {
        const Source *x = &s->list[i];
        if (!x->on || x->state == SRC_NO_INDEX || x->state == SRC_FAILED) continue;
        n += snprintf(out + n, (size_t)(max - n), "%s%s %d%s", n ? ", " : "", x->name, x->count,
                      x->state == SRC_CACHED ? " (saved copy)" : "");
    }
    for (i = 0; i < s->count && n < max - 1; i++) {
        const Source *x = &s->list[i];
        if (!x->on || x->state != SRC_FAILED) continue;
        n += snprintf(out + n, (size_t)(max - n), "%s%s: not read", bad++ ? ", " : (n ? "; " : ""), x->name);
    }
    if (n >= max) out[max - 1] = 0;
}

const char *repo_source_name(const Sources *s, const char *id)
{
    int i;
    if (!s || !id) return "";
    for (i = 0; i < s->count; i++) if (strcmp(s->list[i].id, id) == 0) return s->list[i].name;
    return id;
}
